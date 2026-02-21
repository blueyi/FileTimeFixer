#include <iostream>
#include <filesystem>
#include <vector>
#include <sys/stat.h>
#include <ctime>
#include <algorithm>
#ifdef _WIN32  
#include <io.h> // Use io.h for Windows as an alternative to unistd.h  
#else  
#include <unistd.h>  
#endif
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <exiv2/exiv2.hpp>
#include <regex>

// #define _WIN32
#ifdef _WIN32
#include <time.h>
#endif

#ifdef _WIN32
#include <windows.h> // Include for FILETIME and related Windows API functions
#endif

#ifndef F_OK  
#define F_OK 0 // Define F_OK if not already defined  
#endif


namespace fs = std::filesystem;

using namespace std;

// Check if an 8-digit date is valid (YYYYMMDD)
bool isValidDate(const string& dateStr) {
    if (dateStr.length() != 8) return false;
    int year = stoi(dateStr.substr(0, 4));
    int month = stoi(dateStr.substr(4, 2));
    int day = stoi(dateStr.substr(6, 2));

    if (month < 1 || month > 12) return false;
    int daysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))) daysInMonth[1] = 29;
    return day >= 1 && day <= daysInMonth[month - 1];
}

// Check if a 6-digit time is valid (HHMMSS)
bool isValidTime(const std::string& timeStr) {
    if (timeStr.length() != 6) return false;
    int hour = stoi(timeStr.substr(0, 2));
    int minute = stoi(timeStr.substr(2, 2));
    int second = stoi(timeStr.substr(4, 2));
    return (hour >= 0 && hour < 24) && (minute >= 0 && minute < 60) && (second >= 0 && second < 60);
}

// Convert timestamp to Beijing Time (UTC+8), example: 2023-10-23 15:30:00.123
std::string timestampToBeijingTime(int64_t timestamp, bool isMilliseconds) {
    if (!isMilliseconds) timestamp *= 1000;  // Convert seconds to milliseconds
    time_t seconds = timestamp / 1000;
    int ms = timestamp % 1000;

    struct tm tm_utc;
#ifdef _WIN32
    gmtime_s(&tm_utc, &seconds);  // Use gmtime_s on Windows
#else
    gmtime_r(&seconds, &tm_utc);  // Use gmtime_r on other platforms
#endif
    tm_utc.tm_hour += 8;          // Add 8-hour offset directly
    mktime(&tm_utc);              // Normalize time (automatically handle overflow)

    stringstream ss;
    ss << put_time(&tm_utc, "%Y-%m-%d %H:%M:%S") << "." << setw(3) << setfill('0') << ms;
    return ss.str();
}
// Main function: Extract date and time from the file name
std::string ParseFileNameTime(const string& filename) {
    smatch match;

    // Pattern 1: Match 8-digit date and 6-digit time (e.g., 20231111_193849)
    std::regex pattern1(R"((\d{8})[_-](\d{6}))");
    if (regex_search(filename, match, pattern1) && isValidDate(match[1]) && isValidTime(match[2])) {
        return match[1].str().substr(0, 4) + "-" + match[1].str().substr(4, 2) + "-" + match[1].str().substr(6, 2)
            + " " + match[2].str().substr(0, 2) + ":" + match[2].str().substr(2, 2) + ":" + match[2].str().substr(4, 2);
    }

    // Pattern 2: Match standalone 8-digit date (e.g., 20220115)
    std::regex pattern2(R"((\d{8}))");
    if (!(filename.rfind("mmexport", 0) == 0) && regex_search(filename, match, pattern2) && isValidDate(match[1])) {
        return match[1].str().substr(0, 4) + "-" + match[1].str().substr(4, 2) + "-" + match[1].str().substr(6, 2);
    }

    // Pattern 3: Match 13-digit or 10-digit timestamp (e.g., 1568301595980)
    std::regex pattern3(R"((\d{10}|\d{13})(?=\.\w+$))");
    if (regex_search(filename, match, pattern3)) {
        int64_t ts = stoll(match[1]);
        bool isMs = (match[1].length() == 13);
        std::string strTime = timestampToBeijingTime(ts, isMs);
        std::string str(strTime);
        str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
        if (isValidDate(str.substr(0, 8))) {
            return strTime;
        }
        if (strTime.rfind('.') != std::string::npos && strTime.rfind('.') >= 13 && str.rfind("mmexport", 0) == 0) {
            strTime = strTime.substr(strTime.rfind('.') - 13, strTime.rfind('.'));
            return timestampToBeijingTime(stoll(strTime), isMs);
        }
    }
    std::cerr << "Unable to parse time from file name: " << filename << std::endl;
    return "";
}

