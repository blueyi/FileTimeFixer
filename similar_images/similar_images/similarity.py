"""Perceptual hash based similarity with level-based thresholds."""

from datetime import datetime
from pathlib import Path
from typing import Callable, Iterator

# progress_callback(phase, current, total, detail, result=None); in "compare" phase result is similarity float
ProgressCallback = Callable[[str, int, int, Path | tuple[Path, Path] | None, float | None], None]

import imagehash
from PIL import Image

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
) -> list[tuple[Path, Path, float]]:
    """
    Single directory: return all similar pairs with similarity score.
    Returns [(path_a, path_b, similarity), ...], similarity in [0, 1].
    progress_callback(phase, current, total, detail, result=None): in "compare" phase result is similarity.
    """
    paths = _collect_image_paths(root, recursive=recursive)
    if len(paths) < 2:
        return []
    ham = threshold if threshold is not None else LEVEL_DEFAULTS.get(level, LEVEL_DEFAULTS[2])
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
    result: list[tuple[Path, Path, float]] = []
    pair_idx = 0
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            p1, h1 = items[i]
            p2, h2 = items[j]
            d = h1 - h2
            sim = similarity_score(h1, h2)
            if progress_callback and total_pairs > 0:
                progress_callback("compare", pair_idx, total_pairs, (p1, p2), sim)
            pair_idx += 1
            if d <= ham:
                result.append((p1, p2, sim))
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
