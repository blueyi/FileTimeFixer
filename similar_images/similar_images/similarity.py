"""Perceptual hash based similarity with level-based thresholds."""

import os
import re
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterator

# progress_callback(phase, current, total, detail, result=None); in "compare" phase result is similarity float
ProgressCallback = Callable[[str, int, int, Path | tuple[Path, Path] | None, float | None], None]

import imagehash
from PIL import Image

from .exif_utils import get_exif_datetime

# Supported image extensions (aligned with main project image_util)
IMAGE_EXTENSIONS = {
    ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".tif",
    ".webp", ".heic", ".raw",
}

# Default Hamming distance thresholds by level (pHash): 1=strict 2=medium 3=loose
LEVEL_DEFAULTS = {
    1: 5,
    2: 15,
    3: 25,
}

# Default pHash bit length (16*16) for converting Hamming distance to similarity [0,1]
HASH_BITS = 256


def default_threads() -> int:
    """Suggested number of threads when user does not set --threads (based on CPU count)."""
    return min(32, os.cpu_count() or 4)


def _is_image(path: Path) -> bool:
    return path.suffix.lower() in IMAGE_EXTENSIONS


def similarity_score(h1: imagehash.ImageHash, h2: imagehash.ImageHash, bits: int = HASH_BITS) -> float:
    """Convert Hamming distance to similarity in [0, 1]; 1 means identical."""
    d = h1 - h2
    return max(0.0, 1.0 - d / bits)


def _compute_phash(path: Path, hash_size: int = 16) -> imagehash.ImageHash | None:
    try:
        with Image.open(path) as img:
            img = img.convert("RGB")
            return imagehash.phash(img, hash_size=hash_size)
    except Exception:
        return None


def _collect_image_paths(root: Path, recursive: bool = True) -> list[Path]:
    root = root.resolve()
    if not root.is_dir():
        return []
    if recursive:
        return [p for p in root.rglob("*") if p.is_file() and _is_image(p)]
    return [p for p in root.iterdir() if p.is_file() and _is_image(p)]


# Filename timestamp patterns: YYYYMMDD_HHMMSS or YYYYMMDD (e.g. IMG_20250222_143052.jpg, 20220115.jpg)
_FNAME_DATETIME = re.compile(r"(\d{8})_(\d{6})")
_FNAME_DATE = re.compile(r"(\d{8})")


def parse_datetime_from_filename(path: Path) -> datetime | None:
    """
    Parse datetime from filename (stem). Tries YYYYMMDD_HHMMSS then YYYYMMDD.
    Returns datetime (time 00:00:00 when only date) or None.
    """
    stem = path.stem
    m = _FNAME_DATETIME.search(stem)
    if m:
        y, mo, d = int(m.group(1)[:4]), int(m.group(1)[4:6]), int(m.group(1)[6:8])
        h, mi, s = int(m.group(2)[:2]), int(m.group(2)[2:4]), int(m.group(2)[4:6])
        try:
            return datetime(y, mo, d, h, mi, s)
        except ValueError:
            pass
    m = _FNAME_DATE.search(stem)
    if m:
        y, mo, d = int(m.group(1)[:4]), int(m.group(1)[4:6]), int(m.group(1)[6:8])
        try:
            return datetime(y, mo, d, 0, 0, 0)
        except ValueError:
            pass
    return None


