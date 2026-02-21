#include "TimeConvert.h"
#include "TimeParse.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <time.h>
#else
#include <ctime>
#endif

namespace filetimefixer {

bool parseUTCStringToTm(std::tm& tm, const std::string& utcTimeStr) {
    std::istringstream ss(utcTimeStr);
    if (utcTimeStr.empty()) return false;
    if (utcTimeStr.find('T') != std::string::npos && utcTimeStr.find('-') != std::string::npos) {
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (!ss.fail()) return true;
    }
    if (utcTimeStr.find('-') != std::string::npos) {
        ss.clear();
        ss.str(utcTimeStr);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) return true;
    }
    if (utcTimeStr.find(':') != std::string::npos) {
        ss.clear();
        ss.str(utcTimeStr);
        ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
        if (!ss.fail()) return true;
    }
    return false;
}

std::time_t utcStringToTimestamp(const std::string& timeStr) {
    std::tm tm = {};
    if (!parseUTCStringToTm(tm, timeStr)) return static_cast<time_t>(-1);
    tm.tm_isdst = 0;
#ifdef _WIN32
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}

std::string timestampToUTCString(std::time_t timestamp) {
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &timestamp);
#else
    gmtime_r(&timestamp, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::string exifDateTimeToUTCString(const std::string& exifDateTime) {
    std::tm tm = {};
    if (!parseUTCStringToTm(tm, exifDateTime)) return "";
    tm.tm_isdst = -1;
    std::time_t localTime = std::mktime(&tm);
    localTime += 8 * 3600;
    if (localTime == -1) return "";
    std::ostringstream utcSs;
#ifdef _WIN32
    std::tm utcTm = {};
    if (gmtime_s(&utcTm, &localTime) != 0) return "";
    utcSs << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%S");
#else
    std::tm* utcTmP = std::gmtime(&localTime);
    if (!utcTmP) return "";
    utcSs << std::put_time(utcTmP, "%Y-%m-%dT%H:%M:%S");
#endif
    return utcSs.str();
}

std::string formatTimeToUTC8Name(const std::string& timeStr) {
    std::tm tm = {};
    if (!parseUTCStringToTm(tm, timeStr)) return "";
    tm.tm_isdst = -1;
    std::time_t localTime = std::mktime(&tm);
    if (localTime == -1) return "";
    localTime += 8 * 3600;
    std::ostringstream utcPlus8Ss;
#ifdef _WIN32
    std::tm utcPlus8Tm = {};
    if (gmtime_s(&utcPlus8Tm, &localTime) != 0) return "";
    utcPlus8Ss << std::put_time(&utcPlus8Tm, "%Y%m%d_%H%M%S");
#else
    std::tm* utcPlus8Tm = std::gmtime(&localTime);
    if (!utcPlus8Tm) return "";
    utcPlus8Ss << std::put_time(utcPlus8Tm, "%Y%m%d_%H%M%S");
#endif
    if (timeStr.length() >= 23) {
        std::string msStr = timeStr.substr(20, 3);
        int ms = std::stoi(msStr);
        utcPlus8Ss << "_" << std::setw(3) << std::setfill('0') << ms;
    }
    return utcPlus8Ss.str();
}

std::string supplementDateWithCurrentUtcTime(const std::string& timeStr) {
    if (timeStr.empty() || timeStr.length() > 10) return timeStr;
    std::time_t now = std::time(nullptr);
    std::string utc = timestampToUTCString(now);
    if (utc.length() < 19) return timeStr;
    return timeStr + "T" + utc.substr(11, 8);
}

}  // namespace filetimefixer
