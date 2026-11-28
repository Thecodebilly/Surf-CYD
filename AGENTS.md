# AGENTS.md

Surf conditions display for an ESP32 "Cheap Yellow Display" with a touchscreen. It shows wave height, period, swell and wind direction and tide for a saved surf spot, refreshes every 15 minutes, and has a small surfing game with an online leaderboard. The same C++ also compiles to WebAssembly for a browser emulator.

## Stack

- Firmware: C++ (Arduino framework) on PlatformIO, `espressif32 @ 6.4.0`, board `esp32dev`, env `esp32_35_st7796`, `huge_app.csv` partitions.
- Libraries: ArduinoJson 6, GFX Library for Arduino 1.5.8 (ST7796 320x480 IPS, rotation 1), XPT2046_Touchscreen. Settings live in SPIFFS as small JSON files.
- Emulator: Emscripten (`emcc`, ASYNCIFY) builds the `src/*.cpp` files listed in `SRCS` in `emulator/Makefile` against the header shims in `emulator/shims/`. Verified with the `emscripten/emsdk:3.1.74` Docker image; `emulator/setup.sh` installs the latest emsdk instead.
- `server.js` (root): Express 4 static server for the emulator page.
- `api/`: Express + `pg` high-score API backed by PostgreSQL, a separate Node app with its own `package.json`.

## Where it runs

- Hardware: ESP32 CYD with a 3.5" ST7796 panel and XPT2046 touch. Pins and touch calibration are in `include/Config.h`.
- Web emulator: `emulator/.gitignore` says `surfcyd.js` and `surfcyd.wasm` are committed for Railway deployment. There is no hosting config in this repo, but GitHub's deployment records show Railway deploying each push to `main` to production, so a merge to `main` goes live.
- `server.js` serves the files in `emulator/` as static files at `/` and `/SurfBoard` (a comment says the main site proxies `/SurfBoard`), so anything put in `emulator/` is public. `emulator/index.html` is the public Surf Board landing page with the emulator canvas in it. Keep notes, docs (including any nested AGENTS.md) and anything private out of `emulator/`.
- High-score API: the firmware and emulator call the deployed API at `API_BASE` in `src/Database.cpp` and use only `GET /records` and `POST /records`. `api/README.md` describes deploying `api/` to Railway. The deployed service may not run this code: `src/Database.cpp` says `GET /records` returns every score ever posted, while `api/server.js` returns 10.
- Outside services the device calls: Open-Meteo geocoding and marine (`include/Config.h`), NOAA tides and NWS wind (`src/Network.cpp`), NTP (`src/main.cpp`).

## Repo map

- `platformio.ini`: the only firmware build env.
- `src/main.cpp`: `setup()` (first-boot welcome, name entry, Wi-Fi, NTP, location, wave threshold) and `loop()` (fetch, draw, touch dispatch, 15 min refresh).
- `src/Network.cpp`: Wi-Fi connect, Open-Meteo geocode and marine forecast, NOAA tide station lookup and tide heights, NWS wind.
- `src/Display.cpp`: drawing helpers (`drawButton`, `showStatus`, `drawScreenHeader`) and the forecast, settings, files viewer and welcome screens. The game and leaderboard screens draw in `src/Game.cpp`, the keyboard and setup screens in `src/TouchUI.cpp`.
- `src/TouchUI.cpp`: touch mapping, on-screen keyboard, Wi-Fi/location/wave setup flows, settings actions.
- `src/Storage.cpp`: SPIFFS save/load/delete for the settings files (tide direction has save/delete only, player name save/load only), plus `logInfo`/`logError`.
- `src/Game.cpp`: the surf game, local high score (`/high_score.json`), leaderboard screen.
- `src/Database.cpp`, `include/RecordParser.h`: leaderboard client; streams `GET /records` and posts `POST /records`.
- `src/Theme.cpp`: dark and light themes in RGB565.
- `src/Config.cpp`: default spots and SPIFFS file paths.
- `include/`: one header per module, plus `Types.h` for shared structs.
- `test/forecast_test.cpp`: forecast and tide fetch check against canned replies, run under Emscripten and Node.
- `emulator/`: `Makefile`, `setup.sh` (installs emsdk to `~/emsdk`), `main_emulator.cpp`, `shims/`, `canvas_bridge.js`, `index.html`, and the committed build output.
- `api/server.js`: routes `/highscore` (GET, POST), `/records` (GET, POST), `/leaderboard`, `/health`. `api/test_api.sh [base-url]` (default `http://localhost:3000`) curls `/health`, `/highscore` and `/leaderboard`, not `/records`.
- `api/README.md`: schema, setup and Railway steps. Parts are stale: `create_table.sql` does not exist, `DB_API_URL` is now `API_BASE`, and `/records` is not documented.
- `test_noaa_api.py`: manual check of the live NOAA API, needs `requests`.
- `GLOBAL_HIGHSCORE_README.md`: older design notes for the leaderboard. Parts are stale (it mentions `DB_API_URL` and `POST /highscore`; the firmware now uses `API_BASE` and `/records`).
- `Readme.md` is empty.