def _exif_sorted_candidate_pairs(
    group: list[Path],
    exif_window_seconds: int,
) -> list[tuple[Path, Path]]:
    """
    Sort paths by EXIF time (oldest first; no-EXIF at end). Return only pairs
    that fall within exif_window_seconds (sliding window) AND whose file sizes
    differ by at most 5% of the smaller one. This lets us skip hashing for
    images that are far apart in time OR clearly different in size.

    - Pairs where both have EXIF and are within the time window AND size window
      are considered candidates.
    - No-EXIF images are compared only with each other (all pairs in the
      no-EXIF suffix), still respecting the 5% size tolerance.
    """
    if exif_window_seconds <= 0 or len(group) < 2:
        return []
    exif_times: dict[Path, datetime | None] = {p: get_exif_datetime(p) for p in group}
    sizes: dict[Path, int] = {}
    for p in group:
        try:
            sizes[p] = p.stat().st_size
        except OSError:
            sizes[p] = 0

    def _size_close(p1: Path, p2: Path) -> bool:
        s1, s2 = sizes.get(p1, 0), sizes.get(p2, 0)
        if s1 <= 0 or s2 <= 0:
            return True  # if size unknown, do not filter by size
        m = min(s1, s2)
        return abs(s1 - s2) <= m * 0.05
    # Sort: has EXIF first by time, then no EXIF (by path for stability)
    def _key(p: Path) -> tuple[bool, datetime, str]:
        t = exif_times[p]
        if t is None:
            return (True, datetime.min, str(p))  # no-EXIF after
        return (False, t, str(p))

    sorted_paths = sorted(group, key=_key)
    # Split into EXIF prefix and no-EXIF suffix
    exif_prefix: list[Path] = [p for p in sorted_paths if exif_times[p] is not None]
    no_exif_suffix: list[Path] = [p for p in sorted_paths if exif_times[p] is None]
    pairs: list[tuple[Path, Path]] = []
    # Sliding window over EXIF prefix
    for i in range(len(exif_prefix)):
        t_i = exif_times[exif_prefix[i]]
        assert t_i is not None
        for j in range(i + 1, len(exif_prefix)):
            t_j = exif_times[exif_prefix[j]]
            assert t_j is not None
            if (t_j - t_i).total_seconds() > exif_window_seconds:
                break
            if _size_close(exif_prefix[i], exif_prefix[j]):
                pairs.append((exif_prefix[i], exif_prefix[j]))
    # All pairs among no-EXIF
    for i in range(len(no_exif_suffix)):
        for j in range(i + 1, len(no_exif_suffix)):
            if _size_close(no_exif_suffix[i], no_exif_suffix[j]):
                pairs.append((no_exif_suffix[i], no_exif_suffix[j]))
    return pairs


def find_similar_groups(
    root: Path,
    level: int = 2,
    threshold: int | None = None,
    recursive: bool = True,
) -> list[list[Path]]:
    """
    Scan images under root and group by similarity level.

    :param root: Root directory to scan
    :param level: Similarity level 1/2/3
    :param threshold: Hamming distance threshold; None uses level default
    :param recursive: Whether to recurse into subdirectories
    :return: List of groups, each a sorted list of paths (at least 2 per group)
    """
    paths = _collect_image_paths(root, recursive=recursive)
    if len(paths) < 2:
        return []

    ham = threshold if threshold is not None else LEVEL_DEFAULTS.get(level, LEVEL_DEFAULTS[2])
    hashes: dict[Path, imagehash.ImageHash] = {}
    for p in paths:
        h = _compute_phash(p)
        if h is not None:
            hashes[p] = h

    # Union-find: group paths with Hamming distance <= ham
    parent: dict[Path, Path] = {p: p for p in hashes}

    def find(x: Path) -> Path:
        if parent[x] != x:
            parent[x] = find(parent[x])
        return parent[x]

    def union(x: Path, y: Path) -> None:
        px, py = find(x), find(y)
        if px != py:
            parent[px] = py

    items = list(hashes.items())
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            p1, h1 = items[i]
            p2, h2 = items[j]
            if h1 - h2 <= ham:
                union(p1, p2)

    # Group by root, keep only groups with at least 2 images
    groups: dict[Path, list[Path]] = {}
    for p in hashes:
        r = find(p)
        groups.setdefault(r, []).append(p)
    return [sorted(g) for g in groups.values() if len(g) >= 2]


def iter_similar_pairs(
    root: Path,
    level: int = 2,
    threshold: int | None = None,
    recursive: bool = True,
) -> Iterator[tuple[Path, Path]]:
    """Yield similar pairs (each pair from a group emitted once)."""
    for group in find_similar_groups(root, level=level, threshold=threshold, recursive=recursive):
        for i in range(len(group)):
            for j in range(i + 1, len(group)):
                yield group[i], group[j]


