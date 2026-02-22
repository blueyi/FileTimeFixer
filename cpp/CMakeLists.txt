cmake_minimum_required(VERSION 3.10)
project(FileTimeFixer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
	TimeParse.cpp
	TimeConvert.cpp
	ExifHelper.cpp
	FileTimeHelper.cpp
	ImageUtil.cpp
	TargetTimeResolver.cpp
	VideoMetaHelper.cpp
	Main.cpp
	Tests.cpp
)

add_executable(FileTimeFixer ${SOURCES})
target_include_directories(FileTimeFixer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FileTimeFixer PRIVATE exiv2)

# Copy exiv2.dll next to the executable on Windows so it runs from any CWD (e.g. Git Bash)
if(WIN32)
  set(EXIV2_DLL "")
  if(TARGET exiv2::exiv2)
    get_target_property(EXIV2_DLL exiv2::exiv2 IMPORTED_LOCATION)
    if(NOT EXIV2_DLL)
      get_target_property(EXIV2_DLL exiv2::exiv2 IMPORTED_LOCATION_DEBUG)
    endif()
  endif()
  if(NOT EXIV2_DLL)
    set(EXIV2_SEARCH_PATHS
      ${CMAKE_PREFIX_PATH}
      ${VCPKG_INSTALLED_DIR}
      ${CMAKE_BINARY_DIR}/vcpkg_installed
      ${CMAKE_SOURCE_DIR}/../vcpkg_installed
      ${CMAKE_SOURCE_DIR}/../../vcpkg_installed
      $ENV{VCPKG_ROOT}/installed
    )
    find_file(EXIV2_DLL NAMES exiv2.dll
      PATHS ${EXIV2_SEARCH_PATHS}
      PATH_SUFFIXES bin x64-windows/bin x86-windows/bin
      NO_DEFAULT_PATH)
  endif()
  if(EXIV2_DLL)
    message(STATUS "Will copy exiv2.dll from: ${EXIV2_DLL}")
    add_custom_command(TARGET FileTimeFixer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${EXIV2_DLL}" $<TARGET_FILE_DIR:FileTimeFixer>
      COMMENT "Copy exiv2.dll to output directory"
    )
  else()
    message(STATUS "exiv2.dll not found; copy it next to FileTimeFixer.exe to run (e.g. from vcpkg_installed/x64-windows/bin)")
  endif()
endif()

# MSVC: UTF-8 source (fix C4819), suppress CRT deprecation for ctime/access
if(MSVC)
  target_compile_options(FileTimeFixer PRIVATE /utf-8 /wd4996 /D_CRT_SECURE_NO_WARNINGS)
endif()
