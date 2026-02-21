#pragma once

#include <string>

namespace filetimefixer {

// Parsed time string from filename ("YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DD")
// Validate 8-digit date YYYYMMDD
bool isValidDate(const std::string& dateStr);

// Validate 6-digit time HHMMSS
bool isValidTime(const std::string& timeStr);

// Timestamp to Beijing-time string (seconds or milliseconds)
std::string timestampToBeijingTime(int64_t timestamp, bool isMilliseconds);

// Parse time from filename: 8+6, 8-digit date, 10/13-digit timestamp, mmexport, etc.
// Returns empty string on failure (may print to stderr)
std::string parseFileNameTime(const std::string& filename);

}  // namespace filetimefixer
