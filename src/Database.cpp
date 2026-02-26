#include "Database.h"
#include "Storage.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static const char *API_BASE = "https://surf-board-api-production.up.railway.app";

// Fetch the top-scoring record (GET /records returns rows ordered by score DESC).
GlobalHighScore fetchGlobalHighScore() {
  GlobalHighScore result;
  if (WiFi.status() != WL_CONNECTED) {
    logError("fetchGlobalHighScore: WiFi not connected");
    return result;
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(String(API_BASE) + "/records");
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, payload)) {
      JsonArray arr = doc.as<JsonArray>();
      if (!arr.isNull() && arr.size() > 0) {
        result.name  = arr[0]["name"].as<String>();
        result.score = arr[0]["score"] | 0UL;
        result.valid = true;
        logInfo("Global high score: " + result.name + " - " + String(result.score));
      }
    } else {
      logError("fetchGlobalHighScore: JSON parse failed");
    }
  } else {
    logError("fetchGlobalHighScore: HTTP " + String(code));
  }

  http.end();
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

// Fetch all records ordered by score DESC, return up to 10 as a Leaderboard.
Leaderboard fetchLeaderboard() {
  Leaderboard result;
  if (WiFi.status() != WL_CONNECTED) {
    logError("fetchLeaderboard: WiFi not connected");
    return result;
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(String(API_BASE) + "/records");
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, payload)) {
      JsonArray arr = doc.as<JsonArray>();
      if (!arr.isNull()) {
        for (JsonVariant entry : arr) {
          if (result.count >= 10) break;
          result.entries[result.count].rank  = result.count + 1;
          result.entries[result.count].name  = entry["name"].as<String>();
          result.entries[result.count].score = entry["score"] | 0UL;
          result.count++;
        }
        result.valid = true;
        logInfo("Leaderboard fetched: " + String(result.count) + " entries");
      }
    } else {
      logError("fetchLeaderboard: JSON parse failed");
    }
  } else {
    logError("fetchLeaderboard: HTTP " + String(code));
  }

  http.end();
  return result;
}
