#include "TimeParse.h"
#include <regex>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#include <time.h>
#else
#include <ctime>
#endif

namespace filetimefixer {

bool isValidDate(const std::string& dateStr) {
    if (dateStr.length() != 8) return false;
    int year = std::stoi(dateStr.substr(0, 4));
    int month = std::stoi(dateStr.substr(4, 2));
    int day = std::stoi(dateStr.substr(6, 2));
    if (month < 1 || month > 12) return false;
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0)))
        daysInMonth[1] = 29;
    return day >= 1 && day <= daysInMonth[month - 1];
}

bool isValidTime(const std::string& timeStr) {
    if (timeStr.length() != 6) return false;
    int hour = std::stoi(timeStr.substr(0, 2));
    int minute = std::stoi(timeStr.substr(2, 2));
    int second = std::stoi(timeStr.substr(4, 2));
    return (hour >= 0 && hour < 24) && (minute >= 0 && minute < 60) && (second >= 0 && second < 60);
}

// UTC ms timestamp -> Beijing time (UTC+8), portable (no gmtime/locale)
static void utcSecondsToYMDHMS(int64_t utcSeconds, int& y, int& mo, int& d, int& h, int& mi, int& s) {
    const int64_t SEC_PER_DAY = 86400;
    int64_t day = utcSeconds / SEC_PER_DAY;
    int64_t secInDay = utcSeconds % SEC_PER_DAY;
    if (secInDay < 0) { secInDay += SEC_PER_DAY; --day; }
    h = static_cast<int>(secInDay / 3600);
    mi = static_cast<int>((secInDay % 3600) / 60);
    s = static_cast<int>(secInDay % 60);
    for (y = 1970; ; ++y) {
        int daysThisYear = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
        if (day < daysThisYear) break;
        day -= daysThisYear;
    }
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) daysInMonth[1] = 29;
    mo = 0;
    while (mo < 12 && day >= daysInMonth[mo]) { day -= daysInMonth[mo]; ++mo; }
    d = static_cast<int>(day) + 1;
    mo += 1;
}

std::string timestampToBeijingTime(int64_t timestamp, bool isMilliseconds) {
    if (!isMilliseconds) timestamp *= 1000;
    int64_t seconds = timestamp / 1000;
    int ms = static_cast<int>(timestamp % 1000);
    if (ms < 0) { ms += 1000; seconds -= 1; }
    int64_t beijingSeconds = seconds + 8 * 3600;
    int y, mo, d, h, mi, s;
    utcSecondsToYMDHMS(beijingSeconds, y, mo, d, h, mi, s);
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(4) << y << "-" << std::setw(2) << mo << "-" << std::setw(2) << d
       << " " << std::setw(2) << h << ":" << std::setw(2) << mi << ":" << std::setw(2) << s
       << "." << std::setw(3) << ms;
    return ss.str();
}

std::string parseFileNameTime(const std::string& filename) {
    std::smatch match;

    std::regex pattern1(R"((\d{8})[_-](\d{6}))");
    if (std::regex_search(filename, match, pattern1) && isValidDate(match[1].str()) && isValidTime(match[2].str())) {
        return match[1].str().substr(0, 4) + "-" + match[1].str().substr(4, 2) + "-" + match[1].str().substr(6, 2)
            + " " + match[2].str().substr(0, 2) + ":" + match[2].str().substr(2, 2) + ":" + match[2].str().substr(4, 2);
    }

    // ptYYYY_MM_DD_HH_MM_SS (e.g. pt2021_10_23_21_52_39.jpg)
    std::regex patternPt(R"(pt(\d{4})_(\d{2})_(\d{2})_(\d{2})_(\d{2})_(\d{2}))");
    if (std::regex_search(filename, match, patternPt)) {
        std::string yyyymmdd = match[1].str() + match[2].str() + match[3].str();
        std::string hhmmss = match[4].str() + match[5].str() + match[6].str();
        if (isValidDate(yyyymmdd) && isValidTime(hhmmss)) {
            return match[1].str() + "-" + match[2].str() + "-" + match[3].str()
                + " " + match[4].str() + ":" + match[5].str() + ":" + match[6].str();
        }
    }

    // Screenshot_YYYY-MM-DD-HH-MM-SS[-...] (e.g. Screenshot_2021-03-25-01-12-43-235_com.tencent.mm.jpg)
    std::regex patternScreenshot(R"(Screenshot_(\d{4})-(\d{2})-(\d{2})-(\d{2})-(\d{2})-(\d{2}))");
    if (std::regex_search(filename, match, patternScreenshot)) {
        std::string yyyymmdd = match[1].str() + match[2].str() + match[3].str();
        std::string hhmmss = match[4].str() + match[5].str() + match[6].str();
        if (isValidDate(yyyymmdd) && isValidTime(hhmmss)) {
            return match[1].str() + "-" + match[2].str() + "-" + match[3].str()
                + " " + match[4].str() + ":" + match[5].str() + ":" + match[6].str();
        }
    }

    std::regex pattern2(R"((\d{8}))");
    if (filename.rfind("mmexport", 0) != 0 && std::regex_search(filename, match, pattern2) && isValidDate(match[1].str())) {
        return match[1].str().substr(0, 4) + "-" + match[1].str().substr(4, 2) + "-" + match[1].str().substr(6, 2);
    }

    std::regex pattern3(R"((\d{10}|\d{13})(?=\.\w+$))");
    if (std::regex_search(filename, match, pattern3)) {
        int64_t ts = std::stoll(match[1].str());
        bool isMs = (match[1].length() == 13);
        std::string strTime = timestampToBeijingTime(ts, isMs);
        std::string str(strTime);
        str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
        if (str.length() >= 8 && isValidDate(str.substr(0, 8))) {
            return strTime;
        }
        if (strTime.rfind('.') != std::string::npos && strTime.rfind('.') >= 13 && filename.rfind("mmexport", 0) == 0) {
            size_t dot = strTime.rfind('.');
            std::string sub = strTime.substr(dot - 13, 13);
            if (sub.length() == 13) {
                int64_t subTs = std::stoll(sub);
                return timestampToBeijingTime(subTs, isMs);
            }
        }
    }
    return "";
}

}  // namespace filetimefixer
