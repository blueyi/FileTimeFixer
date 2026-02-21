"""Tests for image file detection."""

import pytest
from pathlib import Path

from filetimefixer.image_util import is_image_file, IMAGE_EXTENSIONS


@pytest.mark.parametrize(
    "ext",
    [".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp", ".heic", ".raw"],
)
def test_is_image_file_true(ext: str) -> None:
    assert is_image_file(Path(f"photo{ext}")) is True
    assert is_image_file(Path(f"photo{ext.upper()}")) is True


@pytest.mark.parametrize(
    "name",
    [".txt", ".pdf", ".mp4", ".json", ""],
)
def test_is_image_file_false(name: str) -> None:
    p = Path(f"file{name}") if name else Path("noext")
    assert is_image_file(p) is (p.suffix.lower() in IMAGE_EXTENSIONS)
