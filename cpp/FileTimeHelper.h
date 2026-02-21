#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace filetimefixer {

// Set file creation/access/modification time (Windows) or mtime (Linux/Mac) to UTC corresponding to timeStr
bool setFileTimesToTargetTime(const fs::path& filepath, const std::string& timeStr);

void printPosixFileTimes(const std::string& filename);

bool renameFile(const std::string& oldName, const std::string& newName);

}  // namespace filetimefixer
