#include "TimeParse.h"
#include "TimeConvert.h"
#include "ExifHelper.h"
#include "FileTimeHelper.h"
#include "ImageUtil.h"
#include "TargetTimeResolver.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Default test folder when no directory argument is given (change to your path if needed)
#ifdef _WIN32
static const char kDefaultTestFolder[] = "F:\\Photos\\time_fix_test - Copy";
#else
static const char kDefaultTestFolder[] = "/tmp/time_fix_test";
#endif

namespace {

// On Windows convert ACP string to UTF-8 for log file; on other platforms return as-is.
static std::string toUtf8ForLog(const std::string& s) {
#ifdef _WIN32
    if (s.empty()) return s;
    int wlen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return s;
    std::wstring wbuf(static_cast<size_t>(wlen), 0);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &wbuf[0], wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return s;
    std::string out(static_cast<size_t>(ulen), 0);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), -1, &out[0], ulen, nullptr, nullptr);
    out.resize(static_cast<size_t>(ulen - 1));
    return out;
#else
    return s;
#endif
}

static std::string sanitizeForLogFilename(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            out += '_';
        else
            out += c;
    }
    return out;
}

// Process a single image file (when path is a file rather than a directory).
bool processSingleFile(const fs::path& filePath) {
    try {
        if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
            std::cerr << "Path does not exist or is not a regular file: " << filePath << std::endl;
            return false;
        }
        if (!filetimefixer::isImageFile(filePath)) {
            std::cerr << "Not an image file: " << filePath << std::endl;
            return false;
        }
        std::string pathStr = filePath.string();
        std::string fileName = filePath.filename().string();
        std::string fileExtension = filePath.extension().string();
        fs::path parentPath = filePath.parent_path();

        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        char dateTimeBuf[32];
        std::snprintf(dateTimeBuf, sizeof(dateTimeBuf), "%04d%02d%02d_%02d%02d%02d",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
            lt->tm_hour, lt->tm_min, lt->tm_sec);
        std::string folderName = parentPath.filename().string();
        if (folderName.empty()) folderName = "single";
        std::string logName = sanitizeForLogFilename(folderName) + "_" + dateTimeBuf + ".log";
        fs::path logPath = fs::current_path() / logName;
        std::ofstream logFile(logPath, std::ios::out | std::ios::app);
        if (logFile) {
            if (logFile.tellp() == 0)
                logFile << "\xEF\xBB\xBF";  // UTF-8 BOM
            logFile << "===== FileTimeFixer run (single file) " << dateTimeBuf << " =====\n";
            logFile << "File: " << toUtf8ForLog(pathStr) << "\n";
        }

        std::cout << "---- Process single file: " << filePath << " ----" << std::endl;

        bool renamedThisFile = false;
        std::string finalPath = pathStr;
        bool success = false;

        try {
            std::string nameTime = filetimefixer::parseFileNameTime(fileName);
            std::string exifTimeRaw = filetimefixer::getExifTimeEarliest(pathStr);
            std::string exifTime = filetimefixer::exifDateTimeToUTCString(exifTimeRaw);

            filetimefixer::ResolveResult resolved = filetimefixer::resolveTargetTime(nameTime, exifTime);
            if (resolved.targetTime.empty()) {
                std::cerr << "[Ignore] Unable to parse time: " << fileName << std::endl;
                if (logFile) logFile << "  Error: Unable to parse time\n";
                return false;
            }
            if (resolved.targetTime.length() <= 10)
                resolved.targetTime = filetimefixer::supplementDateWithCurrentUtcTime(resolved.targetTime);

            std::string formattedTimeStr = filetimefixer::formatTimeToUTC8Name(resolved.targetTime);
            if (formattedTimeStr.empty()) {
                std::cerr << "[Ignore] Failed to format time: " << resolved.targetTime << std::endl;
                if (logFile) logFile << "  Error: Failed to format target time\n";
                return false;
            }

            std::string targetFileName = "IMG_" + formattedTimeStr + fileExtension;
            std::cout << fileName << " | NameTime: " << nameTime
                      << ", ExifTime: " << exifTime << ", TargetTime: " << resolved.targetTime
                      << " [" << filetimefixer::scenarioName(resolved.scenario) << "] => " << targetFileName << std::endl;

            if (targetFileName != fileName) {
                std::string newFilePath = parentPath.string() + "/" + targetFileName;
                if (fs::exists(newFilePath)) {
                    std::cerr << "Target file already exists: " << newFilePath << std::endl;
                    if (logFile) logFile << "  Error: Target file already exists\n";
                    return false;
                }
                if (!filetimefixer::renameFile(pathStr, newFilePath)) {
                    std::cerr << "Rename failed: " << pathStr << std::endl;
                    if (logFile) logFile << "  Error: Rename failed\n";
                    return false;
                }
                finalPath = newFilePath;
                renamedThisFile = true;
            } else {
                std::cout << "File name already correct: " << pathStr << std::endl;
            }

            bool exifOk = filetimefixer::modifyExifDataForTime(finalPath, resolved.targetTime);
            bool fileTimeOk = filetimefixer::setFileTimesToTargetTime(fs::path(finalPath), resolved.targetTime);
            std::string exifInfo = filetimefixer::getExifTimeInfoString(finalPath);
            std::cout << "  [EXIF after fix] " << exifInfo << std::endl;
            if (!fileTimeOk) {
                std::cerr << "File time modification failed: " << finalPath << std::endl;
            } else {
                success = true;
            }
            if (logFile) {
                logFile << "1. File: " << toUtf8ForLog(finalPath) << "\n  TargetTime: " << resolved.targetTime
                        << "  EXIF_ok: " << (exifOk ? "yes" : "no")
                        << "  FileTime_ok: " << (fileTimeOk ? "yes" : "no")
                        << "\n  [EXIF after fix] " << toUtf8ForLog(exifInfo) << "\n";
            }
        } catch (const Exiv2::Error& e) {
            std::cerr << "[Skip] Exiv2 error on " << fileName << ": " << e.what() << std::endl;
            if (logFile) logFile << "  Error: Exiv2 - " << toUtf8ForLog(e.what()) << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[Skip] Exception on " << fileName << ": " << e.what() << std::endl;
            if (logFile) logFile << "  Error: " << toUtf8ForLog(e.what()) << "\n";
        }

        std::cout << "------------------------------------------" << std::endl;
        std::cout << "[Summary] Single file: " << (success ? "OK" : "Error") << std::endl;
        if (logFile) {
            logFile << "------------------------------------------\n[Summary] Single file: " << (success ? "OK" : "Error") << "\n";
            logFile << "Log file: " << toUtf8ForLog(logPath.string()) << "\n";
            logFile.close();
            std::cout << "Log written to: " << logPath.string() << std::endl;
        }
        return success;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }
}

