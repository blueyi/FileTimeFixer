#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace filetimefixer {

bool isImageFile(const fs::path& filePath);

}  // namespace filetimefixer
