#pragma once

#include <string>

namespace filetimefixer {

// Target time resolution scenario (avoid enum value "None" - macro on some Windows builds)
enum class TargetTimeScenario {
    NoTime,                   // unable to resolve
    NameOnly,
    ExifOnly,
    BothUseEarliest,
    ExifTooOldUseName,
    SameDayExifMidnightUseName,
    SameDayNameMidnightUseExif,
    SameDayBothFullUseMorePrecise,
    SameDayNameDateOnlyUseExif,   // name has date only, exif has time -> use exif
    SameDayExifDateOnlyUseName    // exif has date only, name has time -> use name
};

struct ResolveResult {
    std::string targetTime;
    TargetTimeScenario scenario = TargetTimeScenario::NoTime;
};

// Resolve target time and scenario from nameTime and exifTime (both in normalized format)
ResolveResult resolveTargetTime(const std::string& nameTime, const std::string& exifTime);

const char* scenarioName(TargetTimeScenario s);

}  // namespace filetimefixer