bool traverseDirectory(const fs::path& directory) {
    try {
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            std::cerr << "Path does not exist or is not a directory: " << directory << std::endl;
            return false;
        }
        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        char dateTimeBuf[32];
        std::snprintf(dateTimeBuf, sizeof(dateTimeBuf), "%04d%02d%02d_%02d%02d%02d",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
            lt->tm_hour, lt->tm_min, lt->tm_sec);
        std::string folderName = directory.filename().string();
        if (folderName.empty()) folderName = "folder";
        std::string logName = sanitizeForLogFilename(folderName) + "_" + dateTimeBuf + ".log";
        fs::path logPath = fs::current_path() / logName;
        std::ofstream logFile(logPath, std::ios::out | std::ios::app);
        if (logFile) {
            if (logFile.tellp() == 0)
                logFile << "\xEF\xBB\xBF";  // UTF-8 BOM
            logFile << "===== FileTimeFixer run " << dateTimeBuf << " =====\n";
            logFile << "Directory: " << toUtf8ForLog(directory.string()) << "\n";
        }

        std::cout << "---- Traverse Directory: " << directory << " ----" << std::endl;
        if (logFile) logFile << "---- Traverse Directory: " << toUtf8ForLog(directory.string()) << " ----\n";

        int totalFileCount = 0;
        int logSeq = 0;          // Sequence number for each file in log (1-based)
        int successCount = 0;   // Processed with rename and/or EXIF/file-time change, no error
        int unchangedCount = 0; // No rename needed (filename already correct), no error
        std::vector<std::pair<std::string, std::string>> errorEntries; // (full path, error message)

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_directory()) {
                std::cout << "---- Directory: " << entry.path() << " ----" << std::endl;
            }
            if (!fs::is_regular_file(entry.status())) continue;

            totalFileCount++;
            if (!filetimefixer::isImageFile(entry.path())) {
                std::cout << "Non-image file: " << entry.path() << std::endl;
                continue;
            }

            std::string filePath = entry.path().string();
            std::string fileName = entry.path().filename().string();
            std::string fileExtension = entry.path().extension().string();
            bool renamedThisFile = false;
            logSeq++;

            try {
                std::string nameTime = filetimefixer::parseFileNameTime(fileName);
                std::string exifTimeRaw = filetimefixer::getExifTimeEarliest(filePath);
                std::string exifTime = filetimefixer::exifDateTimeToUTCString(exifTimeRaw);

                filetimefixer::ResolveResult resolved = filetimefixer::resolveTargetTime(nameTime, exifTime);
                if (resolved.targetTime.empty()) {
                    std::cerr << "[Ignore] Unable to parse time: " << fileName << std::endl;
                    errorEntries.emplace_back(filePath, "Unable to parse time");
                    continue;
                }
                if (resolved.targetTime.length() <= 10)
                    resolved.targetTime = filetimefixer::supplementDateWithCurrentUtcTime(resolved.targetTime);

                std::string formattedTimeStr = filetimefixer::formatTimeToUTC8Name(resolved.targetTime);
                if (formattedTimeStr.empty()) {
                    std::cerr << "[Ignore] Failed to format time: " << resolved.targetTime << std::endl;
                    errorEntries.emplace_back(filePath, "Failed to format target time: " + resolved.targetTime);
                    continue;
                }

                std::string targetFileName = "IMG_" + formattedTimeStr + fileExtension;
                std::cout << totalFileCount << ": " << fileName << " | NameTime: " << nameTime
                          << ", ExifTime: " << exifTime << ", TargetTime: " << resolved.targetTime
                          << " [" << filetimefixer::scenarioName(resolved.scenario) << "] => " << targetFileName << std::endl;

                std::string finalPath = filePath;
                if (targetFileName != fileName) {
                    std::string newFilePath = entry.path().parent_path().string() + "/" + targetFileName;
                    if (fs::exists(newFilePath)) {
                        std::cerr << "Target file already exists: " << newFilePath << std::endl;
                        errorEntries.emplace_back(filePath, "Target file already exists: " + newFilePath);
                        continue;
                    }
                    if (!filetimefixer::renameFile(filePath, newFilePath)) {
                        std::cerr << "Rename failed: " << filePath << std::endl;
                        errorEntries.emplace_back(filePath, "Rename failed");
                        continue;
                    }
                    finalPath = newFilePath;
                    renamedThisFile = true;
                } else {
                    std::cout << "File name already correct: " << filePath << std::endl;
                }

                bool exifOk = filetimefixer::modifyExifDataForTime(finalPath, resolved.targetTime);
                bool fileTimeOk = filetimefixer::setFileTimesToTargetTime(fs::path(finalPath), resolved.targetTime);
                std::string exifInfo = filetimefixer::getExifTimeInfoString(finalPath);
                std::cout << "  [EXIF after fix] " << exifInfo << std::endl;
                if (!fileTimeOk) {
                    std::cerr << "File time modification failed: " << finalPath << std::endl;
                    errorEntries.emplace_back(finalPath, "File time modification failed");
                } else {
                    if (renamedThisFile) successCount++; else unchangedCount++;
                }
                if (logFile) {
                    logFile << logSeq << ". File: " << toUtf8ForLog(finalPath) << "\n  TargetTime: " << resolved.targetTime
                            << "  EXIF_ok: " << (exifOk ? "yes" : "no")
                            << "  FileTime_ok: " << (fileTimeOk ? "yes" : "no")
                            << "\n  [EXIF after fix] " << toUtf8ForLog(exifInfo) << "\n";
                }
            } catch (const Exiv2::Error& e) {
                std::cerr << "[Skip] Exiv2 error on " << fileName << ": " << e.what() << std::endl;
                errorEntries.emplace_back(filePath, std::string("Exiv2 error: ") + e.what());
            } catch (const std::exception& e) {
                std::cerr << "[Skip] Exception on " << fileName << ": " << e.what() << std::endl;
                errorEntries.emplace_back(filePath, std::string("Exception: ") + e.what());
            }
        }

        const int totalImageCount = successCount + unchangedCount + static_cast<int>(errorEntries.size());
        std::cout << "------------------------------------------" << std::endl;
        std::cout << "[Summary]" << std::endl;
        std::cout << "  Total processed: " << totalImageCount << std::endl;
        std::cout << "  Success:         " << successCount << std::endl;
        std::cout << "  Unchanged:       " << unchangedCount << std::endl;
        std::cout << "  Errors:          " << errorEntries.size() << std::endl;
        if (logFile) {
            logFile << "------------------------------------------\n[Summary]\n"
                    << "  Total: " << totalImageCount << "  Success: " << successCount << "  Unchanged: " << unchangedCount << "  Errors: " << errorEntries.size() << "\n";
        }
        if (!errorEntries.empty()) {
            std::cout << "[Error details]" << std::endl;
            for (size_t i = 0; i < errorEntries.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << errorEntries[i].first << "\n      " << errorEntries[i].second << std::endl;
                if (logFile) logFile << "  Error: " << toUtf8ForLog(errorEntries[i].first) << " | " << toUtf8ForLog(errorEntries[i].second) << "\n";
            }
        }
        std::cout << "------------------------------------------" << std::endl;
        if (logFile) {
            logFile << "Log file: " << toUtf8ForLog(logPath.string()) << "\n";
            logFile.close();
            std::cout << "Log written to: " << logPath.string() << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    // Suppress Exiv2 warnings (e.g. "Directory Photo has an unexpected next pointer"); keep errors visible
    Exiv2::LogMsg::setLevel(Exiv2::LogMsg::error);
#ifdef _DEBUG
    std::cout << "Tip: Debug build may trigger 'abort()' on some images (Exiv2). For batch runs use Release: cmake --build . --config Release, then run Release\\FileTimeFixer.exe\n" << std::endl;
#endif
    std::string dirToProcess;
    if (argc < 2) {
        dirToProcess = kDefaultTestFolder;
        std::cout << "No directory given, using default test folder:\n  " << dirToProcess << "\n" << std::endl;
    } else {
        std::string arg = argv[1];
        if (arg == "--test" || arg == "-t") {
            extern int runAllTests();
            return runAllTests();
        }
        fs::path pathArg = fs::path(arg);
        if (fs::exists(pathArg) && fs::is_regular_file(pathArg)) {
            return processSingleFile(pathArg) ? 0 : 1;
        }
        dirToProcess = arg;
    }
    return traverseDirectory(dirToProcess) ? 0 : 1;
}