## Commands

- In Git Bash on Windows, prefix each `docker run` below with `MSYS_NO_PATHCONV=1`, or docker rejects the rewritten `-w` path.
- Build firmware: `pio run`. Verified in Docker: `docker run --rm -v pio-cache:/root/.platformio -v "$PWD:/work" -w /work python:3.12-slim bash -c "pip install -q platformio && pio run"`.
- Flash: `pio run -t upload`, serial monitor `pio device monitor` at 115200. Not run: needs the board, and agents must not flash.
- Forecast test: verified. From the repo root: `docker run --rm -e TZ=America/New_York -v "$PWD:/src" -w /src emscripten/emsdk:3.1.74 bash -c "git clone -q --depth 1 --branch v6.21.5 https://github.com/bblanchon/ArduinoJson.git /opt/ArduinoJson && em++ -std=gnu++17 -sASYNCIFY -Iemulator/shims -Iinclude -I/opt/ArduinoJson/src test/forecast_test.cpp src/Network.cpp src/Storage.cpp src/Config.cpp src/Theme.cpp emulator/shims/shim_impl.cpp -o /tmp/t.js && node /tmp/t.js"`. It prints `forecast_test: ok`. Keep it on one line: the copy in the header of `test/forecast_test.cpp` breaks after `test/forecast_test.cpp` and fails to link if pasted as shown.
- Emulator build: run the firmware build first (the Makefile takes ArduinoJson from `.pio/libdeps`), then `docker run --rm -v "$PWD:/src" -w /src/emulator emscripten/emsdk:3.1.74 make OUTPUT=/tmp/surfcyd.js`. Verified; the output stays in the container. Drop `OUTPUT=...` to overwrite the committed `emulator/surfcyd.js` and `surfcyd.wasm`. Local route, not run: `cd emulator && ./setup.sh && source ~/emsdk/emsdk_env.sh && make` (`setup.sh` installs the latest emsdk into `~/emsdk`).
- Emulator serve: `npm ci && npm start` at the root (port `PORT`, default 8080). Verified (`npm ci`, then `node server.js`, which is what `npm start` runs). `make serve` (python http.server on 8000) depends on the build target, so it can rebuild over the committed output: not run.
- API: `cd api && npm install && npm start` (or `npm run dev` with nodemon). Not run: needs PostgreSQL. The pool always connects with SSL, so a local database needs SSL turned on. There are no migrations: the `records` table is created by hand (SQL in `api/README.md`; `api/server.js` prints it and exits when the table is missing). Use a throwaway database locally; the live one holds the leaderboard.
- Syntax check: `node --check server.js && node --check api/server.js`. Verified.
- There is no lint config and no CI.

## Configuration

- Root `server.js`: `PORT`.
- `api/server.js` (from env or `api/.env`, see `api/.env.example`): `DATABASE_URL` (required; the server exits without it), `PORT`, `API_KEY` (optional; when set, every route except `/health` needs `X-API-Key` or `?apikey=`).
- Firmware: compile-time constants in `include/Config.h` (pins, touch calibration, refresh and timeout intervals, API URLs), `src/Config.cpp` (default spots, SPIFFS paths) and `API_BASE` in `src/Database.cpp`. There are no build-flag secrets.

## Conventions

