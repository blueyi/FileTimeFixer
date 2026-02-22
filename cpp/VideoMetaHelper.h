#pragma once

#include <string>

namespace filetimefixer {

/// Get QuickTime/MP4 creation_time from video file (via ffprobe). Returns UTC string "YYYY-MM-DDTHH:MM:SS" or empty.
std::string getVideoCreationTimeUtc(const std::string& filePath);

/// Set creation_time in video file (via ffmpeg). Returns true on success. Requires ffmpeg on PATH.
bool setVideoCreationTime(const std::string& filePath, const std::string& targetTimeUtc);

/// Get a short string describing video time metadata for logging (e.g. "creation_time=2023-10-23T12:00:00" or "(no video metadata)").
std::string getVideoTimeInfoString(const std::string& filePath);

}  // namespace filetimefixer
