#include "TimeParse.h"
#include "TimeConvert.h"
#include "TargetTimeResolver.h"
#include "ExifHelper.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

namespace {

struct FileNameTestCase {
    std::string filename;
    std::string expectedTime;  // Empty means expect parse failure
};

struct ResolverTestCase {
    std::string nameTime;
    std::string exifTime;
    std::string expectedTargetTime;
    filetimefixer::TargetTimeScenario expectedScenario;
};

// Aligned with test_spec/time_parse.yaml and test_spec/target_resolver.yaml
void runFileNameTests() {
    std::cout << "\n========== File name time parse (ParseFileNameTime) ==========\n" << std::endl;
    std::vector<FileNameTestCase> cases = {
        { "20160331_202334.jpg", "2016-03-31 20:23:34" },
        { "IMG_20231111_193849.jpg", "2023-11-11 19:38:49" },
        { "VID_20210801_171003.jpg", "2021-08-01 17:10:03" },
        { "PANO_20231001_143241.jpg", "2023-10-01 14:32:41" },
        { "MTXX_PT20230623_190638417.jpg", "2023-06-23 19:06:38" },
        { "mmexport1568301595980.jpg", "2019-09-12 23:19:55.980" },
        { "mmexport1602999370599.jpg", "2020-10-18 13:36:10.599" },
        { "MEITU_20240807_123043882.jpg", "2024-08-07 12:30:43" },
        { "wx_camera_1719390504866.jpg", "2024-06-26 16:28:24.866" },
        { "1605199092110.jpeg", "2020-11-13 00:38:12.110" },
        { "20220115-wczt.jpg", "2022-01-15" },
        { "l00972450_1543624986659.jpg", "2018-12-01 08:43:06.659" },
        { "20220115.jpg", "2022-01-15" },
        { "mmexport1620111487858.jpg", "2021-05-04 14:58:07.858" },
        { "nonsense.txt", "" },
        { "no_digits_here.png", "" },
    };

    int passed = 0, failed = 0;
    for (const auto& c : cases) {
        std::string got = filetimefixer::parseFileNameTime(c.filename);
        bool ok = (got == c.expectedTime);
        if (ok) ++passed; else ++failed;
        std::cout << (ok ? "[PASS]" : "[FAIL]") << " " << std::setw(50) << std::left << c.filename
                  << " => " << (got.empty() ? "(empty)" : got);
        if (!ok) std::cout << "  (expected: " << (c.expectedTime.empty() ? "(empty)" : c.expectedTime) << ")";
        std::cout << std::endl;
    }
    std::cout << "\nFileName tests: " << passed << " passed, " << failed << " failed.\n" << std::endl;
}

void runResolverTests() {
    std::cout << "\n========== Target time resolver (ResolveTargetTime) ==========\n" << std::endl;
    std::vector<ResolverTestCase> cases = {
        { "", "", "", filetimefixer::TargetTimeScenario::NoTime },
        { "2023-10-23 15:30:00", "", "2023-10-23 15:30:00", filetimefixer::TargetTimeScenario::NameOnly },
        { "", "2023-10-23T14:00:00", "2023-10-23T14:00:00", filetimefixer::TargetTimeScenario::ExifOnly },
        { "2023-10-23 15:30:00", "2023-10-23T14:00:00", "2023-10-23T14:00:00", filetimefixer::TargetTimeScenario::BothUseEarliest },
        { "2023-10-23 10:00:00", "2023-10-23T15:00:00", "2023-10-23 10:00:00", filetimefixer::TargetTimeScenario::BothUseEarliest },
        { "2023-10-23 12:00:00", "2009-06-01T12:00:00", "2023-10-23 12:00:00", filetimefixer::TargetTimeScenario::ExifTooOldUseName },
        { "2023-10-23 15:30:00", "2023-10-23T00:00:00", "2023-10-23 15:30:00", filetimefixer::TargetTimeScenario::SameDayExifMidnightUseName },
        { "2023-10-23 00:00:00", "2023-10-23T14:30:00", "2023-10-23T14:30:00", filetimefixer::TargetTimeScenario::SameDayNameMidnightUseExif },
        { "2023-10-23 14:30:00", "2023-10-23T14:30:00", "2023-10-23T14:30:00", filetimefixer::TargetTimeScenario::SameDayBothFullUseMorePrecise },
        { "2023-10-23 14:30:01", "2023-10-23T14:30:00", "2023-10-23 14:30:01", filetimefixer::TargetTimeScenario::SameDayBothFullUseMorePrecise },
    };

    int passed = 0, failed = 0;
    for (const auto& c : cases) {
        filetimefixer::ResolveResult r = filetimefixer::resolveTargetTime(c.nameTime, c.exifTime);
        bool okTime = (r.targetTime == c.expectedTargetTime);
        bool okScenario = (r.scenario == c.expectedScenario);
        bool ok = okTime && okScenario;
        if (ok) ++passed; else ++failed;
        std::cout << (ok ? "[PASS]" : "[FAIL]") << " name=\"" << (c.nameTime.empty() ? "(empty)" : c.nameTime)
                  << "\" exif=\"" << (c.exifTime.empty() ? "(empty)" : c.exifTime) << "\"\n"
                  << "       => " << (r.targetTime.empty() ? "(empty)" : r.targetTime)
                  << " [" << filetimefixer::scenarioName(r.scenario) << "]";
        if (!ok) {
            std::cout << "\n       expected => " << (c.expectedTargetTime.empty() ? "(empty)" : c.expectedTargetTime)
                      << " [" << filetimefixer::scenarioName(c.expectedScenario) << "]";
        }
        std::cout << std::endl;
    }
    std::cout << "\nResolver tests: " << passed << " passed, " << failed << " failed.\n" << std::endl;
}

// EXIF output format is "YYYY:MM:DD HH:MM:SS" (colons in date, T -> space)
void runExifFormatTests() {
    std::cout << "\n========== EXIF time format (formatTimeForExif) ==========\n" << std::endl;
    struct Case { std::string in; std::string expected; };
    std::vector<Case> cases = {
        { "2023-10-23 15:30:00", "2023:10:23 15:30:00" },
        { "2023-10-23T14:00:00", "2023:10:23 14:00:00" },
        { "2016-03-31 20:23:34", "2016:03:31 20:23:34" },
        { "2021-12-28 00:00:00", "2021:12:28 00:00:00" },
        { "2024-08-07 12:30:43", "2024:08:07 12:30:43" },
    };
    int passed = 0, failed = 0;
    for (const auto& c : cases) {
        std::string got = filetimefixer::formatTimeForExif(c.in);
        bool ok = (got == c.expected);
        if (ok) ++passed; else ++failed;
        std::cout << (ok ? "[PASS]" : "[FAIL]") << " " << std::setw(28) << std::left << c.in
                  << " => " << got;
        if (!ok) std::cout << "  (expected: " << c.expected << ")";
        std::cout << std::endl;
    }
    std::cout << "\nEXIF format tests: " << passed << " passed, " << failed << " failed.\n" << std::endl;
}

void printScenarioTable() {
    std::cout << "\n========== Target time resolver scenarios ==========\n" << std::endl;
    std::cout << "| Scenario | Description |" << std::endl;
    std::cout << "|----------|-------------|" << std::endl;
    std::cout << "| None | No time from filename or EXIF |" << std::endl;
    std::cout << "| NameOnly | Time from filename only |" << std::endl;
    std::cout << "| ExifOnly | Time from EXIF only |" << std::endl;
    std::cout << "| BothUseEarliest | Both present, use earlier |" << std::endl;
    std::cout << "| ExifTooOldUseName | EXIF before 2010-01-01, use name time |" << std::endl;
    std::cout << "| SameDayExifMidnightUseName | Same day, EXIF 00:00:00, use name |" << std::endl;
    std::cout << "| SameDayNameMidnightUseExif | Same day, name 00:00:00, use EXIF |" << std::endl;
    std::cout << "| SameDayBothFullUseMorePrecise | Same day, both with time, use more precise |" << std::endl;
    std::cout << std::endl;
}

}  // namespace

int runAllTests() {
    std::cout << "FileTimeFixer test run (aligned with test_spec/ for C++ and Python)" << std::endl;
    printScenarioTable();
    runFileNameTests();
    runResolverTests();
    runExifFormatTests();
    std::cout << "Done." << std::endl;
    return 0;
}
