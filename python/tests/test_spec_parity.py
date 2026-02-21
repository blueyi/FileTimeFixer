"""Tests loaded from shared test_spec/ (YAML). Ensures Python and C++ behavior match."""

import pytest
from pathlib import Path

# test_spec 位于仓库根目录，相对于 python/tests/ 为 ../../test_spec
SPEC_DIR = Path(__file__).resolve().parent.parent.parent / "test_spec"


def _load_yaml(name: str) -> dict:
    try:
        import yaml
    except ImportError:
        pytest.skip("PyYAML not installed")
    path = SPEC_DIR / name
    if not path.exists():
        pytest.skip(f"test_spec not found: {path}")
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f)


@pytest.fixture(scope="module")
def time_parse_spec():
    return _load_yaml("time_parse.yaml")


@pytest.fixture(scope="module")
def target_resolver_spec():
    return _load_yaml("target_resolver.yaml")


def test_time_parse_from_spec(time_parse_spec):
    """文件名时间解析：与 test_spec/time_parse.yaml 一致."""
    from filetimefixer.time_parse import parse_file_name_time
    cases = time_parse_spec.get("cases") or []
    for c in cases:
        filename = c["filename"]
        expected = c["expected"]
        got = parse_file_name_time(filename)
        assert got == expected, f"parse_file_name_time({filename!r}) => {got!r}, expected {expected!r}"


def test_target_resolver_from_spec(target_resolver_spec):
    """目标时间解析：与 test_spec/target_resolver.yaml 一致."""
    from filetimefixer.target_time_resolver import resolve_target_time, TargetTimeScenario, scenario_name
    scenario_map = {s.value: s for s in TargetTimeScenario}
    cases = target_resolver_spec.get("cases") or []
    for c in cases:
        name_time = c.get("name_time", "")
        exif_time = c.get("exif_time", "")
        expected_target = c.get("expected_target", "")
        expected_scenario_str = c.get("expected_scenario", "None")
        expected_scenario = scenario_map.get(expected_scenario_str, TargetTimeScenario.None_)
        result = resolve_target_time(name_time, exif_time)
        assert result.target_time == expected_target, (
            f"resolve_target_time({name_time!r}, {exif_time!r}) => target {result.target_time!r}, expected {expected_target!r}"
        )
        assert result.scenario == expected_scenario, (
            f"resolve_target_time(...) => scenario {scenario_name(result.scenario)}, expected {expected_scenario_str}"
        )
