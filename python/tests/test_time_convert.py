"""Tests for time conversion (format_time_to_utc8_name, exif_datetime_to_utc_string)."""

import pytest

from filetimefixer.time_convert import (
    format_time_to_utc8_name,
    exif_datetime_to_utc_string,
    utc_string_to_timestamp,
)


@pytest.mark.parametrize(
    "time_str",
    [
        "2023-10-23 15:30:00",
        "2023-10-23T14:00:00",
        "2022-01-15",
    ],
)
def test_format_time_to_utc8_name_valid(time_str: str) -> None:
    got = format_time_to_utc8_name(time_str)
    assert len(got) >= 15, f"Expected YYYYMMDD_HHMMSS format, got {got!r}"
    assert got[:8].isdigit() and got[8:9] == "_" and got[9:15].isdigit(), (
        f"format_time_to_utc8_name({time_str!r}) => {got!r}"
    )


def test_format_time_to_utc8_name_empty() -> None:
    assert format_time_to_utc8_name("") == ""
    assert format_time_to_utc8_name("invalid") == ""


@pytest.mark.parametrize(
    "exif_str,expected_contains",
    [
        ("2023:10:23 15:30:00", "2023-10-23"),
        ("2023-10-23 15:30:00", "2023-10-23"),
    ],
)
def test_exif_datetime_to_utc_string(exif_str: str, expected_contains: str) -> None:
    got = exif_datetime_to_utc_string(exif_str)
    assert expected_contains in got
    assert "T" in got


def test_exif_datetime_to_utc_string_empty() -> None:
    assert exif_datetime_to_utc_string("") == ""


def test_utc_string_to_timestamp() -> None:
    ts = utc_string_to_timestamp("2023-01-01T00:00:00")
    assert ts > 0
    assert utc_string_to_timestamp("") == -1
    assert utc_string_to_timestamp("not-a-date") == -1
