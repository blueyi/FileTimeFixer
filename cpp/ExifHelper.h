#pragma once

#include <exiv2/exiv2.hpp>
#include <string>

namespace filetimefixer {

bool getExifData(const std::string& filepath, Exiv2::ExifData& exifData);

// Return earliest of EXIF DateTimeOriginal / DateTimeDigitized / Image.DateTime; empty if none found
std::string getExifTimeEarliest(const std::string& filePath);

// Convert "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS" to EXIF format "YYYY:MM:DD HH:MM:SS"
std::string formatTimeForExif(const std::string& timeStr);

// Set all three EXIF time tags to new_datetime (format "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS")
bool modifyExifDataForTime(const std::string& filepath, const std::string& new_datetime);

// Read and return string of the three EXIF time tags for output/log; "(none)" or partial on failure
std::string getExifTimeInfoString(const std::string& filePath);

void printExifTime(const std::string& filePath);

}  // namespace filetimefixer
