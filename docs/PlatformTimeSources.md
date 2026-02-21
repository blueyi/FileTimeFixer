# Platform time sources for albums and file managers

This project writes the **target time** into the time fields below so that sorting by “date taken” or “date” on Windows, Mac, Android, and iPhone shows photos in the correct order.

| Platform | App / UI | Time used for sort | Fields we set |
|----------|----------|--------------------|---------------|
| **Windows** | Explorer “Date taken” / Photos app “Date taken” | **EXIF**: Date taken from EXIF (DateTimeOriginal etc.); may fall back to file date | EXIF (DateTimeOriginal / DateTimeDigitized / Image.DateTime); file creation/modification/access |
| **Windows** | Explorer “Date modified” / “Date created” | **Filesystem**: last write time, creation time | Same; we set them to target time via SetFileTime |
| **Mac** | Finder column sort | **Filesystem**: birth time, mtime | mtime; EXIF. Note: on macOS, birth time often needs a system tool (e.g. SetFile -d) to change |
| **Mac** | Photos app | **Mixed**: import may use file time or EXIF; sort may use EXIF or add date | We set EXIF + file mtime for consistency |
| **Android** | Google Photos / gallery | **EXIF**: sort by DateTimeOriginal, not file mtime | EXIF three time tags |
| **iPhone** | Photos “Date taken” | **EXIF**: Date Captured = EXIF (DateTimeOriginal) | EXIF three time tags |

## Summary

- **EXIF** must be correct: it drives “date taken” on Windows and sort order on Android and iPhone.
- **File system times** (creation/modification) should match target time: they drive Explorer, Finder, and sometimes Photos on Mac.
- This project resolves a single **target time** from filename and EXIF, then writes it to both EXIF and file times so all four platforms sort correctly.
