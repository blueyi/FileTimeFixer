#include "ImageUtil.h"
#include <algorithm>
#include <string>
#include <vector>

namespace filetimefixer {

static bool hasExtension(const fs::path& filePath, const std::vector<std::string>& extensions) {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

bool isImageFile(const fs::path& filePath) {
    static const std::vector<std::string> imageExtensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp", ".heic", ".raw"
    };
    return hasExtension(filePath, imageExtensions);
}

bool isVideoFile(const fs::path& filePath) {
    static const std::vector<std::string> videoExtensions = {
        ".mp4", ".mov", ".avi", ".mkv", ".m4v", ".webm", ".wmv", ".3gp"
    };
    return hasExtension(filePath, videoExtensions);
}

bool isMediaFile(const fs::path& filePath) {
    return isImageFile(filePath) || isVideoFile(filePath);
}

}  // namespace filetimefixer