void PrintPosixFileTimes(std::string& filename) {
    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) != 0) return;

    std::cout << "Last access time: " << ctime(&fileStat.st_atime)
        << "Last modification time: " << ctime(&fileStat.st_mtime)
        << "Metadata modification time: " << ctime(&fileStat.st_ctime);
}

bool RenameFile(std::string oldName, std::string newName) {
    if (access(oldName.c_str(), F_OK) != 0) {
        std::cerr << "File not exist: " << oldName << std::endl;
        return false;
    }

    if (oldName == newName)
    {
        std::cerr << "New name is the same as old name!" << std::endl;
        return false;
    }

    if (rename(oldName.c_str(), newName.c_str()) == 0) {
        std::cout << "Rename success: " << oldName << " -> " << newName << std::endl;
        return true;
    }

    return false;
}

// parse UTC time string to tm structure
// Example string formats: "2023-10-23T15:30:00Z", "2023-10-23 15:30:00", "2023:10:23 15:30:00"
bool ParseUTCStringTotm(std::tm& tm, const std::string& utcTimeStr) {
    std::istringstream ss(utcTimeStr);

    if (utcTimeStr.empty()) {
        std::cerr << "Empty UTC time string" << std::endl;
        return false;
    }
    

    if (utcTimeStr.find('T') != std::string::npos && utcTimeStr.find('-') != std::string::npos) { // Handle time format with 'T'
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (!ss.fail()) {
            return true;
        }
    }
    else if (utcTimeStr.find('-') != std::string::npos) {
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            return true;
        }
    }
    else if (utcTimeStr.find(':') != std::string::npos) {
        ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
        if (!ss.fail()) {
            return true;
        }
    }

    std::cerr << "Failed to parse UTC time string: " << utcTimeStr << std::endl;
    return false;
}

// Example string format: "2023-10-23T15:30:00Z" (ISO 8601)
time_t UTCStringToTimestamp(const std::string& timStr) {
    std::tm tm = {};
    if (ParseUTCStringTotm(tm, timStr)) return -1;

    // Convert to UTC timestamp
#ifdef _WIN32
    return _mkgmtime(&tm); // Windows-specific function
#else
    return timegm(&tm); // Linux and other platforms
#endif
}

// Convert time_t timestamp to UTC string, example: "2023-10-23T15:30:00"
std::string TimestampToUTCString(time_t timestamp) {
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &timestamp); // Use gmtime_s on Windows
#else
    gmtime_r(&timestamp, &tm); // Use gmtime_r on other platforms
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Modify file creation date for windows, last write time for linux and windows
bool ModifyFileCreationDate(const fs::path& filepath, const std::string& timeStr) {
    // Parse the time string into a tm structure
    std::tm tm = {};
    if (!ParseUTCStringTotm(tm, timeStr)) {
        std::cerr << "Failed to parse time string: " << timeStr << std::endl;
        return false;
    }

    tm.tm_isdst = 0;  // Disable daylight saving time

    // Convert to UTC timestamp (cross-platform handling)
    time_t timestamp;
#if defined(_WIN32)
    timestamp = _mkgmtime(&tm);  // Windows-specific UTC conversion

    // time_t to FILETIME
    FILETIME ftCreate, ftAccess, ftWrite;
    {
        LONGLONG ll = Int32x32To64(timestamp, 10000000) + 116444736000000000;
        ftCreate.dwLowDateTime = (DWORD)ll;
        ftCreate.dwHighDateTime = ll >> 32;
        ftAccess = ftWrite = ftCreate;  // Set all times to the same value
    }

    // Open the file and set the creation time with WIN32 API
    HANDLE hFile = CreateFileW(filepath.c_str(), FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Windows CreateFile failed: " << GetLastError() << std::endl;
        return false;
    }

    BOOL result = SetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
    CloseHandle(hFile);

    if (!result) {
        std::cerr << "SetFileTime failed: " << GetLastError() << std::endl;
        return false;
    }

#else
    // Temporarily set the timezone to UTC on Linux/macOS
    char* tz = std::getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    timestamp = std::mktime(&tm);
    if (tz) {
        setenv("TZ", tz, 1);
    }
    else {
        unsetenv("TZ");
    }
    tzset();
#endif

    //  Handle epoch differences
    auto sys_time = std::chrono::system_clock::from_time_t(timestamp);
    auto sys_duration = sys_time.time_since_epoch();

    // Windows requires compensation for the epoch difference between 1601 and 1970
#if defined(_WIN32)
    constexpr auto windows_epoch_diff =
        std::chrono::seconds(11644473600); // Seconds difference between 1601 and 1970
    auto file_duration =
        std::chrono::duration_cast<fs::file_time_type::duration>(
            sys_duration + windows_epoch_diff);
#else
    auto file_duration =
        std::chrono::duration_cast<fs::file_time_type::duration>(sys_duration);
#endif

    fs::file_time_type file_time(file_duration);
    // Modify the file creation time
    fs::last_write_time(filepath, file_time);
    return true;
}

