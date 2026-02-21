#include "TargetTimeResolver.h"
#include "TimeConvert.h"
#include <algorithm>

namespace filetimefixer {

static std::string normalizeForCompare(std::string s) {
    if (s.size() > 10 && s[10] == ' ') s[10] = 'T';
    return s;
}

const char* scenarioName(TargetTimeScenario s) {
    switch (s) {
        case TargetTimeScenario::NoTime: return "None";
        case TargetTimeScenario::NameOnly: return "NameOnly";
        case TargetTimeScenario::ExifOnly: return "ExifOnly";
        case TargetTimeScenario::BothUseEarliest: return "BothUseEarliest";
        case TargetTimeScenario::ExifTooOldUseName: return "ExifTooOldUseName";
        case TargetTimeScenario::SameDayExifMidnightUseName: return "SameDayExifMidnightUseName";
        case TargetTimeScenario::SameDayNameMidnightUseExif: return "SameDayNameMidnightUseExif";
        case TargetTimeScenario::SameDayBothFullUseMorePrecise: return "SameDayBothFullUseMorePrecise";
        case TargetTimeScenario::SameDayNameDateOnlyUseExif: return "SameDayNameDateOnlyUseExif";
        case TargetTimeScenario::SameDayExifDateOnlyUseName: return "SameDayExifDateOnlyUseName";
        default: return "?";
    }
}

static bool hasDate(const std::string& s) {
    return s.length() >= 10 && s.find('-') != std::string::npos;
}

// True if string has time-of-day (e.g. "YYYY-MM-DDTHH:MM:SS" or "YYYY-MM-DD HH:MM:SS")
static bool hasTimeOfDay(const std::string& s) {
    return s.length() >= 19 && (s[10] == 'T' || s[10] == ' ');
}

// True if string is date-only (no precise time to minute)
static bool isDateOnly(const std::string& s) {
    if (s.length() <= 10) return true;
    if (s.length() >= 19 && s[10] == 'T' && s.substr(11, 8) == "00:00:00") return true;
    return false;
}

ResolveResult resolveTargetTime(const std::string& nameTime, const std::string& exifTime) {
    ResolveResult out;
    if (nameTime.empty() && exifTime.empty()) {
        out.scenario = TargetTimeScenario::NoTime;
        return out;
    }
    if (!nameTime.empty() && exifTime.empty()) {
        out.targetTime = nameTime;
        out.scenario = TargetTimeScenario::NameOnly;
        return out;
    }
    if (nameTime.empty() && !exifTime.empty()) {
        out.targetTime = exifTime;
        out.scenario = TargetTimeScenario::ExifOnly;
        return out;
    }
    // Both non-empty
    if (!hasDate(nameTime) || !hasDate(exifTime)) {
        out.targetTime = hasDate(nameTime) ? nameTime : exifTime;
        out.scenario = hasDate(nameTime) ? TargetTimeScenario::NameOnly : TargetTimeScenario::ExifOnly;
        return out;
    }
    // Use name time when EXIF date is before 2010-01-01 (date part only)
    if (exifTime.substr(0, 10) < "2010-01-01") {
        out.targetTime = nameTime;
        out.scenario = TargetTimeScenario::ExifTooOldUseName;
        return out;
    }
    // Same day: prefer the more precise time (use Exif when name is date-only, use name when exif is date-only)
    if (nameTime.length() >= 10 && exifTime.length() >= 10 && nameTime.substr(0, 10) == exifTime.substr(0, 10)) {
        if (isDateOnly(nameTime) && hasTimeOfDay(exifTime)) {
            out.targetTime = exifTime;
            out.scenario = TargetTimeScenario::SameDayNameDateOnlyUseExif;
            return out;
        }
        if (isDateOnly(exifTime) && hasTimeOfDay(nameTime)) {
            out.targetTime = nameTime;
            out.scenario = TargetTimeScenario::SameDayExifDateOnlyUseName;
            return out;
        }
    }
    std::string nName = normalizeForCompare(nameTime);
    std::string nExif = normalizeForCompare(exifTime);
    out.targetTime = (nName <= nExif) ? nameTime : exifTime;
    out.scenario = TargetTimeScenario::BothUseEarliest;

    if (nameTime.length() >= 10 && exifTime.length() >= 10 && nameTime.substr(0, 10) == exifTime.substr(0, 10)) {
        bool exifMidnight = exifTime.length() >= 19 && exifTime.substr(11, 8) == "00:00:00";
        bool nameMidnight = nameTime.length() >= 19 && nameTime.substr(11, 8) == "00:00:00";
        if (exifMidnight) {
            out.targetTime = nameTime;
            out.scenario = TargetTimeScenario::SameDayExifMidnightUseName;
            return out;
        }
        if (nameMidnight) {
            out.targetTime = exifTime;
            out.scenario = TargetTimeScenario::SameDayNameMidnightUseExif;
            return out;
        }
        std::string nNameToMin = nName.substr(0, 16);
        std::string nExifToMin = nExif.substr(0, 16);
        if (nNameToMin == nExifToMin) {
            out.targetTime = (nName > nExif) ? nameTime : exifTime;
            out.scenario = TargetTimeScenario::SameDayBothFullUseMorePrecise;
        }
    }
    return out;
}

}  // namespace filetimefixer
