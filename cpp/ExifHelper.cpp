#include "ExifHelper.h"
#include "TimeConvert.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace filetimefixer {

// Path string that Exiv2 can open. On Windows without EXIV2_ENABLE_WIN_UNICODE, fopen() expects ACP.
static std::string pathForExiv2(const std::string& filepath) {
#ifdef _WIN32
    std::filesystem::path p(filepath);
    std::wstring wpath = p.wstring();
    int n = WideCharToMultiByte(CP_ACP, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return filepath;
    std::string result(static_cast<size_t>(n) - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, wpath.c_str(), -1, &result[0], n, nullptr, nullptr);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
#else
    return filepath;
#endif
}

static const std::vector<std::string>& exifTimeTags() {
    static const std::vector<std::string> tags = {
        "Exif.Photo.DateTimeOriginal",
        "Exif.Photo.DateTimeDigitized",
        "Exif.Image.DateTime",
    };
    return tags;
}

#ifdef _WIN32
// Convert path to system code page for Exiv2 (fopen) when opening temp files.
static std::string pathToAcp(const std::filesystem::path& p) {
    std::wstring wpath = p.wstring();
    int n = WideCharToMultiByte(CP_ACP, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(static_cast<size_t>(n) - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, wpath.c_str(), -1, &result[0], n, nullptr, nullptr);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}
#endif

#ifdef _WIN32
// Exiv2 built without EXIV2_ENABLE_WIN_UNICODE uses fopen() and fails on UTF-8.
// Short path (8.3) in system code page usually works.
static std::string pathForExiv2WindowsShortPath(const std::string& filepath) {
    std::filesystem::path p(filepath);
    std::wstring wpath = p.wstring();
    wchar_t shortW[MAX_PATH] = {};
    DWORD len = GetShortPathNameW(wpath.c_str(), shortW, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, shortW, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(static_cast<size_t>(n) - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, shortW, -1, &result[0], n, nullptr, nullptr);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

// Open file via temp copy when direct path fails (e.g. Unicode path).
static bool getExifDataViaTemp(const std::string& filepath, Exiv2::ExifData& exifData) {
    namespace fs = std::filesystem;
    fs::path p(filepath);
    if (!fs::is_regular_file(p)) return false;
    fs::path tmpPath = fs::temp_directory_path() / ("ftf_read_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + p.extension().string());
    try {
        fs::copy_file(p, tmpPath, fs::copy_options::overwrite_existing);
        std::string pathToOpen = pathToAcp(tmpPath);
        if (pathToOpen.empty()) pathToOpen = pathForExiv2(tmpPath.string());
        auto image = Exiv2::ImageFactory::open(pathToOpen);
        if (!image.get()) { fs::remove(tmpPath); return false; }
        image->readMetadata();
        exifData = image->exifData();
        fs::remove(tmpPath);
        return true;
    } catch (...) {
        try { fs::remove(tmpPath); } catch (...) {}
        return false;
    }
}

// Read file into memory and open via MemIo so Exiv2 never sees a path (works when path/open fails).
static bool getExifDataViaMemIo(const std::string& filepath, Exiv2::ExifData& exifData) {
    namespace fs = std::filesystem;
    fs::path p(filepath);
    if (!fs::is_regular_file(p)) return false;
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamsize sz = in.tellg();
    if (sz <= 0 || sz > 100 * 1024 * 1024) return false;  // cap 100MB
    in.seekg(0);
    std::vector<Exiv2::byte> buf(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(buf.data()), sz)) return false;
    in.close();
    try {
        Exiv2::BasicIo::UniquePtr io(new Exiv2::MemIo(buf.data(), buf.size()));
        auto image = Exiv2::ImageFactory::open(std::move(io));
        if (!image.get()) return false;
        image->readMetadata();
        exifData = image->exifData();
        return true;
    } catch (const Exiv2::Error&) {
        return false;
    }
}

// Modify EXIF by reading file into MemIo, so Exiv2 never sees a path.
static bool modifyExifDataForTimeViaMemIo(const std::string& filepath, const std::string& exifValue) {
    namespace fs = std::filesystem;
    fs::path p(filepath);
    if (!fs::is_regular_file(p)) return false;
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamsize sz = in.tellg();
    if (sz <= 0 || sz > 100 * 1024 * 1024) return false;
    in.seekg(0);
    std::vector<Exiv2::byte> buf(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(buf.data()), sz)) return false;
    in.close();
    try {
        Exiv2::BasicIo::UniquePtr io(new Exiv2::MemIo(buf.data(), buf.size()));
        auto image = Exiv2::ImageFactory::open(std::move(io));
        if (!image.get()) return false;
        image->readMetadata();
        Exiv2::ExifData& exifData = image->exifData();
        for (const auto& tag : exifTimeTags()) {
            Exiv2::ExifKey key(tag);
            auto pos = exifData.findKey(key);
            if (pos != exifData.end()) {
                pos->setValue(exifValue);
            } else {
                auto value = Exiv2::Value::create(Exiv2::asciiString);
                value->read(exifValue);
                exifData.add(key, value.get());
            }
        }
        image->writeMetadata();
        Exiv2::BasicIo& bio = image->io();
        if (bio.seek(0, Exiv2::BasicIo::beg) != 0) return false;
        Exiv2::DataBuf data = bio.read(bio.size());
        if (data.size() == 0) return false;
        std::ofstream out(p, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(data.c_data()), data.size());
        return out.good();
    } catch (const Exiv2::Error&) {
        return false;
    }
}
#endif

static bool s_exiv2ErrorLogged = false;
static void logExiv2ErrorOnce(const char* msg) {
    if (!s_exiv2ErrorLogged) {
        std::cerr << "Exiv2: " << msg << " (EXIF read/write may be skipped for some files on this system.)" << std::endl;
        s_exiv2ErrorLogged = true;
    }
}

bool getExifData(const std::string& filepath, Exiv2::ExifData& exifData) {
    auto tryOpen = [&](const std::string& pathToOpen) -> bool {
        try {
            auto image = Exiv2::ImageFactory::open(pathToOpen);
            if (!image.get()) return false;
            try { image->readMetadata(); }
            catch (const Exiv2::Error& e) { logExiv2ErrorOnce(e.what()); return false; }
            exifData = image->exifData();
            return true;
        } catch (const Exiv2::Error& e) {
            (void)e;
            return false;
        }
    };

#ifdef _WIN32
    // On Windows, path-based open can trigger abort() in Debug (Exiv2/vcpkg). Try MemIo first so Exiv2 never sees a path.
    if (getExifDataViaMemIo(filepath, exifData))
        return true;
    if (tryOpen(pathForExiv2(filepath)))
        return true;
    logExiv2ErrorOnce("Direct path failed, trying short path or temp copy");
    std::string shortPath = pathForExiv2WindowsShortPath(filepath);
    if (!shortPath.empty() && tryOpen(shortPath))
        return true;
    if (getExifDataViaTemp(filepath, exifData))
        return true;
#else
    if (tryOpen(pathForExiv2(filepath)))
        return true;
#endif
    logExiv2ErrorOnce("Unable to open file");
    return false;
}

std::string getExifTimeEarliest(const std::string& filePath) {
    Exiv2::ExifData exifData;
    if (!getExifData(filePath, exifData)) return "";
    std::string earliestTime;
    for (const auto& tag : exifTimeTags()) {
        Exiv2::ExifKey key(tag);
        auto pos = exifData.findKey(key);
        if (pos != exifData.end()) {
            std::string timeStr = pos->toString();
            if (earliestTime.empty() || timeStr < earliestTime)
                earliestTime = timeStr;
        }
    }
    return earliestTime;
}

// EXIF DateTime* tags require format "YYYY:MM:DD HH:MM:SS" (colons in date).
std::string formatTimeForExif(const std::string& timeStr) {
    std::string out = timeStr;
    if (out.size() >= 10 && out[4] == '-' && out[7] == '-') {
        out[4] = ':';
        out[7] = ':';
    }
    if (out.size() > 10 && out[10] == 'T')
        out[10] = ' ';
    return out;
}

static bool modifyExifDataForTimeImpl(const std::string& pathToOpen, const std::string& exifValue) {
    try {
        auto image = Exiv2::ImageFactory::open(pathToOpen);
        if (!image.get()) return false;
        image->readMetadata();
        Exiv2::ExifData& exifData = image->exifData();
        for (const auto& tag : exifTimeTags()) {
            Exiv2::ExifKey key(tag);
            auto pos = exifData.findKey(key);
            if (pos != exifData.end()) {
                pos->setValue(exifValue);
            } else {
                auto value = Exiv2::Value::create(Exiv2::asciiString);
                value->read(exifValue);
                exifData.add(key, value.get());
            }
        }
        image->writeMetadata();
        return true;
    } catch (const Exiv2::Error&) {
        return false;
    }
}

bool modifyExifDataForTime(const std::string& filepath, const std::string& new_datetime) {
    std::string exifValue = formatTimeForExif(new_datetime);
#ifdef _WIN32
    // Prefer MemIo on Windows to avoid path-based open triggering abort() in Debug.
    if (modifyExifDataForTimeViaMemIo(filepath, exifValue))
        return true;
    if (modifyExifDataForTimeImpl(pathForExiv2(filepath), exifValue))
        return true;
    std::string shortPath = pathForExiv2WindowsShortPath(filepath);
    if (!shortPath.empty() && modifyExifDataForTimeImpl(shortPath, exifValue))
        return true;
    logExiv2ErrorOnce("Short path failed, trying via temp file");
    std::filesystem::path p(filepath);
    auto tmpPath = std::filesystem::temp_directory_path() / ("ftf_exif_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + p.extension().string());
    try {
        std::filesystem::copy_file(p, tmpPath, std::filesystem::copy_options::overwrite_existing);
        bool ok = false;
        std::string pathToOpen = pathToAcp(tmpPath);
        if (pathToOpen.empty()) pathToOpen = pathForExiv2(tmpPath.string());
        if (modifyExifDataForTimeImpl(pathToOpen, exifValue)) {
            std::filesystem::copy_file(tmpPath, p, std::filesystem::copy_options::overwrite_existing);
            ok = true;
        }
        std::filesystem::remove(tmpPath);
        if (ok) return true;
    } catch (...) {
        try { std::filesystem::remove(tmpPath); } catch (...) {}
    }
    return false;
#else
    if (modifyExifDataForTimeImpl(pathForExiv2(filepath), exifValue))
        return true;
    logExiv2ErrorOnce("Failed to open file");
    return false;
#endif
}

std::string getExifTimeInfoString(const std::string& filePath) {
    Exiv2::ExifData exifData;
    if (!getExifData(filePath, exifData)) return "(EXIF read failed)";
    std::string out;
    for (const auto& tag : exifTimeTags()) {
        Exiv2::ExifKey key(tag);
        auto pos = exifData.findKey(key);
        if (pos != exifData.end()) {
            if (!out.empty()) out += "; ";
            out += tag;
            out += "=";
            out += pos->toString();
        }
    }
    return out.empty() ? "(no EXIF time tags)" : out;
}

void printExifTime(const std::string& filePath) {
    Exiv2::ExifData exifData;
    getExifData(filePath, exifData);
    for (const auto& tag : exifTimeTags()) {
        Exiv2::ExifKey key(tag);
        auto pos = exifData.findKey(key);
        if (pos != exifData.end())
            std::cout << tag << ": " << pos->toString() << std::endl;
    }
}

}  // namespace filetimefixer
