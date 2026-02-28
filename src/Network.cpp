#include "Network.h"
#include "Config.h"
#include "Storage.h"
#include "Theme.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;
extern Theme currentTheme;

// Forward declaration
void showStatus(const String &line1, const String &line2, uint16_t color);

// File-scope tide station cache (can be cleared from outside via clearTideStationCache)
static String cachedStationId = "";
static float cachedStationLat = 0.0f;
static float cachedStationLon = 0.0f;

void clearTideStationCache() {
  cachedStationId = "";
  cachedStationLat = 0.0f;
  cachedStationLon = 0.0f;
  Serial.println("[TIDE] Station cache cleared");
}

String urlEncode(const String &value) {
  String encoded;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

bool connectWifi(const WifiCredentials &creds) {
  if (!creds.valid) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
  showStatus("Connecting Wi-Fi", creds.ssid, currentTheme.textSecondary);

  for (uint8_t attempt = 0; attempt < 60; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      showStatus("Wi-Fi connected", WiFi.localIP().toString(), currentTheme.success);
      logInfo("Connected to Wi-Fi " + creds.ssid);
      delay(1000);
      return true;
    }
    delay(500);
  }

  logError("Wi-Fi connection failed for SSID: " + creds.ssid);
  showStatus("Wi-Fi failed", "Tap to re-enter", currentTheme.error);
  return false;
}

std::vector<LocationInfo> fetchLocationMatches(const String &location, int maxResults) {
  std::vector<LocationInfo> matches;
  if (WiFi.status() != WL_CONNECTED) return matches;

  HTTPClient http;
  String url = String(GEOCODE_URL) + "?name=" + urlEncode(location) + "&count=" + String(maxResults) + "&language=en&format=json";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return matches;
  }

  DynamicJsonDocument doc(12 * 1024);
  String payload = http.getString();
  http.end();
  if (deserializeJson(doc, payload)) return matches;

  JsonArray results = doc["results"];
  if (results.isNull()) return matches;

  for (JsonObject result : results) {
    LocationInfo info;
    info.latitude = result["latitude"] | 0.0f;
    info.longitude = result["longitude"] | 0.0f;
    info.displayName = String((const char *)result["name"]);
    String admin1 = result["admin1"] | "";
    String country = result["country"] | "";
    if (!admin1.isEmpty()) info.displayName += ", " + admin1;
    if (!country.isEmpty()) info.displayName += ", " + country;
    info.valid = true;
    matches.push_back(info);
  }
  return matches;
}

LocationInfo fetchLocation(const String &location) {
  LocationInfo info;
  auto matches = fetchLocationMatches(location, 1);
  if (!matches.empty()) return matches[0];
  return info;
}

