# Shared test specs

This directory holds test specifications shared by the **C++** and **Python** implementations so behaviour and output stay aligned.

- **time_parse.yaml**: Filename -> parsed time. Covers 8+6 digit, 8-digit date, 10/13-digit timestamps, mmexport, wx_camera, and parse-failure cases.
- **target_resolver.yaml**: `(name_time, exif_time)` -> `(target_time, scenario)`. Covers None, NameOnly, ExifOnly, BothUseEarliest, ExifTooOldUseName, SameDay* scenarios.

Both implementations should satisfy these specs (Python via pytest loading YAML; C++ tests are kept in sync with the specs).
