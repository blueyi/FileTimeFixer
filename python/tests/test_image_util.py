"""Tests for image and video file detection."""

import pytest
from pathlib import Path

from filetimefixer.image_util import (
    is_image_file,
    is_video_file,
    is_media_file,
    IMAGE_EXTENSIONS,
    VIDEO_EXTENSIONS,
)


@pytest.mark.parametrize(
    "ext",
    [".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp", ".heic", ".raw"],
)
def test_is_image_file_true(ext: str) -> None:
    assert is_image_file(Path(f"photo{ext}")) is True
    assert is_image_file(Path(f"photo{ext.upper()}")) is True


@pytest.mark.parametrize(
    "ext",
    [".mp4", ".mov", ".avi", ".mkv", ".m4v", ".webm", ".wmv", ".3gp"],
)
def test_is_video_file_true(ext: str) -> None:
    assert is_video_file(Path(f"clip{ext}")) is True
    assert is_video_file(Path(f"clip{ext.upper()}")) is True


@pytest.mark.parametrize(
    "name",
    [".txt", ".pdf", ".json", ""],
)
def test_is_image_file_false(name: str) -> None:
    p = Path(f"file{name}") if name else Path("noext")
    assert is_image_file(p) is (p.suffix.lower() in IMAGE_EXTENSIONS)


@pytest.mark.parametrize(
    "path",
    ["photo.jpg", "clip.mp4", "vid.MOV", "img.PNG"],
)
def test_is_media_file_true(path: str) -> None:
    assert is_media_file(Path(path)) is True


@pytest.mark.parametrize(
    "path",
    ["doc.pdf", "data.json", "noext"],
)
def test_is_media_file_false(path: str) -> None:
    assert is_media_file(Path(path)) is False
