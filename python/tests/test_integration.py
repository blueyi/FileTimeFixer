"""Integration tests: resolver + time_parse + time_convert together."""

import pytest
from pathlib import Path

from filetimefixer import (
    parse_file_name_time,
    exif_datetime_to_utc_string,
    format_time_to_utc8_name,
    resolve_target_time,
    scenario_name,
    TargetTimeScenario,
)
from filetimefixer.image_util import is_image_file


def test_full_flow_name_only() -> None:
    name = "IMG_20231111_193849.jpg"
    name_time = parse_file_name_time(name)
    assert name_time == "2023-11-11 19:38:49"
    result = resolve_target_time(name_time, "")
    assert result.target_time == name_time
    assert result.scenario == TargetTimeScenario.NameOnly
    formatted = format_time_to_utc8_name(result.target_time)
    assert "20231111" in formatted and "193849" in formatted


def test_full_flow_both_earliest() -> None:
    name_time = "2023-10-23 15:30:00"
    exif_time = "2023-10-23T14:00:00"
    result = resolve_target_time(name_time, exif_time)
    assert result.target_time == exif_time
    assert result.scenario == TargetTimeScenario.BothUseEarliest
