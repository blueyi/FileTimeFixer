"""Read EXIF datetime for dedupe (keep newer/older)."""

from datetime import datetime
from pathlib import Path

# EXIF tag IDs for time (Pillow / standard)
DATETIME_ORIGINAL = 36867
DATETIME_DIGITIZED = 36868
IMAGE_DATETIME = 306
EXIF_DATETIME_FMT = "%Y:%m:%d %H:%M:%S"


def get_exif_datetime(path: Path) -> datetime | None:
    """
    Read capture time from image EXIF (prefer DateTimeOriginal).
    Returns datetime or None (no EXIF or parse failure).
    """
    try:
        from PIL import Image
    except ImportError:
        return None
    if not path.is_file():
        return None
    try:
        with Image.open(path) as img:
            exif = img.getexif()
            if exif is None:
                return None
            for tag_id in (DATETIME_ORIGINAL, DATETIME_DIGITIZED, IMAGE_DATETIME):
                val = exif.get(tag_id)
                if val and isinstance(val, str):
                    val = val.strip()
                    if val:
                        try:
                            return datetime.strptime(val, EXIF_DATETIME_FMT)
                        except ValueError:
                            continue
    except Exception:
        pass
    return None
