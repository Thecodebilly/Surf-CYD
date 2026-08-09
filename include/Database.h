#ifndef DATABASE_H
#define DATABASE_H

#include <Arduino.h>

struct GlobalHighScore {
  String name = "";
  unsigned long score = 0;
  bool valid = false;
};

struct LeaderboardEntry {
  int rank = 0;
  String name = "";
  unsigned long score = 0;
};

// Number of records the leaderboard screen shows.
static const int LEADERBOARD_SIZE = 10;

struct Leaderboard {
  LeaderboardEntry entries[LEADERBOARD_SIZE];
  int count = 0;
  bool valid = false;
};

// Fetch the top record (highest score) from the API.
GlobalHighScore fetchGlobalHighScore();

// Submit a new record. Returns empty string on success, or error message on failure.
String submitRecord(const String &name, unsigned long score);

// Fetch all records ordered by score descending (up to 10 shown).
Leaderboard fetchLeaderboard();

#endif // DATABASE_H