def find_similar_pairs_with_scores(
    root: Path,
    level: int = 2,
    threshold: int | None = None,
    recursive: bool = True,
    progress_callback: ProgressCallback | None = None,
    fast_same_folder_only: bool = False,
    time_window_seconds: int | None = None,
    exif_time_window_seconds: int | None = None,
    num_threads: int | None = None,
) -> list[tuple[Path, Path, float]]:
    """
    Single directory: return all similar pairs with similarity score.
    Returns [(path_a, path_b, similarity), ...], similarity in [0, 1].
    progress_callback(phase, current, total, detail, result=None): in "compare" phase result is similarity.
    fast_same_folder_only: when recursive, only compare images in the same subfolder (no cross-folder pairs).
    time_window_seconds: only compare two images if filename-derived timestamps are within this many seconds (None = no filter). When fast_same_folder_only is True, caller typically passes 1 if user did not set --time-window (default 1s for --fast).
    exif_time_window_seconds: when fast_same_folder_only and this is set, images are sorted by EXIF time and only pairs within this many seconds are compared (faster than O(n^2) per folder).
    num_threads: when >1, use threads (--fast: one task per subdir; else: parallel hash + parallel compare by pair chunks). None = use default_threads().
    """
    paths = _collect_image_paths(root, recursive=recursive)
    if len(paths) < 2:
        return []
    ham = threshold if threshold is not None else LEVEL_DEFAULTS.get(level, LEVEL_DEFAULTS[2])
    threads = num_threads if num_threads is not None else default_threads()

    def _should_compare_by_time(p1: Path, p2: Path) -> bool:
        if time_window_seconds is None:
            return True
        t1, t2 = parse_datetime_from_filename(p1), parse_datetime_from_filename(p2)
        if t1 is None or t2 is None:
            return True  # compare when timestamp unknown
        return abs((t1 - t2).total_seconds()) <= time_window_seconds

    if fast_same_folder_only and recursive:
        by_dir: dict[Path, list[Path]] = {}
        for p in paths:
            by_dir.setdefault(p.parent, []).append(p)
        total_hash = len(paths)
        # When EXIF time window is set, we only compare candidate pairs (sorted by EXIF, sliding window)
        use_exif_window = exif_time_window_seconds is not None and exif_time_window_seconds > 0
        dir_candidate_pairs: dict[Path, list[tuple[Path, Path]]] = {}
        if use_exif_window:
            for _dir, group in by_dir.items():
                dir_candidate_pairs[_dir] = _exif_sorted_candidate_pairs(group, exif_time_window_seconds)
            total_pairs = sum(len(cp) for cp in dir_candidate_pairs.values())
        else:
            total_pairs = sum(max(0, len(g) * (len(g) - 1) // 2) for g in by_dir.values())

        if threads <= 1:
            # Single-threaded fast path
            hash_idx = 0
            all_hashes: dict[Path, imagehash.ImageHash] = {}
            for _dir, group in sorted(by_dir.items(), key=lambda x: str(x[0])):
                for p in group:
                    if progress_callback:
                        progress_callback("hash", hash_idx, total_hash, p)
                    hash_idx += 1
                    h = _compute_phash(p)
                    if h is not None:
                        all_hashes[p] = h
            if use_exif_window:
                total_pairs = sum(len(cp) for cp in dir_candidate_pairs.values())
            else:
                total_pairs = 0
                for g in by_dir.values():
                    cnt = sum(1 for p in g if p in all_hashes)
                    total_pairs += max(0, cnt * (cnt - 1) // 2)
            result = []
            pair_idx = 0
            for _dir, group in sorted(by_dir.items(), key=lambda x: str(x[0])):
                if use_exif_window:
                    for p1, p2 in dir_candidate_pairs.get(_dir, []):
                        h1, h2 = all_hashes.get(p1), all_hashes.get(p2)
                        if h1 is None or h2 is None:
                            continue
                        d = h1 - h2
                        sim = similarity_score(h1, h2)
                        if progress_callback and total_pairs > 0:
                            progress_callback("compare", pair_idx, total_pairs, (p1, p2), sim)
                        pair_idx += 1
                        if d <= ham:
                            result.append((p1, p2, sim))
                else:
                    items = [(p, all_hashes[p]) for p in group if p in all_hashes]
                    for i in range(len(items)):
                        for j in range(i + 1, len(items)):
                            p1, h1 = items[i]
                            p2, h2 = items[j]
                            if not _should_compare_by_time(p1, p2):
                                continue
                            d = h1 - h2
                            sim = similarity_score(h1, h2)
                            if progress_callback and total_pairs > 0:
                                progress_callback("compare", pair_idx, total_pairs, (p1, p2), sim)
                            pair_idx += 1
                            if d <= ham:
                                result.append((p1, p2, sim))
            return result

        # Multithreaded by subdirectory
        lock = threading.Lock()
        hash_done: list[int] = [0]
        pair_done: list[int] = [0]

        def process_one_folder(args: tuple[Path, list[Path], list[tuple[Path, Path]] | None]) -> list[tuple[Path, Path, float]]:
            if use_exif_window:
                _dir, group, candidate_pairs = args  # type: ignore[misc]
            else:
                _dir, group = args[0], args[1]
                candidate_pairs = None
            local_hashes: dict[Path, imagehash.ImageHash] = {}
            for p in group:
                h = _compute_phash(p)
                if h is not None:
                    local_hashes[p] = h
                if progress_callback:
                    with lock:
                        hash_done[0] += 1
                        progress_callback("hash", hash_done[0] - 1, total_hash, p)
            local_result: list[tuple[Path, Path, float]] = []
            if use_exif_window and candidate_pairs is not None:
                for p1, p2 in candidate_pairs:
                    h1, h2 = local_hashes.get(p1), local_hashes.get(p2)
                    if h1 is None or h2 is None:
                        continue
                    d = h1 - h2
                    sim = similarity_score(h1, h2)
                    if progress_callback and total_pairs > 0:
                        with lock:
                            pair_done[0] += 1
                            progress_callback("compare", pair_done[0] - 1, total_pairs, (p1, p2), sim)
                    if d <= ham:
                        local_result.append((p1, p2, sim))
            else:
                items = list(local_hashes.items())
                for i in range(len(items)):
                    for j in range(i + 1, len(items)):
                        p1, h1 = items[i]
                        p2, h2 = items[j]
                        if not _should_compare_by_time(p1, p2):
                            continue
                        d = h1 - h2
                        sim = similarity_score(h1, h2)
                        if progress_callback and total_pairs > 0:
                            with lock:
                                pair_done[0] += 1
                                progress_callback("compare", pair_done[0] - 1, total_pairs, (p1, p2), sim)
                        if d <= ham:
                            local_result.append((p1, p2, sim))
            return local_result

        result = []
        tasks = sorted(by_dir.items(), key=lambda x: str(x[0]))
        if use_exif_window:
            task_args = [(d, g, dir_candidate_pairs.get(d, [])) for d, g in tasks]
        else:
            task_args = [(d, g, None) for d, g in tasks]
        with ThreadPoolExecutor(max_workers=threads) as executor:
            for future in as_completed(executor.submit(process_one_folder, a) for a in task_args):
                result.extend(future.result())
        return result

    # Non-fast: flat list, compare all pairs (with optional time window)
    if threads <= 1:
        hashes: dict[Path, imagehash.ImageHash] = {}
        total_hash = len(paths)
        for i, p in enumerate(paths):
            if progress_callback:
                progress_callback("hash", i, total_hash, p)
            h = _compute_phash(p)
            if h is not None:
                hashes[p] = h
        items = list(hashes.items())
        n = len(items)
        total_pairs = n * (n - 1) // 2 if n >= 2 else 0
        result = []
        pair_idx = 0
        for i in range(len(items)):
            for j in range(i + 1, len(items)):
                p1, h1 = items[i]
                p2, h2 = items[j]
                if not _should_compare_by_time(p1, p2):
                    continue
                d = h1 - h2
                sim = similarity_score(h1, h2)
                if progress_callback and total_pairs > 0:
                    progress_callback("compare", pair_idx, total_pairs, (p1, p2), sim)
                pair_idx += 1
                if d <= ham:
                    result.append((p1, p2, sim))
        return result

    # Multithreaded non-fast: parallel hash by file, then parallel compare by pair chunks
    total_hash = len(paths)
    hash_done: list[int] = [0]
    lock = threading.Lock()

    def hash_one(p: Path) -> tuple[Path, imagehash.ImageHash | None]:
        h = _compute_phash(p)
        if progress_callback:
            with lock:
                hash_done[0] += 1
                progress_callback("hash", hash_done[0] - 1, total_hash, p)
        return (p, h)

    hashes = {}
    with ThreadPoolExecutor(max_workers=threads) as executor:
        for p, h in executor.map(hash_one, paths):
            if h is not None:
                hashes[p] = h
    items = list(hashes.items())
    n = len(items)
    total_pairs = n * (n - 1) // 2 if n >= 2 else 0
    pair_indices: list[tuple[int, int]] = []
    for i in range(n):
        for j in range(i + 1, n):
            pair_indices.append((i, j))

    def compare_chunk(indices: list[tuple[int, int]]) -> list[tuple[Path, Path, float]]:
        chunk_result: list[tuple[Path, Path, float]] = []
        for idx, (ii, jj) in enumerate(indices):
            p1, h1 = items[ii]
            p2, h2 = items[jj]
            if not _should_compare_by_time(p1, p2):
                continue
            d = h1 - h2
            sim = similarity_score(h1, h2)
            if progress_callback and total_pairs > 0:
                with lock:
                    pair_done[0] += 1
                    progress_callback("compare", pair_done[0] - 1, total_pairs, (p1, p2), sim)
            if d <= ham:
                chunk_result.append((p1, p2, sim))
        return chunk_result

    pair_done = [0]
    chunk_size = max(1, (len(pair_indices) + threads - 1) // threads)
    chunks = [
        pair_indices[k : k + chunk_size]
        for k in range(0, len(pair_indices), chunk_size)
    ]
    result = []
    with ThreadPoolExecutor(max_workers=threads) as executor:
        for future in as_completed(executor.submit(compare_chunk, c) for c in chunks):
            result.extend(future.result())
    return result


def compare_dirs(
    dir1: Path,
    dir2: Path,
    level: int = 2,
    threshold: int | None = None,
    recursive: bool = True,
    progress_callback: ProgressCallback | None = None,
) -> list[tuple[Path, Path, float]]:
    """
    Two directories: compare and return similar pairs at or above the given level.
    Returns [(path_in_dir1, path_in_dir2, similarity), ...].
    progress_callback same as find_similar_pairs_with_scores (result = similarity in "compare" phase).
    """
    paths1 = _collect_image_paths(dir1.resolve(), recursive=recursive)
    paths2 = _collect_image_paths(dir2.resolve(), recursive=recursive)
    if not paths1 or not paths2:
        return []
    ham = threshold if threshold is not None else LEVEL_DEFAULTS.get(level, LEVEL_DEFAULTS[2])
    hashes1: dict[Path, imagehash.ImageHash] = {}
    for i, p in enumerate(paths1):
        if progress_callback:
            progress_callback("hash", i, len(paths1) + len(paths2), p)
        h = _compute_phash(p)
        if h is not None:
            hashes1[p] = h
    hashes2: dict[Path, imagehash.ImageHash] = {}
    for i, p in enumerate(paths2):
        if progress_callback:
            progress_callback("hash", len(paths1) + i, len(paths1) + len(paths2), p)
        h = _compute_phash(p)
        if h is not None:
            hashes2[p] = h
    total_pairs = len(hashes1) * len(hashes2)
    result: list[tuple[Path, Path, float]] = []
    pair_idx = 0
    for p1, h1 in hashes1.items():
        for p2, h2 in hashes2.items():
            d = h1 - h2
            sim = similarity_score(h1, h2)
            if progress_callback and total_pairs > 0:
                progress_callback("compare", pair_idx, total_pairs, (p1, p2), sim)
            pair_idx += 1
            if d <= ham:
                result.append((p1, p2, sim))
    return result


def compare_two_images(path1: Path, path2: Path) -> tuple[float, int] | None:
    """
    Two images: compute similarity.
    Returns (similarity [0,1], hamming_distance), or None if either image cannot be read.
    """
    h1 = _compute_phash(path1)
    h2 = _compute_phash(path2)
    if h1 is None or h2 is None:
        return None
    d = h1 - h2
    return (similarity_score(h1, h2), d)


def pairs_to_groups(pairs: list[tuple[Path, Path, float]]) -> list[list[Path]]:
    """Merge pair list into groups (union-find); each group is deduplicated and sorted."""
    if not pairs:
        return []
    parent: dict[Path, Path] = {}

    def find(x: Path) -> Path:
        if x not in parent:
            parent[x] = x
        if parent[x] != x:
            parent[x] = find(parent[x])
        return parent[x]

    def union(x: Path, y: Path) -> None:
        px, py = find(x), find(y)
        if px != py:
            parent[px] = py

    for p1, p2, _ in pairs:
        union(p1, p2)
    groups: dict[Path, list[Path]] = {}
    for p in parent:
        r = find(p)
        groups.setdefault(r, []).append(p)
    return [sorted(g) for g in groups.values() if len(g) >= 2]


def get_files_to_delete_from_groups(
    groups: list[list[Path]],
    keep: str,
) -> list[Path]:
    """
    For each group, decide which path to keep and which to delete by EXIF time.
    keep == "newer": keep the image with newest EXIF in each group; others go to delete list.
    keep == "older": keep the image with oldest EXIF in each group; others go to delete list.
    No EXIF is treated as oldest (deleted when keep newer, kept when keep older).
    """
    from .exif_utils import get_exif_datetime

    to_delete: list[Path] = []
    for group in groups:
        def _key(p: Path):
            t = get_exif_datetime(p)
            return (0 if t is None else 1, t or datetime.min)

        sorted_group = sorted(group, key=_key)
        if keep == "newer":
            keep_path = sorted_group[-1]
        else:
            keep_path = sorted_group[0]
        to_delete.extend(p for p in group if p != keep_path)
    return to_delete