- Each module is an `include/X.h` plus `src/X.cpp`. Shared structs go in `Types.h`. Globals are shared with `extern` (for example `gfx`, `touch`, `currentTheme`, `surfLocation`).
- Logging goes through `logInfo`/`logError` to Serial. Some debug lines go straight to Serial: `[TIDE]` in `src/Network.cpp`, `[DISPLAY]` in `src/Display.cpp`, and one per touch from `getTouchPoint()` in `src/TouchUI.cpp`.
- Each setting is a JSON file in SPIFFS with its path in `Config.cpp` and its functions in `Storage.cpp`; `/high_score.json` is the exception, handled in `Game.cpp`. The View Files screen lists every file.
- Draw functions fill in `Rect`s that the touch handlers later hit-test with `pointInRect`. Touch handlers return small int codes that `loop()` switches on.
- A new `src/*.cpp` file must also go in `SRCS` in `emulator/Makefile`, and in the test command if the test needs it.
- `emulator/shims/` are hand-written subsets of the Arduino, WiFi, HTTPClient, SPIFFS/FS, SPI, GFX and XPT2046 APIs. A library call that `src/` starts using needs a stub there, or the emulator build (and the forecast test, for the files it compiles) fails while `pio run` still passes. In the emulator `WiFi.status()` is always connected, so Wi-Fi retry paths only run on hardware.
- Tests: `test/forecast_test.cpp` is the only automated test. It fakes `fetch` and `localStorage` with `EM_JS` (any other URL throws, so nothing reaches the network) and checks results with `assert`. Add new checks the same way and build them with the `em++` command above.
- API routes are inline Express handlers in `api/server.js`, each behind `requireApiKey` except `/health`.
- After firmware changes, rebuild `emulator/surfcyd.js` and `surfcyd.wasm` with the Docker command above (without `OUTPUT=`) and commit both so the web demo matches. Never hand-edit them.

## Gotchas

- The panel inverts colors in hardware. Theme values are the intended colors, and `applyTheme()` XORs them with 0xFFFF. Raw color literals in drawing code must be inverted too (see `~YELLOW` in `Game.cpp`). The inversion comes from the `true /* IPS */` argument to `Arduino_ST7796` in `src/Display.cpp`; leave it set.
- Pin defines in `include/Config.h` match the 3.5" CYD, and touch shares the display's SPI pins (`setupTouch()` in `src/TouchUI.cpp`). Touch axes are mapped inverted with `TOUCH_MIN_*`/`TOUCH_MAX_*`, which are calibration for real panels; the emulator touch shim has its own copy. Do not change pins or calibration without the hardware.
- ESP32 heap is tight for back-to-back HTTPS calls. `fetchSurfForecast()` asks Open-Meteo for one day, uses the compact NWS `/forecast` endpoint (not `/forecast/hourly`), filters JSON, frees each payload and calls NOAA before NWS on purpose (see the comments in `src/Network.cpp`). Do not enlarge these responses or reorder the calls without testing on a board.
- Keep `GET /records` (a JSON array of objects with `name` and `score`) and `POST /records` (`name` plus an integer `score`) backward compatible. There is no OTA code, so set-up devices only get new firmware over USB. Keep `parseTopRecords()` in `include/RecordParser.h`, which reads the array one element at a time and keeps only the top scores.
- Keep SPIFFS paths and JSON keys stable. Set-up devices read them back, and the emulator seeds `/wifi.json` (`ssid`, `password`) and `/location.json` (`location`, `latitude`, `longitude`) in `seedDemoData()` in `emulator/index.html`, and `/player_name.json` (`name`) in `emulator/main_emulator.cpp`.
- Settings `Rect` names do not match their labels: `tideButton` is Reset All (deletes every settings file except the player name and high score, then calls `ESP.restart()`), `forgetButton` is Reset WiFi and `waveButton` is Reset Wave. Check the label in `drawSettingsScreen()` before changing a branch in `handleSettingsScreenTouch()`.
- The View Files screen (`viewFilesScreen()` in `src/Display.cpp`) lists every SPIFFS file and its contents on the device screen, and shows values under the JSON key `password` as `********`. Store any new secret under that key or extend the mask, and never log secrets.
- The firmware and emulator do not send `X-API-Key`. Turning on `API_KEY` for the API needs a matching firmware and emulator change first, or the device and emulator can no longer read or post scores.
- The emulator makes real network calls. Playing the game in it posts scores, as player "Emulator", to the live leaderboard. `api/test_api.sh` also posts TEST1 to TEST3 scores to `POST /highscore`, which stores any that beat the current top score, so only point it at a local API.
- `make clean` in `emulator/` deletes the committed `surfcyd.js` and `surfcyd.wasm` that the web demo serves. Restore them with `git restore emulator/surfcyd.js emulator/surfcyd.wasm`.
- `GET /SurfBoard/` on the root server redirects to itself (Express non-strict routing matches `/SurfBoard` too). `/` and `/SurfBoard/index.html` load fine.
- Tide data depends on NTP time. The device waits up to 20 s for NTP at boot.
- Check `gh pr list` before editing. As of 2026-11-28, PR #8 (conflicting, untouched since February) rewrites `drawGoodSurfGraphic()` and `drawBadSurfGraphic()` in `src/Display.cpp`.
