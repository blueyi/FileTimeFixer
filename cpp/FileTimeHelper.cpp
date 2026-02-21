#include "TimeConvert.h"
#include "FileTimeHelper.h"
#include <chrono>
#include <iostream>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifndef F_OK
#define F_OK 0
#endif

namespace filetimefixer {

// Target time string is Beijing (UTC+8). Convert to UTC time_t for file system.
static std::time_t targetTimeToUtcTimestamp(const std::tm& tm) {
    std::time_t utc;
#if defined(_WIN32)
    utc = _mkgmtime(const_cast<std::tm*>(&tm));
#else
    char* tz = std::getenv("TZ");
    setenv("TZ", "UTC", 1);
    tzset();
    utc = std::mktime(const_cast<std::tm*>(&tm));
    if (tz) setenv("TZ", tz, 1); else unsetenv("TZ");
    tzset();
#endif
    if (utc == static_cast<std::time_t>(-1)) return utc;
    return utc - 8 * 3600;  // Beijing -> UTC
}

bool setFileTimesToTargetTime(const fs::path& filepath, const std::string& timeStr) {
    std::tm tm = {};
    if (!parseUTCStringToTm(tm, timeStr)) {
        std::cerr << "Failed to parse time string: " << timeStr << std::endl;
        return false;
    }
    tm.tm_isdst = 0;
    std::time_t timestamp = targetTimeToUtcTimestamp(tm);
    if (timestamp == static_cast<std::time_t>(-1)) {
        std::cerr << "Failed to convert time: " << timeStr << std::endl;
        return false;
    }
#if defined(_WIN32)
    FILETIME ftCreate, ftAccess, ftWrite;
    LONGLONG ll = Int32x32To64(timestamp, 10000000) + 116444736000000000LL;
    ftCreate.dwLowDateTime = (DWORD)ll;
    ftCreate.dwHighDateTime = (DWORD)(ll >> 32);
    ftAccess = ftWrite = ftCreate;
    std::wstring wpath = filepath.wstring();
    HANDLE hFile = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES,
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
    auto sys_time = std::chrono::system_clock::from_time_t(timestamp);
    auto sys_duration = sys_time.time_since_epoch();
    auto file_duration = std::chrono::duration_cast<fs::file_time_type::duration>(sys_duration);
    fs::file_time_type file_time(file_duration);
    fs::last_write_time(filepath, file_time);
#endif
    return true;
}

void printPosixFileTimes(const std::string& filename) {
    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) != 0) return;
    std::cout << "Last access time: " << ctime(&fileStat.st_atime)
              << "Last modification time: " << ctime(&fileStat.st_mtime)
              << "Metadata modification time: " << ctime(&fileStat.st_ctime);
}

bool renameFile(const std::string& oldName, const std::string& newName) {
    if (access(oldName.c_str(), F_OK) != 0) {
        std::cerr << "File not exist: " << oldName << std::endl;
        return false;
    }
    if (oldName == newName) {
        std::cerr << "New name is the same as old name!" << std::endl;
        return false;
    }
    if (rename(oldName.c_str(), newName.c_str()) == 0) {
        std::cout << "Rename success: " << oldName << " -> " << newName << std::endl;
        return true;
    }
    return false;
}

}  // namespace filetimefixer