bool GetExifData(const std::string& filepath, Exiv2::ExifData& exifData) {
    try {
        // 1. Open the image file
        auto image = Exiv2::ImageFactory::open(filepath);
        if (image.get() == nullptr) {
            std::cerr << "Unable to open file: " << filepath << std::endl;
            return false;
        }

        // 2. Read metadata
        try {
            image->readMetadata();
        }
        catch (const Exiv2::Error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        exifData = image->exifData();
        return true;
    }
    catch (const Exiv2::Error& e) {
        std::cerr << "Exiv2 error: " << e.what() << std::endl;
        return false;
    }
}

bool ModifyExifDataForTime(const std::string& filepath, const std::string& new_datetime) {
    try {
        // 1. Open the image file
        auto image = Exiv2::ImageFactory::open(filepath);
        if (image.get() == nullptr) {
            std::cerr << "Unable to open file: " << filepath << std::endl;
            return false;
        }

        // 2. Read metadata
        image->readMetadata();
        Exiv2::ExifData& exifData = image->exifData();

        // 3. Modify capture time tags
        const std::vector<std::string> timeTags = {
            "Exif.Photo.DateTimeOriginal",
            "Exif.Photo.DateTimeDigitized",
            "Exif.Image.DateTime",
        };
        for (const auto& tag : timeTags) {
            Exiv2::ExifKey key(tag);
            auto pos = exifData.findKey(key);
            if (pos != exifData.end()) {
                pos->setValue(new_datetime);
            }
            else {
                // Add new tag if it does not exist
                auto value = Exiv2::Value::create(Exiv2::asciiString);
                value->read(new_datetime);
                exifData.add(key, value.get());
            }
        }
        // 4. Write back metadata
        image->writeMetadata();
        return true;

    }
    catch (const Exiv2::Error& e) {
        std::cerr << "Exiv2 Error: " << e.what() << std::endl;
        return false;
    }
}

void PrintExifTime(const std::string& filePath) {
    Exiv2::ExifData exifData;
    GetExifData(filePath, exifData);
    const std::vector<std::string> timeTags = {
        "Exif.Photo.DateTimeOriginal",
        "Exif.Photo.DateTimeDigitized",
        "Exif.Image.DateTime",
    };

    for (const auto& tag : timeTags) {
        Exiv2::ExifKey key(tag);
        auto pos = exifData.findKey(key);
        if (pos != exifData.end()) {
            std::cout << tag << ": " << pos->toString() << std::endl;
        }
    }
}

std::string GetExifTimeEarliest(const std::string& filePath) {
    Exiv2::ExifData exifData;
    GetExifData(filePath, exifData);
    const std::vector<std::string> timeTags = {
        "Exif.Photo.DateTimeOriginal",
        "Exif.Photo.DateTimeDigitized",
        "Exif.Image.DateTime",
    };
    std::string earliestTime;
    for (const auto& tag : timeTags) {
        Exiv2::ExifKey key(tag);
        auto pos = exifData.findKey(key);
        if (pos != exifData.end()) {
            std::string timeStr = pos->toString();
            //            std::cout << "***" << tag << ": " << timeStr << std::endl;
            if (earliestTime.empty() || timeStr < earliestTime) {
                earliestTime = timeStr;
            }
        }
    }
    return earliestTime;
}


// Convert Exif DateTime string to UTC time string, example: "2023-10-23T15:30:00"
std::string ExifDateTimeToUTCString(const std::string& exifDateTime) {
    // Parse the Exif DateTime string
    std::tm tm = {};
    if (!ParseUTCStringTotm(tm, exifDateTime)) {
        std::cerr << "Failed to parse Exif DateTime: " << exifDateTime << std::endl;
        return "";
    }
    // Convert local time to time_t timestamp
    tm.tm_isdst = -1; // Let mktime automatically determine daylight saving time
    std::time_t localTime = std::mktime(&tm);
    localTime += 8 * 3600; // Convert to UTC+8 time
    if (localTime == -1) {
        std::cerr << "Failed to convert to time_t: " << exifDateTime << std::endl;
        return "";
    }

    // Convert time_t timestamp to UTC time
    std::tm* utcTm = std::gmtime(&localTime);
    if (utcTm == nullptr) {
        std::cerr << "Failed to convert to UTC time: " << exifDateTime << std::endl;
        return "";
    }

    // Format UTC time string
    std::ostringstream utcSs;
    utcSs << std::put_time(utcTm, "%Y-%m-%dT%H:%M:%S");
    return utcSs.str();
}


    // Determine if the file is an image format
bool IsImageFile(const fs::path& filePath) {
    // Define common image file extensions
    const std::vector<std::string> imageExtensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp", ".heic", ".raw"
    };

    // Get the file extension and convert it to lowercase
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Check if the extension is in the list of image extensions
    return std::find(imageExtensions.begin(), imageExtensions.end(), extension) != imageExtensions.end();
}

