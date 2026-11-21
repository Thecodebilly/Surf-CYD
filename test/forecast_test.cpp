// Forecast fetch check against canned Open-Meteo / NOAA / NWS replies. Run from the repo root
// (ArduinoJson v6 checked out at /opt/ArduinoJson, as in the Kiln-CYD Dockerfile):
//   docker run --rm -e TZ=America/New_York -v "$PWD:/src" -w /src emscripten/emsdk:3.1.74 bash -c \
//     "git clone -q --depth 1 --branch v6.21.5 https://github.com/bblanchon/ArduinoJson.git /opt/ArduinoJson &&
//      em++ -std=gnu++17 -sASYNCIFY -Iemulator/shims -Iinclude -I/opt/ArduinoJson/src test/forecast_test.cpp
//      src/Network.cpp src/Storage.cpp src/Config.cpp src/Theme.cpp emulator/shims/shim_impl.cpp -o /tmp/t.js &&
//      node /tmp/t.js"
// TZ is set so that local time and UTC fall on different days, as in a browser running the emulator.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <emscripten.h>
#include "Network.h"
#include "Storage.h"

void showStatus(const String &, const String &, uint16_t) {}

static bool near(float a, float b) { return fabsf(a - b) < 0.001f; }

// 2026-11-21 04:13 UTC, which is still 2026-11-20 in New York. Fake fetch: NOAA answers
// 2.0 + hour/10 ft for the UTC date and 7.0 + hour/10 for any other date.
EM_JS(void, installFakes, (), {
    Date.now = () => Date.UTC(2026, 10, 21, 4, 13);
    const store = new Map();
    globalThis.localStorage = {
      getItem: (k) => (store.has(k) ? store.get(k) : null),
      setItem: (k, v) => store.set(k, String(v)),
      removeItem: (k) => store.delete(k),
    };
    globalThis.down = {};
    const json = (body) => ({ status: 200, text: async () => JSON.stringify(body) });
    const hours = [...Array(24).keys()];
    globalThis.fetch = async (url) => {
      if (url.includes('marine-api')) {
        return json({ hourly: {
          time: hours.map((h) => '2026-11-21T' + String(h).padStart(2, '0') + ':00'),
          wave_height: hours.map((h) => h / 10), wave_period: hours.map(() => 9), wave_direction: hours.map(() => 90) } });
      }
      if (url.includes('tidesandcurrents')) {
        const query = new URL(url).searchParams;
        const station = query.get("station");
        const date = query.get("begin_date");
        if (globalThis.down.noaa || globalThis.down.station === station) return { status: 500, text: async () => '' };
        const d = date.slice(0, 4) + '-' + date.slice(4, 6) + '-' + date.slice(6);
        const base = date === '20261121' ? 2 : 7;
        return json({ predictions: hours.map((h) => ({ t: d + ' ' + String(h).padStart(2, '0') + ':00', v: (base + h / 10).toFixed(3) })) });
      }
      if (url.includes('api.weather.gov')) {
        if (globalThis.down.nws) return { status: 500, text: async () => '' };
        if (url.includes('/points/')) return json({ properties: { forecast: 'https://api.weather.gov/gridpoints/X/1,1/forecast' } });
        return json({ properties: { periods: [{ windSpeed: '10 to 20 mph', windDirection: 'E' }] } });
      }
      throw new Error('unexpected fetch ' + url);
    };
});

// Which upstreams fail: "noaa", "nws", or a station id.
EM_JS(void, setDown, (const char *what), {
  const w = UTF8ToString(what);
  globalThis.down = w === "noaa" ? { noaa: true, nws: true } : w ? { station: w } : {};
});

int main() {
  installFakes();
  const float FT = 0.3048f;

  // Tide for the current UTC hour of the UTC day, whatever the local time zone says.
  SurfForecast f = fetchSurfForecast(30.3268f, -81.3836f);  // Jax Beach: one tide station in range
  assert(f.valid && near(f.waveHeight, 0.4f));
  assert(near(f.tideHeight, 2.4f * FT) && near(f.minTide, 2.0f * FT) && near(f.maxTide, 4.3f * FT));
  assert(f.hasWind && near(f.windSpeed, 15.0f) && near(f.windDirection, 90.0f));

  // NOAA down with today's bounds cached: no tide data, not a 0.0 ft tide inside cached bounds.
  setDown("noaa");
  f = fetchSurfForecast(30.3268f, -81.3836f);
  assert(f.valid && f.tideHeight == 0.0f && f.minTide == 0.0f && f.maxTide == 0.0f);
  // No NWS wind: flagged so the screen shows dashes rather than 0.0 mph.
  assert(!f.hasWind);

  // Primary station down, secondaries up: the secondary's height, not a blend with the failed 0.0.
  deleteTideBounds();
  clearTideStationCache();
  String primary = findNearestTideStation(41.54f, -70.64f);  // Woods Hole area: three stations in range
  setDown(primary.c_str());
  f = fetchSurfForecast(41.54f, -70.64f);
  assert(near(f.tideHeight, 2.4f * FT) && near(f.minTide, 2.0f * FT) && near(f.maxTide, 4.3f * FT));

  printf("forecast_test: ok\n");
  return 0;
}
