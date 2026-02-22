#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace filetimefixer {

bool isImageFile(const fs::path& filePath);
bool isVideoFile(const fs::path& filePath);
/// True if file is an image or video we can process (rename + fix time).
bool isMediaFile(const fs::path& filePath);

}  // namespace filetimefixer
