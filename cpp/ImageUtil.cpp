#include "ImageUtil.h"
#include <algorithm>
#include <string>
#include <vector>

namespace filetimefixer {

bool isImageFile(const fs::path& filePath) {
    static const std::vector<std::string> imageExtensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp", ".heic", ".raw"
    };
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(imageExtensions.begin(), imageExtensions.end(), ext) != imageExtensions.end();
}

}  // namespace filetimefixer
