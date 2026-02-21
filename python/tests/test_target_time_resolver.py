"""Tests for target time resolution scenarios (resolve_target_time)."""

import pytest

from filetimefixer.target_time_resolver import (
    TargetTimeScenario,
    ResolveResult,
    resolve_target_time,
    scenario_name,
)


@pytest.mark.parametrize(
    "name_time,exif_time,expected_target,expected_scenario",
    [
        ("", "", "", TargetTimeScenario.None_),
        ("2023-10-23 15:30:00", "", "2023-10-23 15:30:00", TargetTimeScenario.NameOnly),
        ("", "2023-10-23T14:00:00", "2023-10-23T14:00:00", TargetTimeScenario.ExifOnly),
        ("2023-10-23 15:30:00", "2023-10-23T14:00:00", "2023-10-23T14:00:00", TargetTimeScenario.BothUseEarliest),
        ("2023-10-23 10:00:00", "2023-10-23T15:00:00", "2023-10-23 10:00:00", TargetTimeScenario.BothUseEarliest),
        ("2023-10-23 12:00:00", "2009-06-01T12:00:00", "2023-10-23 12:00:00", TargetTimeScenario.ExifTooOldUseName),
        ("2023-10-23 15:30:00", "2023-10-23T00:00:00", "2023-10-23 15:30:00", TargetTimeScenario.SameDayExifMidnightUseName),
        ("2023-10-23 00:00:00", "2023-10-23T14:30:00", "2023-10-23T14:30:00", TargetTimeScenario.SameDayNameMidnightUseExif),
        ("2023-10-23 14:30:00", "2023-10-23T14:30:00", "2023-10-23T14:30:00", TargetTimeScenario.SameDayBothFullUseMorePrecise),
        ("2023-10-23 14:30:01", "2023-10-23T14:30:00", "2023-10-23 14:30:01", TargetTimeScenario.SameDayBothFullUseMorePrecise),
        ("2024-11-12", "2024-11-12T15:18:32", "2024-11-12T15:18:32", TargetTimeScenario.SameDayNameDateOnlyUseExif),
        ("2024-11-12 10:00:00", "2024-11-12", "2024-11-12 10:00:00", TargetTimeScenario.SameDayExifDateOnlyUseName),
    ],
)
def test_resolve_target_time(
    name_time: str,
    exif_time: str,
    expected_target: str,
    expected_scenario: TargetTimeScenario,
) -> None:
    result = resolve_target_time(name_time, exif_time)
    assert isinstance(result, ResolveResult)
    assert result.target_time == expected_target, (
        f"resolve_target_time({name_time!r}, {exif_time!r}) => target {result.target_time!r}, expected {expected_target!r}"
    )
    assert result.scenario == expected_scenario, (
        f"resolve_target_time(...) => scenario {result.scenario}, expected {expected_scenario}"
    )


@pytest.mark.parametrize(
    "scenario,expected_name",
    [
        (TargetTimeScenario.None_, "None"),
        (TargetTimeScenario.NameOnly, "NameOnly"),
        (TargetTimeScenario.ExifOnly, "ExifOnly"),
        (TargetTimeScenario.BothUseEarliest, "BothUseEarliest"),
        (TargetTimeScenario.ExifTooOldUseName, "ExifTooOldUseName"),
        (TargetTimeScenario.SameDayExifMidnightUseName, "SameDayExifMidnightUseName"),
        (TargetTimeScenario.SameDayNameMidnightUseExif, "SameDayNameMidnightUseExif"),
        (TargetTimeScenario.SameDayBothFullUseMorePrecise, "SameDayBothFullUseMorePrecise"),
        (TargetTimeScenario.SameDayNameDateOnlyUseExif, "SameDayNameDateOnlyUseExif"),
        (TargetTimeScenario.SameDayExifDateOnlyUseName, "SameDayExifDateOnlyUseName"),
    ],
)
def test_scenario_name(scenario: TargetTimeScenario, expected_name: str) -> None:
    assert scenario_name(scenario) == expected_name
