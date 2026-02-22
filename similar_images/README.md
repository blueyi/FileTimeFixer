# Similar Images

Scan directories for images and detect similar ones by **similarity level**; output pairs or groups. Independent from [FileTimeFixer](../README.md) time-fix; use alone or before/after it (find similar → decide keep/delete → run time fix).

Design and level definitions: [docs/ImageSimilarityDesign.md](../docs/ImageSimilarityDesign.md).

---

## Layout

```
similar_images/
├── README.md           # This file
├── requirements.txt    # Python dependencies
├── pyproject.toml      # Optional: pip install -e . for CLI entry point
└── similar_images/     # Package source
    ├── __init__.py
    ├── similarity.py   # Perceptual hash and level thresholds
    └── cli.py          # CLI entry
```

---

## Features

1. **Single directory**: **Recursively** scan and output similar pairs with similarity and paths. Pairs with **100% similarity** are treated as **duplicates**; summary counts them and prompts whether to delete (only 100% duplicates are eligible; one kept per group by oldest EXIF).
2. **Two directories**: Compare two directories at the given level and output **file paths** (and similarity) of similar images between them.
3. **Two images**: Output the **similarity** of two given images.

---

## Install and run

```bash
cd similar_images
pip install -r requirements.txt
# Optional: pip install -e . then use find-similar command
```

### 1. Single directory: recursive scan, similar pairs and duplicate prompt

By default **recursively** scan the directory and subdirectories; output each pair’s similarity and paths. Pairs with **100% similarity** are marked `[duplicate]`. At the end a **summary** is printed (similar count, duplicate count/groups) and you are **asked whether to delete duplicates** (only 100% pairs enter this flow; oldest EXIF kept per group). Use **`--yes`** to delete without prompting.

```bash
# Leave a space between path and options, e.g. "F:\Photos\DCIM" --verbose
python -m similar_images.cli <directory>
# Do not recurse (current directory only)
python -m similar_images.cli --no-recursive <directory>
# Level and threshold
python -m similar_images.cli --level 1 <directory>
python -m similar_images.cli --level 2 --threshold 12 <directory>
# JSON (results, summary, duplicate stats)
python -m similar_images.cli --json <directory>
# Progress bar and current files
python -m similar_images.cli --verbose <directory>
python -m similar_images.cli -v <directory>
# Delete duplicates without prompt
python -m similar_images.cli --yes <directory>
```

#### Fast compare (same folder + optional time window)

When photos/videos are renamed with timestamps (e.g. `IMG_YYYYMMDD_HHMMSS.ext`), use **`--fast`** to speed up recursive scans:

- **`--fast`** (no `--time-window`): When recursing, **only compare images inside the same subfolder** (no cross-folder pairs). By default only pairs whose **filename timestamps are within 1 second** are compared (e.g. burst shots with same second). So with `--fast` alone you get: same folder only + 1s time window.
- **`--time-window SECS`**: Override the time window (default with `--fast` is 1). Only compare two images if **filename-derived timestamps** are within `SECS` seconds (e.g. `86400` = same day, `3600` = 1 hour). Timestamps are parsed from patterns like `YYYYMMDD_HHMMSS` or `YYYYMMDD` in the filename. If either file has no parseable timestamp, the pair is still compared.

```bash
# Same-folder only (faster recursive scan)
python -m similar_images.cli --fast <directory>

# Same folder + only compare images within 1 hour (by filename time)
python -m similar_images.cli --fast --time-window 3600 <directory>

# Same folder + only same day (by filename time)
python -m similar_images.cli --fast --time-window 86400 <directory>
```

#### Threading (`--threads`)

Single-dir mode supports multi-threading to speed up hashing and comparison:

- **With `--fast`**: one task per **subfolder** (each folder is processed in a worker; parallelism is by directory).
- **Without `--fast`**: hashing is parallel by **file**, then pair comparison is parallel by **chunk** of pairs.

Use **`--threads N`** to set the number of threads. If omitted, a default is chosen from the CPU count (capped at 32). Example:

```bash
# Auto threads (from CPU count)
python -m similar_images.cli <directory>

# Explicit thread count
python -m similar_images.cli --threads 8 <directory>
python -m similar_images.cli --fast --threads 4 <directory>
```

### 2. Two directories: compare and output paths

