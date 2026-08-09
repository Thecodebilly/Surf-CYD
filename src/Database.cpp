#include "Database.h"
#include "RecordParser.h"
#include "Storage.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static const char *API_BASE = "https://surf-board-api-production.up.railway.app";

// GET /records, keeping the highest `capacity` scores.
//
// The endpoint returns every score ever posted as one array — already several
// kilobytes and growing with each game — so the response is parsed one element
// at a time rather than loaded into a fixed JSON buffer.
//
// Returns the number of records written to `out`, or -1 on failure.
static int fetchTopRecords(TopRecord *out, int capacity, const char *context) {
  if (WiFi.status() != WL_CONNECTED) {
    logError(String(context) + ": WiFi not connected");
    return -1;
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(String(API_BASE) + "/records");
  http.setTimeout(10000);

  int code  = http.GET();
  int count = -1;

  if (code == HTTP_CODE_OK) {
    if (http.getSize() >= 0) {
      // Content-Length known: read straight off the socket.
      count = parseTopRecords(*http.getStreamPtr(), out, capacity);
    } else {
      // Chunked reply — the raw stream would still carry the chunk framing, so
      // let HTTPClient unwrap it first.
      String payload = http.getString();
      MemoryStream buffered(payload.c_str());
      count = parseTopRecords(buffered, out, capacity);
    }
    if (count < 0) logError(String(context) + ": JSON parse failed");
  } else {
    logError(String(context) + ": HTTP " + String(code));
  }

  http.end();
  return count;
}

// Fetch the top-scoring record.
GlobalHighScore fetchGlobalHighScore() {
  GlobalHighScore result;

  TopRecord top[1];
  if (fetchTopRecords(top, 1, "fetchGlobalHighScore") > 0) {
    result.name  = top[0].name;
    result.score = top[0].score;
    result.valid = true;
    logInfo("Global high score: " + result.name + " - " + String(result.score));
  }

  return result;
}

// Submit a new record (POST /records). Returns "" on success or error message on failure.
String submitRecord(const String &name, unsigned long score) {
  if (WiFi.status() != WL_CONNECTED) {
    return "No WiFi connection";
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(String(API_BASE) + "/records");
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument body(256);
  body["name"]  = name;
  body["score"] = score;
  String payload;
  serializeJson(body, payload);

  int code = http.POST(payload);
  String errorMsg = "";

  if (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED) {
    logInfo("Record submitted: " + name + " - " + String(score));
  } else {
    // Try to extract an error message from the response body
    String resp = http.getString();
    DynamicJsonDocument errDoc(512);
    if (!deserializeJson(errDoc, resp) && errDoc.containsKey("error")) {
      errorMsg = errDoc["error"].as<String>();
    } else {
      errorMsg = "HTTP " + String(code);
    }
    logError("submitRecord failed: " + errorMsg);
  }

  http.end();
  return errorMsg;
}

// Fetch the highest LEADERBOARD_SIZE scores.
Leaderboard fetchLeaderboard() {
  Leaderboard result;

  TopRecord top[LEADERBOARD_SIZE];
  int count = fetchTopRecords(top, LEADERBOARD_SIZE, "fetchLeaderboard");
  if (count < 0) return result;

  for (int i = 0; i < count; i++) {
    result.entries[i].rank  = i + 1;
    result.entries[i].name  = top[i].name;
    result.entries[i].score = top[i].score;
  }
  result.count = count;
  result.valid = true;
  logInfo("Leaderboard fetched: " + String(count) + " entries");

  return result;
}
