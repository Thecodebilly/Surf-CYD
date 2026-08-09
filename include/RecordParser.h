#ifndef RECORD_PARSER_H
#define RECORD_PARSER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

// A single parsed record. Fixed-size storage so streaming never allocates.
struct TopRecord {
  char          name[24] = "";
  unsigned long score    = 0;
};

// Stream over an in-memory buffer, for responses that must be buffered before
// they can be parsed.
class MemoryStream : public Stream {
  const char *_data;
  size_t      _len;
  size_t      _pos;

public:
  explicit MemoryStream(const char *data)
      : _data(data ? data : ""), _len(strlen(_data ? data : "")), _pos(0) {}

  int    available() override { return (int)(_len - _pos); }
  int    read() override      { return _pos < _len ? (unsigned char)_data[_pos++] : -1; }
  int    peek() override      { return _pos < _len ? (unsigned char)_data[_pos] : -1; }
  size_t write(uint8_t) override { return 0; }
};

// Inserts one record into `out`, which is kept sorted by score descending and
// never grows past `capacity`.
inline void insertTopRecord(TopRecord *out, int &count, int capacity,
                            const char *name, unsigned long score) {
  if (capacity <= 0) return;
  if (count == capacity && score <= out[capacity - 1].score) return;

  int pos = (count < capacity) ? count : capacity - 1;
  while (pos > 0 && out[pos - 1].score < score) {
    out[pos] = out[pos - 1];
    pos--;
  }

  strncpy(out[pos].name, name ? name : "", sizeof(out[pos].name) - 1);
  out[pos].name[sizeof(out[pos].name) - 1] = '\0';
  out[pos].score = score;

  if (count < capacity) count++;
}

// Reads a JSON array of {"name":...,"score":...} objects one element at a time,
// keeping only the highest `capacity` scores. The API returns every score ever
// posted, so the array must never be held in memory all at once.
//
// Returns the number of records written to `out` (sorted by score descending),
// or -1 if the payload was not a readable JSON array.
inline int parseTopRecords(Stream &stream, TopRecord *out, int capacity) {
  int  count = 0;
  char c;

  // Advance to the start of the array.
  bool foundArray = false;
  while (stream.readBytes(&c, 1) == 1) {
    if (c == '[') { foundArray = true; break; }
    if (!isspace((unsigned char)c)) return -1;  // an error page, not an array
  }
  if (!foundArray) return -1;

  // An empty array has nothing for the parser to consume. peek() is enough to
  // spot it: the byte after '[' arrives in the same packet, and if it somehow
  // has not, we fall through to the blocking parse below.
  while (isspace((unsigned char)stream.peek())) stream.readBytes(&c, 1);
  if (stream.peek() == ']') return 0;

  // Only these two fields are stored, so per-element memory stays constant even
  // if the API starts returning more columns.
  StaticJsonDocument<64> filter;
  filter["name"]  = true;
  filter["score"] = true;

  while (true) {
    StaticJsonDocument<192> entry;
    if (deserializeJson(entry, stream, DeserializationOption::Filter(filter))) {
      return count > 0 ? count : -1;
    }
    insertTopRecord(out, count, capacity, entry["name"] | "", entry["score"] | 0UL);

    // ',' means another element follows, ']' ends the array.
    bool more = false;
    while (stream.readBytes(&c, 1) == 1) {
      if (c == ',') { more = true; break; }
      if (c == ']') break;
    }
    if (!more) break;
  }

  return count;
}

#endif  // RECORD_PARSER_H