```bash
python -m similar_images.cli <dir1> <dir2>
python -m similar_images.cli --level 2 <dir1> <dir2>
python -m similar_images.cli --json <dir1> <dir2>
# Progress
python -m similar_images.cli --verbose <dir1> <dir2>
```

### 3. Two images: output similarity

```bash
python -m similar_images.cli <image1> <image2>
python -m similar_images.cli --json <image1> <image2>
```

After `pip install -e .` you can use the `find-similar` command with the same arguments (1 dir / 2 dirs / 2 images).

---

## Similarity levels

| Level | Name    | Description |
|-------|---------|-------------|
| 1     | Strict  | Nearly the same image (compression, slight resize) |
| 2     | Medium  | Same image, different size/crop/edits |
| 3     | Loose   | Same scene, different framing (currently looser hash; feature matching later) |

---

## Progress and log (directory mode)

With **`--verbose`** / **`-v`**:

- **Progress bar**: “Hashing” phase then “Comparing” phase with percentage.
- **Current file**: Progress bar shows current image(s) or pair.

Only in single-dir or two-dir mode; not in two-image mode.

---

## Duplicates (100% similar) and delete prompt

After a single-dir scan, the program counts pairs with **100% similarity** (duplicates) and prints “Duplicate (100% similar): N pair(s) in G group(s)” in the summary, then **asks whether to delete duplicates**. Only 100% identical images enter this flow; **oldest EXIF** is kept per group, the rest are listed for deletion. Answer `y` to delete; use **`--yes`** to skip the prompt and delete.

---

## EXIF-based dedupe (single dir only; all similar pairs)

Use **`--dedupe`** to keep one image per similar group by EXIF time and delete the rest (applies to **all similar pairs at current level**, not only 100%):

- **`--dedupe keep-older`** (default): Keep the image with **oldest** EXIF time; delete the others.
- **`--dedupe keep-newer`**: Keep the image with **newest** EXIF time; delete the others (older or no EXIF).

Images without EXIF are treated as “oldest”. Default is to ask for confirmation; **`--yes`** / **`-y`** skips the prompt.

```bash
python -m similar_images.cli --dedupe keep-older -y <directory>
python -m similar_images.cli --dedupe keep-newer <directory>
```

---

## Copy similar images to current directory

With **`--copy-similar`**, all images that appear in any similar pair are copied to a directory **`similar_images_<timestamp>`** under the current working directory (timestamp `YYYYMMDD_HHMMSS`). Supported in single-dir and two-dir mode; name conflicts get `_2`, `_3`, etc.

```bash
python -m similar_images.cli --copy-similar <directory>
python -m similar_images.cli --copy-similar <dir1> <dir2>
```

---

## Read pairs from file (-r)

Use **`-r`** / **`--read-from-file`** with a text file where each line has two image paths separated by **`, `** (comma space). The program compares each pair and outputs similarity.

- Blank lines and lines starting with `#` are ignored.
- Progress is streamed to **stdout** (`[n/total] Comparing: path1 <-> path2`). With **`--log-file <file>`**, the same lines are appended to that log.
- Results are printed per pair; **`--level`** controls the “similar” threshold; pairs above it are marked `[similar]`.
- A **summary** is printed at the end (total compared, similar count, deleted count if any). With `--log-file`, the summary is also written to the log. With `--json`, output is `{"results": [...], "summary": {"total_compared", "similar_count", "deleted_count"}}`.

**`--keep-in-line first`** / **`--keep-in-line second`**: For each pair considered similar, **keep** the first or second path in the line and **delete** the other. Default is to ask; **`--yes`** skips the prompt.

```bash
# Compare only
python -m similar_images.cli -r pairs.txt
python -m similar_images.cli -r pairs.txt --level 1 --json

# Keep first in each similar pair, delete second (will prompt)
python -m similar_images.cli -r pairs.txt --keep-in-line first

# Keep second, delete first, no prompt
python -m similar_images.cli -r pairs.txt --keep-in-line second -y

# Also write status to log
python -m similar_images.cli -r pairs.txt --log-file compare.log
```

Example file (`pairs.txt`):

```
/path/to/a.jpg, /path/to/b.jpg
C:\Photos\img1.png, C:\Photos\img2.png
```

---

## Dependencies

- **Python** ≥ 3.9
- **Pillow**: image I/O
- **imagehash**: perceptual hash (pHash/dHash)
- **tqdm**: progress bar (when using `--verbose`)
- Optional for level 3 or histogram: **opencv-python**, **numpy** (see requirements.txt comments)
