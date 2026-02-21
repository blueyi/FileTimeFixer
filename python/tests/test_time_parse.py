"""Tests for filename time parsing (parse_file_name_time)."""

import pytest

from filetimefixer.time_parse import parse_file_name_time


@pytest.mark.parametrize(
    "filename,expected",
    [
        ("20160331_202334.jpg", "2016-03-31 20:23:34"),
        ("IMG_20231111_193849.jpg", "2023-11-11 19:38:49"),
        ("VID_20210801_171003.jpg", "2021-08-01 17:10:03"),
        ("PANO_20231001_143241.jpg", "2023-10-01 14:32:41"),
        ("MTXX_PT20230623_190638417.jpg", "2023-06-23 19:06:38"),
        ("mmexport1568301595980.jpg", "2019-09-13 10:39:55.980"),
        ("mmexport1602999370599.jpg", "2020-10-18 08:36:10.599"),
        ("MEITU_20240807_123043882.jpg", "2024-08-07 12:30:43"),
        ("pt2021_10_23_21_52_39.jpg", "2021-10-23 21:52:39"),
        ("Screenshot_2021-03-25-01-12-43-235_com.tencent.mm.jpg", "2021-03-25 01:12:43"),
        ("wx_camera_1719390504866.jpg", "2024-06-25 22:28:24.866"),
        ("1605199092110.jpeg", "2020-07-14 15:18:12.110"),
        ("20220115-wczt.jpg", "2022-01-15"),
        ("l00972450_1543624986659.jpg", "2018-12-31 08:43:06.659"),
        ("20220115.jpg", "2022-01-15"),
        ("mmexport1620111487858.jpg", "2021-05-04 08:58:07.858"),
        ("nonsense.txt", ""),
        ("no_digits_here.png", ""),
    ],
)
def test_parse_file_name_time(filename: str, expected: str) -> None:
    got = parse_file_name_time(filename)
    assert got == expected, f"parse_file_name_time({filename!r}) => {got!r}, expected {expected!r}"