// format the time string to YYYYMMDD_HHMMSS used as part of file name
std::string FormatTimeToUTC8Name(const std::string& timeStr) {
    // Parse the time string
    std::tm tm = {};
    if (!ParseUTCStringTotm(tm, timeStr)) {
        std::cerr << "Failed to parse time string: " << timeStr << std::endl;
        return "";
    }

    // Convert local time to time_t timestamp
    tm.tm_isdst = -1; // Let mktime automatically determine daylight saving time
    std::time_t localTime = std::mktime(&tm);
    if (localTime == -1) {
        std::cerr << "Failed to convert to time_t: " << timeStr << std::endl;
        return "";
    }

    // Convert time_t timestamp to UTC+8 time
    localTime += 8 * 3600; // Add 8 hours in seconds
    std::tm* utcPlus8Tm = std::gmtime(&localTime);
    if (utcPlus8Tm == nullptr) {
        std::cerr << "Failed to convert to UTC+8 time: " << timeStr << std::endl;
        return "";
    }

    // Format UTC+8 time string
    std::ostringstream utcPlus8Ss;
    stringstream ss;
    utcPlus8Ss << std::put_time(utcPlus8Tm, "%Y%m%d_%H%M%S");

    // example: "2018-09-08 00:46:26.883"
    if (timeStr.length() >= 23) {
        // Extract milliseconds from the original time string
        std::string msStr = timeStr.substr(20, 3);
        int ms = std::stoi(msStr);
        utcPlus8Ss << "_" << std::setw(3) << std::setfill('0') << ms;
    }
    return utcPlus8Ss.str();
}

