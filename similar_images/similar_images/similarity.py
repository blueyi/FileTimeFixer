"""Perceptual hash based similarity with level-based thresholds."""

from datetime import datetime
from pathlib import Path
from typing import Callable, Iterator

# progress_callback(phase, current, total, detail, result=None)；compare 阶段 result 为相似度 float
ProgressCallback = Callable[[str, int, int, Path | tuple[Path, Path] | None, float | None], None]

import imagehash
from PIL import Image

# 支持的图片扩展名（与主项目 image_util 对齐）
IMAGE_EXTENSIONS = {
    ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".tif",
    ".webp", ".heic", ".raw",
}

# 等级默认汉明距离阈值（pHash）：1=严格 2=中等 3=宽松
LEVEL_DEFAULTS = {
    1: 5,
    2: 15,
    3: 25,
}

# 默认 pHash 位数（16*16），用于将汉明距离转为相似度 [0,1]
HASH_BITS = 256


def _is_image(path: Path) -> bool:
    return path.suffix.lower() in IMAGE_EXTENSIONS


def similarity_score(h1: imagehash.ImageHash, h2: imagehash.ImageHash, bits: int = HASH_BITS) -> float:
    """汉明距离转相似度，返回 [0, 1]，1 表示完全相同。"""
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
    在 root 目录下扫描图片，按相似度等级分组相似图片。

    :param root: 扫描根目录
    :param level: 相似度等级 1/2/3
    :param threshold: 汉明距离阈值，若为 None 则使用等级默认值
    :param recursive: 是否递归子目录
    :return: 每组为相似图片路径列表（至少 2 张才成组）
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

    # 并查集：把汉明距离 <= ham 的归为一组
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

    # 按根归类，只保留至少 2 张的组
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
    """逐对产出相似图片（同一组内两两配对只产一次）。"""
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
    单目录：输出所有相似图片对及相似度。
    返回 [(path_a, path_b, similarity), ...]，similarity 为 [0, 1]。
    progress_callback(phase, current, total, detail, result=None)：compare 阶段 result 为相似度 [0,1]。
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
    两目录：对比两个目录下在指定等级及以上的相似图片。
    返回 [(path_in_dir1, path_in_dir2, similarity), ...]。
    progress_callback 同 find_similar_pairs_with_scores（compare 阶段 result 为相似度）。
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
    两图：计算两张图片的相似度。
    返回 (similarity [0,1], hamming_distance)，若任一方无法读图则返回 None。
    """
    h1 = _compute_phash(path1)
    h2 = _compute_phash(path2)
    if h1 is None or h2 is None:
        return None
    d = h1 - h2
    return (similarity_score(h1, h2), d)


def pairs_to_groups(pairs: list[tuple[Path, Path, float]]) -> list[list[Path]]:
    """将相似对列表合并为相似组（并查集），每组内路径去重且排序。"""
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
    根据 EXIF 时间决定每组保留谁、删除谁。
    keep == "newer"：每组保留 EXIF 时间最新的一张，其余加入删除列表。
    keep == "older"：每组保留 EXIF 时间最早的一张，其余加入删除列表。
    无 EXIF 的视为最早（keep newer 时会被删，keep older 时保留）。
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