// Find the nearest NOAA tide station to given coordinates using nearest-neighbor search.
// Covers US East/Gulf/West coasts, Alaska, Hawaii, Puerto Rico, USVI, and Pacific territories.
// Locations outside NOAA coverage (e.g. Portugal, El Salvador, Central America) will return ""
// because no station will be within the MAX_STATION_DISTANCE_KM threshold.
String findNearestTideStation(float latitude, float longitude) {
  if (WiFi.status() != WL_CONNECTED) return "";

  logInfo("Finding NOAA station for coordinates: lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6));

  struct TideStation {
    float lat;
    float lon;
    const char *id;
    const char *name;
  };

  // Maximum distance (km) to accept a station — beyond this tide info is unavailable.
  // ~400 km covers coastal areas well while rejecting truly unsupported regions.
  const float MAX_STATION_DISTANCE_KM = 400.0f;

  static const TideStation stations[] = {
    // ── US EAST COAST — MAINE ───────────────────────────────────────────────
    { 44.903f,  -66.990f, "8410140", "Eastport, ME" },
    { 44.646f,  -67.201f, "8411060", "Cutler Farris Wharf, ME" },
    { 44.387f,  -68.204f, "8413320", "Bar Harbor, ME" },
    { 44.225f,  -68.799f, "8414131", "Blue Hill, ME" },
    { 44.017f,  -69.117f, "8415490", "Rockland, ME" },
    { 43.951f,  -69.657f, "8416121", "Boothbay Harbor, ME" },
    { 43.776f,  -70.008f, "8417177", "Wells, ME" },
    { 43.657f,  -70.247f, "8418150", "Portland, ME" },
    { 43.325f,  -70.560f, "8419317", "Kennebunk, ME" },
    { 43.133f,  -70.650f, "8419363", "York Harbor, ME" },
    // ── US EAST COAST — NEW HAMPSHIRE / MASSACHUSETTS ───────────────────────
    { 42.980f,  -70.817f, "8423898", "Hampton Harbor, NH" },
    { 42.814f,  -70.869f, "8425453", "Newburyport, MA" },
    { 42.608f,  -70.660f, "8441501", "Gloucester, MA" },
    { 42.359f,  -71.050f, "8443970", "Boston, MA" },
    { 42.085f,  -70.203f, "8446166", "Plymouth, MA" },
    { 41.753f,  -70.620f, "8447386", "Fall River, MA" },
    { 41.524f,  -70.672f, "8447435", "New Bedford, MA" },
    { 41.524f,  -70.671f, "8447505", "Chatham, MA" },
    { 41.553f,  -70.614f, "8447930", "Woods Hole, MA" },
    { 41.284f,  -70.099f, "8449130", "Nantucket, MA" },
    // ── US EAST COAST — RHODE ISLAND / CONNECTICUT / NEW YORK ───────────────
    { 41.505f,  -71.327f, "8452660", "Newport, RI" },
    { 41.732f,  -71.340f, "8452944", "Providence, RI" },
    { 41.580f,  -71.427f, "8454049", "Quonset Point, RI" },
    { 41.356f,  -72.090f, "8461490", "New London, CT" },
    { 41.283f,  -72.928f, "8465705", "New Haven, CT" },
    { 41.174f,  -73.182f, "8467150", "Bridgeport, CT" },
    { 41.071f,  -71.960f, "8510560", "Montauk, NY" },
    { 40.890f,  -73.369f, "8516945", "Kings Point, NY" },
    { 40.700f,  -74.014f, "8518750", "The Battery, NY" },
    { 40.638f,  -74.065f, "8519483", "Bergen Point, NY" },
    { 40.623f,  -73.952f, "8517741", "Jamaica Bay, NY" },
    // ── US EAST COAST — NEW JERSEY ──────────────────────────────────────────
    { 40.466f,  -74.009f, "8531680", "Sandy Hook, NJ" },
    { 40.214f,  -74.012f, "8530973", "Raritan Bay, NJ" },
    { 39.944f,  -74.053f, "8533085", "Barnegat, NJ" },
    { 39.356f,  -74.418f, "8534720", "Atlantic City, NJ" },
    { 38.968f,  -74.960f, "8536110", "Cape May, NJ" },
    // ── US EAST COAST — DELAWARE / MARYLAND ─────────────────────────────────
    { 39.579f,  -75.588f, "8551762", "Delaware City, DE" },
    { 38.782f,  -75.119f, "8557380", "Lewes, DE" },
    { 38.327f,  -75.088f, "8570283", "Ocean City Inlet, MD" },
    { 38.571f,  -76.068f, "8571892", "Cambridge, MD" },
    { 38.983f,  -76.483f, "8574680", "Baltimore, MD" },
    { 38.317f,  -76.451f, "8577330", "Solomons Island, MD" },
    { 38.417f,  -76.583f, "8578240", "Bishops Head, MD" },
    { 38.873f,  -77.021f, "8594900", "Washington DC" },
    // ── US EAST COAST — VIRGINIA ─────────────────────────────────────────────
    { 37.167f,  -76.444f, "8637689", "Yorktown, VA" },
    { 36.947f,  -76.330f, "8638610", "Sewells Point, VA" },
    { 37.015f,  -76.016f, "8632200", "Kiptopeke, VA" },
    { 36.832f,  -76.298f, "8639348", "Money Point, VA" },
    // ── US EAST COAST — NORTH CAROLINA ──────────────────────────────────────
    { 36.183f,  -75.747f, "8651370", "Duck, NC" },
    { 35.792f,  -75.549f, "8652587", "Oregon Inlet Marina, NC" },
    { 35.228f,  -75.703f, "8654467", "Hatteras, NC" },
    { 34.718f,  -76.667f, "8656483", "Beaufort, NC" },
    { 34.227f,  -77.953f, "8658120", "Wilmington, NC" },
    { 33.920f,  -78.015f, "8659084", "Southport, NC" },
    // ── US EAST COAST — SOUTH CAROLINA ──────────────────────────────────────
    { 33.655f,  -78.918f, "8661070", "Springmaid Pier, SC" },
    { 33.352f,  -79.186f, "8662245", "Georgetown, SC" },
    { 32.782f,  -79.926f, "8665530", "Charleston, SC" },
    { 32.457f,  -80.671f, "8669289", "Hilton Head, SC" },
    // ── US EAST COAST — GEORGIA ──────────────────────────────────────────────
    { 32.083f,  -80.900f, "8670870", "Fort Pulaski, GA" },
    { 31.233f,  -81.400f, "8677344", "Brunswick, GA" },
    // ── US EAST COAST — FLORIDA (EAST) ──────────────────────────────────────
    { 30.670f,  -81.466f, "8720030", "Fernandina Beach, FL" },
    { 30.397f,  -81.428f, "8720218", "Mayport, FL" },
    { 29.859f,  -81.265f, "8720587", "St. Augustine, FL" },
    { 29.228f,  -81.023f, "8721604", "Daytona Beach, FL" },
    { 28.454f,  -80.558f, "8722956", "Port Canaveral, FL" },
    { 27.660f,  -80.237f, "8722670", "Lake Worth Pier, FL" },
    { 26.127f,  -80.104f, "8722669", "Lake Worth, FL" },
    { 25.774f,  -80.130f, "8723170", "Miami Beach, FL" },
    { 25.521f,  -80.374f, "8723214", "Virginia Key, FL" },
    { 24.943f,  -80.537f, "8724580", "Key Largo, FL" },
    { 24.555f,  -81.808f, "8724580", "Key West, FL" },
    // ── US GULF COAST — FLORIDA (WEST) ──────────────────────────────────────
    { 26.133f,  -81.795f, "8725110", "Naples, FL" },
    { 26.448f,  -82.086f, "8725520", "Fort Myers, FL" },
    { 26.960f,  -82.460f, "8726384", "Port Charlotte, FL" },
    { 27.767f,  -82.630f, "8726520", "St. Petersburg, FL" },
    { 27.920f,  -82.437f, "8726607", "Old Port Tampa, FL" },
    { 28.929f,  -82.878f, "8727235", "Cedar Key, FL" },
    { 30.072f,  -84.178f, "8727520", "St. Marks, FL" },
    { 29.730f,  -85.028f, "8728690", "Apalachicola, FL" },
    { 30.154f,  -85.660f, "8729108", "Panama City, FL" },
    // ── US GULF COAST — ALABAMA / MISSISSIPPI ────────────────────────────────
    { 30.404f,  -87.211f, "8729840", "Pensacola, FL" },
    { 30.250f,  -88.075f, "8735180", "Dauphin Island, AL" },
    { 30.691f,  -88.043f, "8737048", "Mobile State Docks, AL" },
    { 30.268f,  -89.090f, "8741533", "Pascagoula, MS" },
    { 30.396f,  -88.885f, "8744117", "Biloxi, MS" },
    { 30.325f,  -89.326f, "8747437", "Bay Waveland, MS" },
    // ── US GULF COAST — LOUISIANA ────────────────────────────────────────────
    { 30.220f,  -89.930f, "8760551", "South Pass, LA" },
    { 29.263f,  -89.957f, "8761724", "Grand Isle, LA" },
    { 29.949f,  -90.070f, "8761927", "New Canal Station, LA" },
    { 29.780f,  -90.420f, "8764044", "Berwick, LA" },
    { 29.456f,  -91.327f, "8764227", "Eugene Island, LA" },
    { 29.767f,  -91.883f, "8764314", "Cypremort Point, LA" },
    { 29.782f,  -93.326f, "8767816", "Lake Charles, LA" },
    { 29.768f,  -93.343f, "8768094", "Calcasieu Pass, LA" },
    // ── US GULF COAST — TEXAS ────────────────────────────────────────────────
    { 29.690f,  -93.912f, "8770570", "Sabine Pass North, TX" },
    { 29.361f,  -94.793f, "8771341", "Galveston (Pier 21), TX" },
    { 29.285f,  -94.789f, "8771450", "Galveston (Pleasure Pier), TX" },
    { 28.944f,  -95.323f, "8772447", "Freeport, TX" },
    { 29.681f,  -95.028f, "8773037", "Morgans Point, TX" },
    { 29.373f,  -94.897f, "8771341", "Texas City, TX" },
    { 28.022f,  -97.047f, "8774770", "Rockport, TX" },
    { 27.834f,  -97.054f, "8775870", "Corpus Christi, TX" },
    { 27.427f,  -97.221f, "8776604", "Baffin Bay, TX" },
    { 26.063f,  -97.216f, "8779770", "Port Isabel, TX" },
    // ── US WEST COAST — WASHINGTON ───────────────────────────────────────────
    { 48.370f, -124.616f, "9443090", "Neah Bay, WA" },
    { 48.125f, -123.438f, "9444090", "Port Angeles, WA" },
    { 48.113f, -122.760f, "9444900", "Port Townsend, WA" },
    { 48.584f, -123.013f, "9449424", "Friday Harbor, WA" },
    { 47.602f, -122.339f, "9447130", "Seattle, WA" },
    { 47.269f, -122.415f, "9446484", "Tacoma, WA" },
    { 47.052f, -122.905f, "9446482", "Olympia, WA" },
    { 47.909f, -124.637f, "9441102", "Westport, WA" },
    { 46.656f, -123.822f, "9440910", "Cape Disappointment, WA" },
    // ── US WEST COAST — OREGON ───────────────────────────────────────────────
    { 46.208f, -123.768f, "9439040", "Astoria, OR" },
    { 44.625f, -124.050f, "9435380", "Newport, OR" },
    { 43.365f, -124.217f, "9432780", "Charleston, OR" },
    { 42.744f, -124.504f, "9431647", "Port Orford, OR" },
    // ── US WEST COAST — CALIFORNIA ───────────────────────────────────────────
    { 41.745f, -124.184f, "9419750", "Crescent City, CA" },
    { 40.770f, -124.179f, "9418767", "Humboldt Bay, CA" },
    { 38.955f, -123.745f, "9415020", "Point Arena, CA" },
    { 38.062f, -122.919f, "9415020", "Point Reyes, CA" },
    { 37.806f, -122.465f, "9414290", "San Francisco, CA" },
    { 37.902f, -122.361f, "9414863", "Richmond, CA" },
    { 37.928f, -122.025f, "9415096", "Martinez, CA" },
    { 37.772f, -122.225f, "9414750", "Alameda, CA" },
    { 37.334f, -121.899f, "9413617", "Alviso Slough, CA" },
    { 36.965f, -122.017f, "9413745", "Santa Cruz, CA" },
    { 36.605f, -121.898f, "9413450", "Monterey, CA" },
    { 35.179f, -120.754f, "9412110", "Port San Luis, CA" },
    { 34.408f, -119.688f, "9411340", "Santa Barbara, CA" },
    { 34.002f, -119.494f, "9411399", "Port Hueneme, CA" },
    { 33.961f, -118.423f, "9410660", "Los Angeles, CA" },
    { 33.720f, -118.272f, "9410230", "Cabrillo Beach, CA" },
    { 33.456f, -118.489f, "9410079", "Avalon, CA" },
    { 32.867f, -117.258f, "9410230", "La Jolla, CA" },
    { 32.714f, -117.174f, "9410170", "San Diego, CA" },
    // ── ALASKA — SOUTHEAST ───────────────────────────────────────────────────
    { 55.342f, -131.626f, "9450460", "Ketchikan, AK" },
    { 56.469f, -132.377f, "9451054", "Wrangell, AK" },
    { 56.585f, -132.956f, "9451600", "Petersburg, AK" },
    { 57.050f, -135.340f, "9451600", "Sitka, AK" },
    { 58.299f, -134.411f, "9452210", "Juneau, AK" },
    { 59.546f, -139.727f, "9453220", "Yakutat, AK" },
    // ── ALASKA — SOUTHCENTRAL ────────────────────────────────────────────────
    { 60.558f, -145.756f, "9454050", "Cordova, AK" },
    { 61.128f, -146.361f, "9454240", "Valdez, AK" },
    { 61.238f, -149.891f, "9455090", "Anchorage, AK" },
    { 59.441f, -151.716f, "9455500", "Seldovia, AK" },
    { 59.596f, -151.414f, "9455920", "Homer, AK" },
    { 57.731f, -152.510f, "9457292", "Kodiak, AK" },
    { 56.902f, -153.946f, "9457804", "Alitak, AK" },
    // ── ALASKA — SOUTHWEST / ALEUT ───────────────────────────────────────────
    { 55.339f, -160.497f, "9459450", "Sand Point, AK" },
    { 54.991f, -163.535f, "9459881", "King Cove, AK" },
    { 53.880f, -166.536f, "9462450", "Unalaska/Dutch Harbor, AK" },
    { 52.943f, -168.876f, "9462620", "Nikolski, AK" },
    { 51.863f, -176.658f, "9461710", "Adak Island, AK" },
    // ── ALASKA — WEST / ARCTIC ───────────────────────────────────────────────
    { 64.500f, -165.427f, "9468756", "Nome, AK" },
    { 63.881f, -160.800f, "9468333", "Unalakleet, AK" },
    { 66.897f, -162.595f, "9491094", "Red Dog Dock, AK" },
    { 70.411f, -148.525f, "9497645", "Prudhoe Bay, AK" },
    // ── HAWAII ───────────────────────────────────────────────────────────────
    { 19.730f, -155.060f, "1617760", "Hilo, HI" },
    { 19.730f, -156.064f, "1617433", "Kawaihae, HI" },
    { 20.895f, -156.477f, "1615680", "Kahului, HI" },
    { 20.731f, -156.441f, "1615680", "Maalaea, HI" },
    { 20.890f, -156.683f, "1615093", "Kaunakakai, HI" },
    { 21.307f, -157.867f, "1612340", "Honolulu, HI" },
    { 21.395f, -157.799f, "1613198", "Mokuoloe, HI" },
    { 21.482f, -158.197f, "1612340", "Waianae, HI" },
    { 21.954f, -159.356f, "1611400", "Nawiliwili, HI" },
    { 21.890f, -159.602f, "1611400", "Port Allen, HI" },
    { 28.210f, -177.375f, "1619910", "Midway Islands, HI" },
    // ── PUERTO RICO ──────────────────────────────────────────────────────────
    { 18.469f,  -66.117f, "9755371", "San Juan, PR" },
    { 18.474f,  -66.721f, "9757809", "Arecibo, PR" },
    { 18.427f,  -67.154f, "9759110", "Aguadilla, PR" },
    { 18.214f,  -67.159f, "9759938", "Mayaguez, PR" },
    { 17.970f,  -66.614f, "9759110", "Magueyes Island, PR" },
    { 17.970f,  -66.400f, "9759394", "La Parguera, PR" },
    { 18.000f,  -65.889f, "9752695", "Esperanza, PR" },
    { 18.335f,  -65.634f, "9753216", "Fajardo, PR" },
    { 17.962f,  -66.151f, "9754228", "Yabucoa Harbor, PR" },
    // ── US VIRGIN ISLANDS ────────────────────────────────────────────────────
    { 18.340f,  -64.924f, "9751401", "Charlotte Amalie, USVI" },
    { 17.746f,  -64.702f, "9751639", "Christiansted, USVI" },
    { 18.301f,  -64.998f, "9751381", "Lameshur Bay, USVI" },
    { 18.195f,  -65.012f, "9752235", "Culebra, USVI" },
    // ── GREAT LAKES ──────────────────────────────────────────────────────────
    { 46.785f,  -92.093f, "9099041", "Duluth, MN" },
    { 46.721f,  -92.103f, "9099018", "Superior Entry, WI" },
    { 46.495f,  -84.349f, "9076024", "Sault Ste. Marie, MI" },
    { 46.597f,  -90.887f, "9087044", "Ashland, WI" },
    { 44.545f,  -87.560f, "9087072", "Green Bay, WI" },
    { 44.460f,  -87.490f, "9087079", "Kewaunee, WI" },
    { 43.740f,  -87.700f, "9087044", "Sheboygan, WI" },
    { 43.038f,  -87.880f, "9087023", "Milwaukee, WI" },
    { 42.350f,  -87.830f, "9087096", "Waukegan, IL" },
    { 41.770f,  -87.532f, "9086080", "Chicago, IL" },
    { 42.360f,  -86.220f, "9087057", "Holland, MI" },
    { 43.232f,  -86.250f, "9087057", "Muskegon, MI" },
    { 44.783f,  -85.630f, "9087044", "Traverse City, MI" },
    { 46.473f,  -84.367f, "9076024", "St. Marys River, MI" },
    { 42.880f,  -82.430f, "9034052", "Port Huron, MI" },
    { 41.524f,  -82.708f, "9063053", "Marblehead, OH" },
    { 41.691f,  -83.472f, "9063054", "Toledo, OH" },
    { 41.502f,  -81.694f, "9063012", "Cleveland, OH" },
    { 41.766f,  -81.278f, "9063028", "Fairport, OH" },
    { 42.083f,  -80.090f, "9063007", "Conneaut, OH" },
    { 42.130f,  -79.974f, "9063020", "Erie, PA" },
    { 42.878f,  -78.882f, "9063020", "Buffalo, NY" },
    // ── PACIFIC TERRITORIES ───────────────────────────────────────────────────
    { 13.444f,  144.655f, "1631428", "Apra Harbor, Guam" },
    { 14.276f,  145.023f, "1632200", "Saipan, Northern Mariana Islands" },
    { -14.281f,-170.690f, "1770000", "Pago Pago, American Samoa" },
    { 19.283f,  166.618f, "1630000", "Wake Island" },
    { 16.745f,  169.523f, "1622670", "Johnston Atoll" },
    // ── ADDITIONAL US EAST COAST FILLS ───────────────────────────────────────
    { 44.106f,  -68.990f, "8414250", "Penobscot Bay, ME" },
    { 43.072f,  -70.712f, "8419317", "Portsmouth, NH" },
    { 42.534f,  -71.164f, "8442150", "Salem, MA" },
    { 42.090f,  -70.660f, "8446166", "Duxbury, MA" },
    { 41.668f,  -71.162f, "8447435", "Taunton River, MA" },
    { 39.087f,  -74.802f, "8535951", "Ocean City, NJ" },
    { 38.687f,  -75.361f, "8556762", "Rehoboth Beach, DE" },
    { 38.221f,  -76.070f, "8571773", "Oxford, MD" },
    { 37.528f,  -76.295f, "8632200", "Windmill Point, VA" },
    { 37.290f,  -76.502f, "8632837", "Yorktown, VA" },
    { 36.620f,  -76.018f, "8639348", "Chesapeake, VA" },
    { 35.918f,  -76.478f, "8651370", "Stumpy Point, NC" },
    { 35.105f,  -76.843f, "8655875", "Cherry Branch, NC" },
    { 34.905f,  -76.680f, "8656483", "Morehead City, NC" },
    // ── ADDITIONAL GULF COAST FILLS ──────────────────────────────────────────
    { 30.162f,  -85.802f, "8729108", "Panama City Beach, FL" },
    { 30.348f,  -87.541f, "8729210", "Navarre Beach, FL" },
    { 30.404f,  -87.650f, "8729840", "Perdido Pass, AL" },
    { 29.722f,  -94.983f, "8770822", "Texas Point, TX" },
    { 29.450f,  -95.022f, "8772447", "Texas City, TX" },
    { 29.100f,  -95.580f, "8773259", "Matagorda Ship Channel, TX" },
    { 28.444f,  -96.397f, "8773767", "Port O'Connor, TX" },
    { 27.818f,  -97.398f, "8775870", "Ingleside, TX" },
    // ── ADDITIONAL WEST COAST FILLS ──────────────────────────────────────────
    { 48.940f, -122.750f, "9449880", "Bellingham, WA" },
    { 47.520f, -122.620f, "9446484", "Bremerton, WA" },
    { 46.970f, -124.105f, "9440910", "Grays Harbor, WA" },
    { 45.555f, -122.673f, "9440910", "Columbia River, WA" },
    { 45.555f, -122.673f, "9440422", "Vancouver, WA" },
    { 44.050f, -124.112f, "9434939", "Florence, OR" },
    { 43.371f, -124.210f, "9432780", "Coos Bay, OR" },
    { 42.345f, -124.368f, "9431647", "Gold Beach, OR" },
    { 40.166f, -124.181f, "9418767", "Eureka, CA" },
    { 39.445f, -123.807f, "9416841", "Fort Bragg, CA" },
    { 38.330f, -123.046f, "9415020", "Bodega Bay, CA" },
    { 37.491f, -122.216f, "9414523", "Redwood City, CA" },
    { 37.596f, -122.032f, "9414750", "Newark Slough, CA" },
    { 37.930f, -122.416f, "9414795", "Crockett, CA" },
    { 38.065f, -122.122f, "9415095", "Benicia, CA" },
    { 38.120f, -122.935f, "9415020", "Tomales Bay, CA" },
    { 35.673f, -121.119f, "9412340", "Cayucos, CA" },
    { 34.914f, -120.440f, "9411541", "Vandenberg, CA" },
    { 34.460f, -120.017f, "9411081", "Gaviota, CA" },
    { 34.007f, -118.498f, "9410580", "Santa Monica, CA" },
    { 33.463f, -117.714f, "9410195", "Dana Point, CA" },
    { 33.629f, -117.928f, "9410660", "Newport Beach, CA" },
    { 33.159f, -117.389f, "9410170", "Oceanside, CA" },
    // ── ADDITIONAL ALASKA FILLS ──────────────────────────────────────────────
    { 58.388f, -135.724f, "9452400", "Skagway, AK" },
    { 58.357f, -134.575f, "9452210", "Auke Bay, AK" },
    { 58.960f, -135.340f, "9453220", "Gustavus, AK" },
    { 60.120f, -149.441f, "9455090", "Seward, AK" },
    { 60.765f, -146.353f, "9454240", "Whittier, AK" },
    { 61.577f, -148.990f, "9455090", "Wasilla, AK" },
    { 59.050f, -158.490f, "9459450", "Dillingham, AK" },
    { 58.300f, -162.018f, "9459881", "Naknek, AK" },
    { 57.537f, -157.485f, "9457804", "Egegik, AK" },
    { 54.133f, -165.780f, "9462450", "Akutan, AK" },
    { 63.748f, -171.489f, "9468756", "St. Lawrence Island, AK" },
    { 64.731f, -163.421f, "9468333", "Elim, AK" },
    { 67.884f, -164.789f, "9491094", "Kotzebue, AK" },
    { 71.293f, -156.789f, "9497645", "Barrow/Utqiagvik, AK" },
    // ── ADDITIONAL HAWAII FILLS ──────────────────────────────────────────────
    { 21.185f, -157.083f, "1612340", "Kaneohe Bay, HI" },
    { 20.026f, -155.834f, "1617760", "Kailua-Kona, HI" },
    { 18.912f, -155.651f, "1617760", "Punaluu, HI" },
    { 20.783f, -157.038f, "1615680", "Lanai City, HI" },
    { 21.100f, -157.025f, "1612340", "Laie, HI" },
    // ── ADDITIONAL PUERTO RICO FILLS ─────────────────────────────────────────
    { 18.370f,  -65.327f, "9753216", "Vieques, PR" },
    { 18.461f,  -66.439f, "9757809", "Manati, PR" },
    { 18.090f,  -65.460f, "9754228", "Humacao, PR" },
    { 18.003f,  -66.614f, "9759110", "Ponce, PR" },
    // ── ADDITIONAL GREAT LAKES FILLS ─────────────────────────────────────────
    { 43.952f,  -76.495f, "9063020", "Sackets Harbor, NY" },
    { 43.456f,  -76.524f, "9063020", "Oswego, NY" },
    { 43.165f,  -77.620f, "9063020", "Rochester, NY" },
    { 42.250f,  -83.150f, "9034052", "Detroit, MI" },
    { 42.108f,  -86.458f, "9087057", "South Haven, MI" },
    { 43.789f,  -82.479f, "9034052", "Port Sanilac, MI" },
    { 44.350f,  -83.903f, "9034052", "Oscoda, MI" },
    { 45.068f,  -83.431f, "9034052", "Cheboygan, MI" },
    { 45.777f,  -84.720f, "9076024", "Mackinaw City, MI" },
    { 46.357f,  -87.994f, "9099018", "Marquette, MI" },
    { 47.122f,  -88.568f, "9099041", "Houghton, MI" },
    { 42.728f,  -87.795f, "9086080", "Kenosha, WI" },
    { 43.800f,  -88.000f, "9087079", "Manitowoc, WI" },
    { 44.088f,  -87.660f, "9087072", "Two Rivers, WI" },
    // ── ADDITIONAL PACIFIC TERRITORIES ───────────────────────────────────────
    { 13.474f,  144.718f, "1631428", "Agana, Guam" },
    { 15.215f,  145.761f, "1632200", "Rota, Northern Mariana Islands" },
    { -14.174f,-170.772f, "1770000", "Leone, American Samoa" },
    { -14.307f,-170.763f, "1770000", "Aunu'u, American Samoa" },
  };

  const int stationCount = sizeof(stations) / sizeof(stations[0]);

  // Haversine distance in km
  auto haversineKm = [](float lat1, float lon1, float lat2, float lon2) -> float {
    const float R = 6371.0f;
    float dLat = (lat2 - lat1) * 0.01745329f;
    float dLon = (lon2 - lon1) * 0.01745329f;
    float a = sinf(dLat / 2) * sinf(dLat / 2) +
              cosf(lat1 * 0.01745329f) * cosf(lat2 * 0.01745329f) *
              sinf(dLon / 2) * sinf(dLon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
  };

  int    bestIdx  = -1;
  float  bestDist = 1e9f;
  for (int i = 0; i < stationCount; i++) {
    float d = haversineKm(latitude, longitude, stations[i].lat, stations[i].lon);
    if (d < bestDist) {
      bestDist = d;
      bestIdx  = i;
    }
  }

  if (bestIdx >= 0 && bestDist <= MAX_STATION_DISTANCE_KM) {
    logInfo("Nearest NOAA station: " + String(stations[bestIdx].name) +
            " (" + String(stations[bestIdx].id) + ") — " +
            String(bestDist, 1) + " km away");
    return String(stations[bestIdx].id);
  }

  logError("No NOAA station within " + String(MAX_STATION_DISTANCE_KM, 0) +
           " km (nearest was " + String(bestDist, 1) + " km). Tide data unavailable for this region.");
  return "";
}

// Fetch current tide height from NOAA station
float fetchNOAATideHeight(const String &stationId, float &minTide, float &maxTide) {
  if (WiFi.status() != WL_CONNECTED || stationId.isEmpty()) {
    logError("fetchNOAATideHeight: WiFi not connected or stationId empty");
    minTide = 0.0f;
    maxTide = 0.0f;
    return 0.0f;
  }
  
  // Check if NTP time sync has completed, retry if not
  time_t now = time(nullptr);
  if (now < 1000000000) {
    logError("fetchNOAATideHeight: NTP time not synced (time=" + String(now) + "), retrying...");
    // Try syncing again
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    for (int i = 0; i < 30; i++) {
      delay(100);
      now = time(nullptr);
      if (now >= 1000000000) {
        logInfo("NTP sync succeeded on retry: " + String(now));
        break;
      }
    }
    
    // If still not synced, fail
    if (now < 1000000000) {
      logError("fetchNOAATideHeight: NTP sync failed even after retry (time=" + String(now) + ")");
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
  }
  
  // Get current date in YYYYMMDD format
  struct tm *timeinfo = localtime(&now);
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y%m%d", timeinfo);
  String currentDate = String(dateStr);
  String cacheKey = currentDate + "_gmt"; // suffix ensures old lst_ldt cache is ignored
  
  // Check if we have cached tide bounds for today
  String cachedDate = "";
  bool hasCachedBounds = loadTideBounds(minTide, maxTide, cachedDate);

  // Invalidate if stale date, wrong tz key, or both zero
  if (hasCachedBounds && (cachedDate != cacheKey || (minTide == 0.0f && maxTide == 0.0f))) {
    logInfo("Cached tide bounds invalid or stale (" + cachedDate + " vs " + cacheKey + "), refetching");
    hasCachedBounds = false;
  }
  
  // If we don't have today's valid bounds cached, fetch from API
  if (!hasCachedBounds) {
    logInfo("Fetching NOAA tides for station " + stationId + " for today: " + currentDate);
    
    HTTPClient http;
    // Fetch only today's predictions in GMT so hours match the device's UTC clock
    String url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&station=" + stationId + 
                 "&datum=MLLW&time_zone=gmt&units=english&interval=h&begin_date=" + currentDate + 
                 "&end_date=" + currentDate + "&format=json";
    Serial.printf("[TIDE] Non-cached URL: %s\n", url.c_str());
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      logError("Failed to fetch NOAA tide data: HTTP " + String(code) + " for URL: " + url);
      http.end();
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    String payload = http.getString();
    http.end();
    
    logInfo("NOAA API response (first 200 chars): " + payload.substring(0, min(200, (int)payload.length())));
    
    DynamicJsonDocument doc(8 * 1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      logError("Failed to parse NOAA tide JSON: " + String(error.c_str()));
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    JsonArray predictions = doc["predictions"];
    if (predictions.isNull() || predictions.size() == 0) {
      // Check if there's an error message in the response
      if (doc.containsKey("error")) {
        logError("NOAA API error: " + String((const char*)doc["error"]["message"]));
      } else {
        logError("No tide predictions in NOAA response (array size: " + String(predictions.size()) + ")");
      }
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    logInfo("NOAA returned " + String(predictions.size()) + " tide predictions for today");
    Serial.printf("[TIDE] predictions count=%d, current hour=%d\n", predictions.size(), timeinfo->tm_hour);
    
    // Find the prediction for the current hour (predictions[0] is always midnight)
    int currentHourNow = timeinfo->tm_hour;
    String currentTideStr = predictions[0]["v"] | "0.0"; // fallback to midnight
    for (JsonVariant pred : predictions) {
      String timeStr = pred["t"] | "";
      int predHour = timeStr.length() >= 13 ? timeStr.substring(11, 13).toInt() : -1;
      if (predHour == currentHourNow) {
        currentTideStr = pred["v"] | "0.0";
        Serial.printf("[TIDE] Matched hour %d: t=%s v=%s\n", currentHourNow, timeStr.c_str(), currentTideStr.c_str());
        break;
      }
    }
    float currentTideHeight = currentTideStr.toFloat();
    Serial.printf("[TIDE] currentTideHeight=%.2f ft\n", currentTideHeight);
    
    // Find min and max from all predictions for today
    float minTideFeet = currentTideHeight;
    float maxTideFeet = currentTideHeight;
    
    for (JsonVariant pred : predictions) {
      String heightStr = pred["v"] | "0.0";
      float height = heightStr.toFloat();
      if (height < minTideFeet) minTideFeet = height;
      if (height > maxTideFeet) maxTideFeet = height;
    }
    
    // Convert from feet to meters for consistency with rest of app
    float currentTideMeters = currentTideHeight * 0.3048f;
    minTide = minTideFeet * 0.3048f;
    maxTide = maxTideFeet * 0.3048f;
    
    // Save today's bounds to file for reuse (with _gmt suffix to distinguish from old lst_ldt cache)
    saveTideBounds(minTide, maxTide, cacheKey);
    
    logInfo("NOAA tide - Current: " + String(currentTideHeight, 2) + " ft (" + String(currentTideMeters, 2) + " m), " +
            "Daily Range: " + String(minTideFeet, 2) + " to " + String(maxTideFeet, 2) + " ft (" + 
            String(minTide, 2) + " to " + String(maxTide, 2) + " m)");
    Serial.printf("[TIDE] RETURNING currentM=%.3f, minM=%.3f, maxM=%.3f\n", currentTideMeters, minTide, maxTide);
    
    return currentTideMeters;
  } else {
    // We have today's bounds cached, just fetch current tide height
    logInfo("Using cached tide bounds for " + cacheKey + ": " + String(minTide, 2) + "m to " + String(maxTide, 2) + "m");
    Serial.printf("[TIDE] Cached branch: minM=%.3f maxM=%.3f cacheKey=%s\n", minTide, maxTide, cacheKey.c_str());
    
    HTTPClient http;
    // Fetch full day predictions in GMT so hours match the device's UTC clock
    String url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&station=" + stationId + 
                 "&datum=MLLW&time_zone=gmt&units=english&interval=h&begin_date=" + currentDate + 
                 "&end_date=" + currentDate + "&format=json";
    Serial.printf("[TIDE] Cached URL: %s\n", url.c_str());
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      logError("Failed to fetch current NOAA tide: HTTP " + String(code));
      http.end();
      return 0.0f;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(8 * 1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      logError("Failed to parse NOAA tide JSON: " + String(error.c_str()));
      return 0.0f;
    }
    
    JsonArray predictions = doc["predictions"];
    if (predictions.isNull() || predictions.size() == 0) {
      logError("No current tide prediction in NOAA response");
      return 0.0f;
    }
    
    // Find the prediction for the current hour (predictions[0] is always midnight)
    int currentHourNow = timeinfo->tm_hour;
    String currentTideStr = predictions[0]["v"] | "0.0"; // fallback to first
    Serial.printf("[TIDE] Cached branch: predictions=%d, currentHour=%d\n", predictions.size(), currentHourNow);
    for (JsonVariant pred : predictions) {
      String timeStr = pred["t"] | "";
      int predHour = timeStr.length() >= 13 ? timeStr.substring(11, 13).toInt() : -1;
      if (predHour == currentHourNow) {
        currentTideStr = pred["v"] | "0.0";
        Serial.printf("[TIDE] Cached matched hour %d: t=%s v=%s\n", currentHourNow, timeStr.c_str(), currentTideStr.c_str());
        break;
      }
    }
    float currentTideHeight = currentTideStr.toFloat();
    float currentTideMeters = currentTideHeight * 0.3048f;
    
    logInfo("Current tide: " + String(currentTideHeight, 2) + " ft (" + String(currentTideMeters, 2) + " m)");
    Serial.printf("[TIDE] Cached RETURNING currentM=%.3f\n", currentTideMeters);
    
    return currentTideMeters;
  }
}

SurfForecast fetchSurfForecast(float latitude, float longitude) {
  SurfForecast forecast;
  if (WiFi.status() != WL_CONNECTED) return forecast;

  // Fetch wave data from Marine API
  HTTPClient http;
  String url = String(MARINE_URL) + "?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) + "&hourly=wave_height,wave_period,wave_direction&timezone=auto";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return forecast;
  }

  DynamicJsonDocument doc(24 * 1024);
  String payload = http.getString();
  http.end();
  if (deserializeJson(doc, payload)) return forecast;

  JsonArray times = doc["hourly"]["time"];
  JsonArray heights = doc["hourly"]["wave_height"];
  JsonArray periods = doc["hourly"]["wave_period"];
  JsonArray directions = doc["hourly"]["wave_direction"];
  if (times.isNull() || heights.isNull() || periods.isNull() || directions.isNull() || times.size() == 0) {
    return forecast;
  }

  forecast.timeLabel = String(times[0].as<const char *>());
  forecast.waveHeight = heights[0] | 0.0f;
  forecast.wavePeriod = periods[0] | 0.0f;
  forecast.waveDirection = directions[0] | 0.0f;

  // Fetch wind data from Weather Forecast API
  url = String(WEATHER_URL) + "?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) + "&hourly=wind_speed_10m,wind_direction_10m&wind_speed_unit=mph&timezone=auto";
  http.begin(url);
  code = http.GET();
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
    http.end();
    DynamicJsonDocument windDoc(12 * 1024);
    if (deserializeJson(windDoc, payload) == DeserializationError::Ok) {
      JsonArray windSpeeds = windDoc["hourly"]["wind_speed_10m"];
      JsonArray windDirections = windDoc["hourly"]["wind_direction_10m"];
      if (!windSpeeds.isNull() && !windDirections.isNull() && windSpeeds.size() > 0) {
        forecast.windSpeed = windSpeeds[0] | 0.0f;
        forecast.windDirection = windDirections[0] | 0.0f;
      }
    }
  } else {
    http.end();
  }
  
  // Fetch tide data from NOAA
  // Use file-scope cached station ID (can be cleared when location changes)
  
  // Check if we need to find a new station (location changed significantly)
  if (cachedStationId.isEmpty() || abs(latitude - cachedStationLat) > 0.5f || abs(longitude - cachedStationLon) > 0.5f) {
    logInfo("Finding nearest NOAA tide station for lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6));
    cachedStationId = findNearestTideStation(latitude, longitude);
    cachedStationLat = latitude;
    cachedStationLon = longitude;
  } else {
    logInfo("Using cached NOAA station: " + cachedStationId + " (lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6) + ")");
  }
  
  // Fetch tide height from NOAA
  if (!cachedStationId.isEmpty()) {
    forecast.tideHeight = fetchNOAATideHeight(cachedStationId, forecast.minTide, forecast.maxTide);
    Serial.printf("[TIDE] forecast: height=%.3fm, min=%.3fm, max=%.3fm\n", forecast.tideHeight, forecast.minTide, forecast.maxTide);
  } else {
    logError("No NOAA tide station found, using default tide height");
    forecast.tideHeight = 0.0f;
    forecast.minTide = 0.0f;
    forecast.maxTide = 0.0f;
  }

  forecast.valid = true;
  return forecast;
}