bool TraverseDirectory(const fs::path& directory) {
    try {
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            std::cerr << "Path does not exist or is not a directory: " << directory << std::endl;
            return false;
        }
        std::cout << "---- Traverse Directory: " << directory << " ----" << std::endl;

        int totalFileCount = 0;
        std::vector<std::string> errorFiles;

        for (const auto& entry : fs::recursive_directory_iterator(directory))
        {
            if (entry.is_directory())
            {
                std::cout << "---- Directory: " << entry.path() << " ----" << std::endl;
            }
            if (fs::is_regular_file(entry.status()))
            {
                totalFileCount++;
                if (!IsImageFile(entry.path()))
                {
                    std::cout << "Non-image file: " << entry.path() << std::endl;
                    continue;
                }
                std::string filePath = entry.path().string();
                std::string fileName = entry.path().filename().string();
                std::cout << totalFileCount << ": " << fileName << ": ";
                std::string fileExtension = entry.path().extension().string();
                std::string nameTime = ParseFileNameTime(fileName);
                std::string targetTime = nameTime;
                std::string exifTime = GetExifTimeEarliest(filePath);
                exifTime = ExifDateTimeToUTCString(exifTime);
                if (exifTime.empty() && nameTime.empty())
                {
                    std::cerr << "[Ignore]Unable to parse time: " << fileName << std::endl;
                    errorFiles.push_back(filePath);
                    continue;
                }

                if (!targetTime.empty() && !exifTime.empty() && targetTime.find('-') != std::string::npos &&
                    exifTime.find('-') != std::string::npos)
                {
                    // for some exif time too old, we need to use the name time
                    std::string timeLimt("2010-01-01 00:00:00");
                    if (exifTime < timeLimt)
                    {
                        targetTime = nameTime;
                        std::cout << "Exif time is too old: " << exifTime << std::endl;
                    }
                    else
                    {
                        targetTime = std::min<std::string>(nameTime, exifTime);
                    }

                    if (nameTime.length() >= 10 && exifTime.length() >= 10 && nameTime.substr(0, 10) == exifTime.substr(0, 10))
                    {
                        // if the time of exif time is "00:00:00", we need to use the name time
                        if (exifTime.length() >= 19 && exifTime.substr(11, 8) == "00:00:00")
                        {
                            targetTime = nameTime;
                            std::cout << "Exif time is 00:00:00: " << exifTime << std::endl;
                        }
                        else if (nameTime.length() >= 19 && nameTime.substr(11, 8) == "00:00:00")
                        {
                            targetTime = exifTime;
                            std::cout << "Name time is 00:00:00: " << nameTime << std::endl;
                        }
                        else
                        {
                            targetTime = nameTime.length() > exifTime.length() ? nameTime : exifTime;
                        }
                    }
                }
                else if (!targetTime.empty())
                {
                    targetTime = targetTime;
                }
                else if (!exifTime.empty())
                {
                    targetTime = exifTime;
                }
                else
                {
                    std::cerr << "[Ignore]Unable to parse time: " << fileName << std::endl;
                    errorFiles.push_back(filePath);
                    continue;
                }

                // format the time string to YYYYMMDD_HHMMSS used as part of file name
                std::string formattedTimeStr = FormatTimeToUTC8Name(targetTime);
                if (formattedTimeStr.empty())
                {
                    std::cerr << "[Ignore]Failed to format time to UTC8 Str: " << targetTime << std::endl;
                    errorFiles.push_back(filePath);
                    continue;
                }

                std::string targetFileName = "IMG_" + formattedTimeStr + fileExtension;
                std::cout << " NameTime: " << nameTime << ", ExifTime: "
                    << exifTime << ", TargetTime: " << targetTime << ", TargetName: " << targetFileName << std::endl;
                // Modify exif time
                if (!ModifyExifDataForTime(filePath, targetTime)) {
                    std::cerr << "Exif time modification failed: " << filePath << std::endl;
                    errorFiles.push_back(filePath);
                }

                // modify file time
                if (!ModifyFileCreationDate(filePath, targetTime)) {
                    std::cerr << "File time modification failed: " << filePath << std::endl;
                    errorFiles.push_back(filePath);
                }

                // Rename file with correct time
                if (targetFileName == fileName)
                {
                    // File name is already correct
                    std::cout << "File name is already correct: " << filePath << std::endl;
                    continue;
                }
                std::string newFilePath = entry.path().parent_path().string() + "/" + targetFileName;
                if (fs::exists(newFilePath))
                {
                    std::cerr << "Target file already exists: " << newFilePath << std::endl;
                    errorFiles.push_back(filePath);
                    continue;
                }
                if (!RenameFile(filePath, newFilePath))
                {
                    std::cerr << "Rename failed: " << filePath << std::endl;
                    errorFiles.push_back(filePath);
                    continue;
                }

                std::cout << std::endl;
            }
        }
        std::size_t errFileCount = errorFiles.size();
        std::cout << "------------------------------------------" << std::endl;
        std::cout << "Total files: " << totalFileCount << ", Error files: " << errFileCount << std::endl;
        if (errFileCount > 0)
        {
            std::cout << "Error file list: " << std::endl;
            for (const auto& errFile : errorFiles)
            {
                std::cout << errFile << std::endl;
            }
        }
        std::cout << "------------------------------------------" << std::endl;
        std::cout << "Total files: " << totalFileCount << ", Error files: " << errFileCount << std::endl;
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    return true;
}

