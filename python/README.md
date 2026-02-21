# FileTimeFixer (Python)

Python implementation with the same behaviour as **cpp/**; specs are defined in **test_spec/** at the repo root for easy contribution and CI.

## Install

```bash
cd python
pip install -e .
# Dev and tests
pip install -e ".[dev]"
```

## Usage

```bash
# Process directory (recursive)
filetimefixer /path/to/photos

# Or as module
python -m filetimefixer.cli /path/to/photos

# Dry run (no writes)
python -m filetimefixer.cli --dry-run /path/to/photos

# Run tests
python -m filetimefixer.cli --test
# or
pytest tests/ -v
```

## Tests

```bash
cd python
pip install -e ".[dev]"
pytest tests/ -v --tb=short
```

CI runs in GitHub Actions (`.github/workflows/python-tests.yml`) on push/PR to `main`, `master`, or `feature/python-refactor` when `python/` changes; matrix: Python 3.9–3.12.

## Layout

```
python/
├── pyproject.toml
├── requirements.txt
├── requirements-dev.txt
├── filetimefixer/
│   ├── __init__.py
│   ├── time_parse.py      # Filename time parsing
│   ├── time_convert.py    # Time formatting (UTC, UTC+8)
│   ├── target_time_resolver.py  # Target time and scenario
│   ├── exif_helper.py     # EXIF read/write (Pillow + piexif)
│   ├── file_time_helper.py     # File time (Windows/Unix)
│   ├── image_util.py      # Image extension checks
│   └── cli.py             # CLI and traversal
└── tests/
    ├── test_time_parse.py
    ├── test_target_time_resolver.py
    ├── test_time_convert.py
    ├── test_image_util.py
    ├── test_exif_helper.py
    └── test_integration.py
```

## Dependencies

- Python >= 3.9
- Pillow >= 9.0 (EXIF read, image I/O)
- piexif >= 1.1 (JPEG EXIF write)

## Behaviour (aligned with C++)

- Filename parsing: `YYYYMMDD_HHMMSS`, 8-digit date, 10/13-digit timestamps, mmexport, wx_camera, etc.
- Target-time scenarios: NameOnly, ExifOnly, BothUseEarliest, ExifTooOldUseName, SameDayExifMidnightUseName, SameDayNameMidnightUseExif, SameDayBothFullUseMorePrecise.
- Output filename: `IMG_YYYYMMDD_HHMMSS.ext` (UTC+8).
