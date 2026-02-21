#pragma once

#include <ctime>
#include <string>

namespace filetimefixer {

// Parse UTC/EXIF time string into tm ("YYYY-MM-DDTHH:MM:SS", "YYYY-MM-DD HH:MM:SS", "YYYY:MM:DD HH:MM:SS")
bool parseUTCStringToTm(std::tm& tm, const std::string& utcTimeStr);

// UTC time string -> time_t (returns (time_t)-1 on parse failure)
std::time_t utcStringToTimestamp(const std::string& timeStr);

// time_t -> UTC string "YYYY-MM-DDTHH:MM:SS"
std::string timestampToUTCString(std::time_t timestamp);

// EXIF DateTime string -> UTC string (EXIF treated as UTC+8)
std::string exifDateTimeToUTCString(const std::string& exifDateTime);

// Format as UTC+8 for filename "YYYYMMDD_HHMMSS" or with ms "YYYYMMDD_HHMMSS_mmm"
std::string formatTimeToUTC8Name(const std::string& timeStr);

// If timeStr is date-only (length <= 10), append current UTC time to avoid duplicate filenames
std::string supplementDateWithCurrentUtcTime(const std::string& timeStr);

}  // namespace filetimefixer