int JustForTest()
{

    vector<string> test_files = {
        "20160331_202334.jpg",
        "IMG_20231111_193849.jpg",
        "VID_20210801_171003.jpg",
        "retouch_2021122809040423_edit_265097447538194.jpg",
        "PANO_20231001_143241.jpg",
        "MTXX_PT20230623_190638417.jpg",
        "mmexport1568301595980.jpg",
        "mmexport1602999370599.jpg",
        "MEITU_20240807_123043882.jpg",
        "wx_camera_1719390504866.jpg",
        "mmexport7be0804a9b1efd520f6f1fdf56b41cd6_1678513449241.jpeg",
        "1605199092110.jpeg",
        "20220115-wczt.jpg",
        "mmexportfd70cfd41603a498b9c452f818963205_1638624850479.jpg",
        "mmexport0ac3a28c487cebfacfcdbe1790585873_1652518600567.jpeg",
        "l00972450_1543624986659.jpg",
        "mmexport1620111487858.jpg"
    };

    for (const auto& f : test_files) {
        cout << setw(45) << left << f
            << " => " << ParseFileNameTime(f) << endl;
    }

    std::string fileName = "IMG_20180428_160000.jpg";
    // std::string folderPath = "/mnt/g/Pic_test/1/";
    std::string folderPath = "g:/Pic_test/1/";
    std::string testFileNameOri = folderPath + "IMG_20180428_160000_ori.jpg";
    std::string testFileNameNew = folderPath + fileName;
    std::cout << "----------------------------------" << std::endl;
    //    std::cout << RenameFile(testFileNameOri, testFileNameNew) << std::endl;

    std::string timeStr = ParseFileNameTime(fileName);
    timeStr = "2006-09-07 07:51:35.123";
    std::cout << "Parsed time: " << timeStr << std::endl;
    PrintPosixFileTimes(testFileNameOri);
    // Example usage: Set the time to 2023-10-01 12:30:45
    PrintExifTime(testFileNameOri);
    if (ModifyExifDataForTime(testFileNameNew, timeStr))
    {
        std::cout << "Capture time modified successfully" << std::endl;
    }
    else {
        std::cout << "Modification failed" << std::endl;
    }

    ModifyFileCreationDate(testFileNameNew, timeStr);
    std::cout << "------------------------------------------" << std::endl;
    PrintPosixFileTimes(testFileNameNew);
    PrintExifTime(testFileNameNew);

    std::cout << ExifDateTimeToUTCString(GetExifTimeEarliest(testFileNameOri)) << std::endl;

    return 0;

    //    std::time_t timestamp = 1600217726; // Example timestamp (2023-01-01T00:00:00Z)
    //    std::string utcString = TimestampToUTCString(timestamp);
    //    std::cout << "UTC time string: " << utcString << std::endl;

    std::string utcString = "2023-01-02";
    //    std::string utcString = "2023-01-02T08:00:00Z";
    std::cout << "UTC time string: " << utcString << std::endl;
    std::time_t timestamp = UTCStringToTimestamp(utcString);
    std::cout << "Timestamp: " << timestamp << std::endl;
    std::string utcString2 = TimestampToUTCString(timestamp);
    std::cout << "UTC time string_New: " << utcString2 << std::endl;

    return 0;

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path>" << std::endl;
        return 1;
    }
    std::string directoryPath = argv[1];

    //    JustForTest();
    //    directoryPath = "/mnt/g/Pic_test/";
    //    std::string directoryPath = "/mnt/f/Photos/Mate60Pro_20250316/";

    TraverseDirectory(directoryPath);
    return 0;
}
