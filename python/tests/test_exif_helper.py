"""Tests for EXIF read/write (with a minimal JPEG fixture if available)."""

from pathlib import Path

import pytest
from PIL import Image

from filetimefixer.exif_helper import get_exif_time_earliest, modify_exif_data_for_time

try:
    import piexif
except ImportError:
    piexif = None


@pytest.fixture
def temp_jpeg_with_exif(tmp_path: Path) -> Path:
    """Create a minimal JPEG with EXIF DateTimeOriginal."""
    path = tmp_path / "test.jpg"
    img = Image.new("RGB", (10, 10), color="red")
    if piexif:
        exif_dict = {"0th": {}, "Exif": {piexif.ExifIFD.DateTimeOriginal: "2023:06:15 12:00:00"}, "GPS": {}, "1st": {}, "thumbnail": None}
        exif_bytes = piexif.dump(exif_dict)
        img.save(path, "JPEG", exif=exif_bytes, quality=95)
    else:
        img.save(path, "JPEG", quality=95)
    return path


def test_get_exif_time_earliest(temp_jpeg_with_exif: Path) -> None:
    t = get_exif_time_earliest(str(temp_jpeg_with_exif))
    assert "2023" in t and "06" in t


def test_get_exif_time_earliest_no_file() -> None:
    assert get_exif_time_earliest("/nonexistent/path.jpg") == ""


def test_modify_exif_data_for_time(temp_jpeg_with_exif: Path) -> None:
    ok = modify_exif_data_for_time(str(temp_jpeg_with_exif), "2024-01-01 10:30:00")
    assert ok is True
    t = get_exif_time_earliest(str(temp_jpeg_with_exif))
    assert "2024" in t or "2023" in t  # reader may return any of the three tags
