# FileTimeFixer

Batch-fix **image** and **video** file timestamps and metadata (EXIF for images, QuickTime `creation_time` for videos) from filenames and metadata, and rename to `IMG_YYYYMMDD_HHMMSS.ext` or `VID_YYYYMMDD_HHMMSS.ext` so media sort by target time on **Windows, Mac, Android, iPhone**. Video metadata requires **ffprobe** and **ffmpeg** on PATH.

---

## Project layout

The repo provides **C++** and **Python** implementations; behaviour and output are aligned via shared test specs.

```
FileTimeFixer/
├── README.md                 # This file
├── docs/                     # Docs (e.g. platform time sources, similarity design)
│   └── PlatformTimeSources.md
├── similar_images/           # Similar image detection (Python, perceptual hash)
│   ├── README.md
│   ├── requirements.txt, pyproject.toml
│   └── similar_images/       # Package: find_similar_groups, CLI find-similar
├── test_spec/                # Shared test specs (C++ and Python)
│   ├── README.md
│   ├── time_parse.yaml       # Filename -> parsed time
│   └── target_resolver.yaml  # (name_time, exif_time) -> (target_time, scenario)
├── cpp/                      # C++ implementation (recommended)
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── TimeParse.*, TimeConvert.*, ExifHelper.*, FileTimeHelper.*,
│   ├── ImageUtil.*, TargetTimeResolver.*, Main.cpp, Tests.cpp
│   └── build/                # Build dir (local)
├── python/                   # Python implementation
│   ├── README.md
│   ├── pyproject.toml, requirements.txt, requirements-dev.txt
│   ├── filetimefixer/        # Package source
│   └── tests/                # pytest (includes test_spec-loaded cases)
└── old_cpp/                  # Legacy single-file C++ (reference only)
    ├── README.md
    └── FileTimeFixer.cpp
```

- **cpp/**: C++ build, depends on Exiv2; single executable, EXIF for JPEG/PNG/HEIC/RAW, etc.
- **python/**: Python, Pillow + piexif; `pip install`; good for contribution and CI.
- **similar_images/**: Python; find similar images by perceptual hash with level 1–3; see [docs/ImageSimilarityDesign.md](docs/ImageSimilarityDesign.md).
- **test_spec/**: YAML definitions for time parsing and target-time resolution; both implementations align with them.
- **old_cpp/**: Legacy C++; for reference only; use **cpp/** for builds.

---

## C++ vs Python

| Aspect | C++ (cpp/) | Python (python/) |
|--------|------------|------------------|
| **Use case** | Daily use, batch, many formats (HEIC/RAW) | Quick try, scripting, CI/contrib |
| **Formats** | Exiv2: JPEG/PNG/HEIC/RAW | Pillow+piexif: JPEG best, PNG partial |
| **Distribution** | Single executable, no runtime deps | Needs Python + pip deps |
| **Build/install** | CMake + Exiv2 | `pip install -e .` |
| **Tests** | `./FileTimeFixer --test` (aligned with test_spec) | `pytest tests/` (test_spec-loaded) |

**Summary**: **C++** is the main implementation (format coverage, single binary); **Python** is an equivalent for easy install and automation. Both are kept in sync via **test_spec/**.

---

## Platform time sources (what we fix)

| Platform | Sort by | Fields we set |
|----------|---------|----------------|
| **Windows** | “Date taken” -> EXIF; “Date modified” -> file time | EXIF three time tags + file creation/access/modification |
| **Mac** | Finder -> file times; Photos -> EXIF or file | EXIF + file mtime |
| **Android** | Google Photos / gallery -> **EXIF** DateTimeOriginal | EXIF three time tags |
| **iPhone** | Photos “Date taken” -> **EXIF** | EXIF three time tags |

See [docs/PlatformTimeSources.md](docs/PlatformTimeSources.md).

---

## Overview

- **Recurse directory**: Process images under the given directory.
- **Time sources**: Parse time from **filename** and from **EXIF** (if present); resolve to a single **target time**.
- **Fixes**: Write target time to EXIF (DateTimeOriginal / DateTimeDigitized / Image.DateTime) and file system time; rename to `IMG_YYYYMMDD_HHMMSS.ext` (UTC+8).

Supported filename patterns: `YYYYMMDD_HHMMSS`, 8-digit date, 10/13-digit timestamps, mmexport, wx_camera, etc. Target-time scenarios (NameOnly, ExifOnly, BothUseEarliest, ExifTooOldUseName, SameDay*, etc.) are in **test_spec/** and in each implementation.

---

## Quick start

**C++ (recommended)**  
If Exiv2/FFmpeg are not installed, run the env init script first: **Windows** `.\cpp\init_env.ps1` | **Linux/macOS** `./cpp/init_env.sh` (or `source cpp/init_env.sh`). Then build:

```bash
cd cpp && mkdir build && cd build && cmake .. && cmake --build . --config Release
./FileTimeFixer <directory>
./FileTimeFixer --test
```

**Python**

```bash
cd python && pip install -e . && filetimefixer <directory>
pytest tests/ -v
```

---

## Test suite and consistency

- **test_spec/** defines all filename-time and target-time cases.
- **C++**: Cases in `cpp/Tests.cpp` match test_spec; run `FileTimeFixer --test`.
- **Python**: `tests/test_spec_parity.py` loads test_spec YAML; run `pytest tests/`.

When changing parsing or adding formats, update **test_spec/** and both implementations so behaviour stays aligned.

---

## License

See the license stated in the repository.
