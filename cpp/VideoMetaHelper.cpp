#include "VideoMetaHelper.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace filetimefixer {

namespace {

/// Run a command and return stdout as string. Returns empty on failure or if command not found.
std::string runCommand(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "";
    std::string result;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) != nullptr)
        result += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

/// Quote path for shell (escape quotes, wrap in double quotes).
std::string quotePath(const std::string& path) {
    std::string out = "\"";
    for (char c : path) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

/// Normalize ffprobe creation_time (e.g. "2023-10-23T12:00:00.000000Z") to "YYYY-MM-DDTHH:MM:SS".
std::string normalizeCreationTime(const std::string& s) {
    std::string t = s;
    while (!t.empty() && (t.back() == '\r' || t.back() == '\n' || t.back() == ' '))
        t.pop_back();
    if (t.size() < 19) return "";
    t = t.substr(0, 19);
    if (t[10] != 'T' && t[10] != ' ') return "";
    if (t[10] == ' ') t[10] = 'T';
    return t;
}

}  // namespace

std::string getVideoCreationTimeUtc(const std::string& filePath) {
    if (filePath.empty()) return "";
    std::string qpath = quotePath(filePath);
    std::string cmd = "ffprobe -v error -show_entries format_tags=creation_time -of default=noprint_wrappers=1:nokey=1 " + qpath;
    std::string out = runCommand(cmd);
    return normalizeCreationTime(out);
}

bool setVideoCreationTime(const std::string& filePath, const std::string& targetTimeUtc) {
    if (filePath.empty() || targetTimeUtc.size() < 19) return false;
    std::string timeForFfmpeg = targetTimeUtc.substr(0, 19);
    if (timeForFfmpeg[10] == ' ') timeForFfmpeg[10] = 'T';
    fs::path p(filePath);
    if (!fs::exists(p) || !fs::is_regular_file(p)) return false;

    fs::path dir = p.parent_path();
    fs::path tempPath = dir / (p.stem().string() + "_ftf_tmp" + p.extension().string());

    std::string qpath = quotePath(filePath);
    std::string qtemp = quotePath(tempPath.string());
    std::string qtime = quotePath(timeForFfmpeg);

#ifdef _WIN32
    std::string cmd = "ffmpeg -y -i " + qpath + " -c copy -movflags use_metadata_tags -metadata creation_time=" + qtime + " " + qtemp + " 2>nul";
#else
    std::string cmd = "ffmpeg -y -i " + qpath + " -c copy -movflags use_metadata_tags -metadata creation_time=" + qtime + " " + qtemp + " 2>/dev/null";
#endif
    int ret = std::system(cmd.c_str());
    if (ret != 0 || !fs::exists(tempPath) || fs::file_size(tempPath) == 0) {
        if (fs::exists(tempPath)) fs::remove(tempPath);
        return false;
    }
    try {
        fs::remove(p);
        fs::rename(tempPath, p);
    } catch (...) {
        if (fs::exists(tempPath)) fs::remove(tempPath);
        return false;
    }
    return true;
}

std::string getVideoTimeInfoString(const std::string& filePath) {
    std::string ct = getVideoCreationTimeUtc(filePath);
    if (ct.empty()) return "(no video metadata)";
    return "creation_time=" + ct;
}

}  // namespace filetimefixer
