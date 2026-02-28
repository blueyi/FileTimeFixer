"""CLI for similar image detection."""

import argparse
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path

from tqdm import tqdm

from .similarity import (
    find_similar_pairs_with_scores,
    compare_dirs,
    compare_two_images,
    pairs_to_groups,
    get_files_to_delete_from_groups,
    LEVEL_DEFAULTS,
    default_threads,
)
from .exif_utils import get_exif_datetime


def _format_similarity(s: float) -> str:
    return f"{s * 100:.2f}%"


def _make_simple_progress_callback():
    """Simple progress callback: print current file(s)/pair and similarity to stdout and flush."""

    def callback(
        phase: str,
        current: int,
        total: int,
        detail: Path | tuple[Path, Path] | None,
        result: float | None = None,
    ) -> None:
        if total <= 0:
            return
        n = current + 1
        if phase == "hash" and isinstance(detail, Path):
            print(f"Hashing ({n}/{total}): {detail}", flush=True)
        elif phase == "compare" and isinstance(detail, (list, tuple)) and len(detail) >= 2:
            sim_str = f"  -> {result * 100:.2f}%" if result is not None else ""
            print(f"Comparing ({n}/{total}): {detail[0]}{sim_str}", flush=True)
            print(f"         <-> {detail[1]}", flush=True)

    return callback


def _parse_duration_seconds(value: str | None, opt_name: str) -> int | None:
    """
    Parse duration like:
    - plain seconds: "60"
    - or combined form: "1d-2h-3s", "1d", "2h-10s", "1d-10s"
    Returns total seconds or None.
    """
    if value is None:
        return None
    value = value.strip()
    if not value:
        return None
    # plain integer seconds
    if value.isdigit():
        return int(value)
    total = 0
    for part in value.split("-"):
        part = part.strip()
        if not part:
            continue
        unit = part[-1].lower()
        num_str = part[:-1]
        if not num_str.isdigit() or unit not in ("d", "h", "s"):
            print(f"Error: invalid duration for {opt_name}: {value}", file=sys.stderr)
            sys.exit(1)
        n = int(num_str)
        if unit == "d":
            total += n * 86400
        elif unit == "h":
            total += n * 3600
        else:  # "s"
            total += n
    return total


def _make_progress_callback(verbose: bool):
    """Progress callback with tqdm bars; when verbose=False caller uses simple callback instead."""
    if not verbose:
        return None
    hash_bar: tqdm | None = None
    compare_bar: tqdm | None = None

    def callback(
        phase: str,
        current: int,
        total: int,
        detail: Path | tuple[Path, Path] | None,
        result: float | None = None,
    ) -> None:
        nonlocal hash_bar, compare_bar
        if phase == "hash" and total > 0:
            if hash_bar is None:
                hash_bar = tqdm(total=total, unit="img", desc="Hashing", unit_scale=False, dynamic_ncols=True)
            if isinstance(detail, Path):
                hash_bar.set_postfix_str(detail.name[:40], refresh=True)
            hash_bar.update(1)
            if hash_bar.n >= total:
                hash_bar.close()
                hash_bar = None
        elif phase == "compare" and total > 0:
            if hash_bar is not None:
                hash_bar.close()
                hash_bar = None
            if compare_bar is None:
                compare_bar = tqdm(total=total, unit="pair", desc="Comparing", unit_scale=False, dynamic_ncols=True)
            if isinstance(detail, (list, tuple)) and len(detail) >= 2:
                a, b = detail[0].name[:25], detail[1].name[:25]
                sim_str = f" {result*100:.1f}%" if result is not None else ""
                compare_bar.set_postfix_str(f"{a} ~ {b}{sim_str}", refresh=True)
            compare_bar.update(1)
            if compare_bar.n >= total:
                compare_bar.close()
                compare_bar = None

    return callback


def _copy_similar_to_cwd(unique_paths: set[Path], cwd: Path) -> Path:
    """Copy files in the set to similar_images_<timestamp> under cwd; use _2, _3 on name conflict. Returns target dir."""
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = cwd / f"similar_images_{ts}"
    out_dir.mkdir(parents=True, exist_ok=True)
    used: set[str] = set()
    for src in sorted(unique_paths):
        if not src.is_file():
            continue
        name = src.name
        stem, ext = src.stem, src.suffix
        dest = out_dir / name
        n = 1
        while dest.exists() or dest.name in used:
            n += 1
            name = f"{stem}_{n}{ext}"
            dest = out_dir / name
        used.add(dest.name)
        shutil.copy2(src, dest)
    return out_dir


AUTO_RESULT_LOG_SENTINEL = Path("__AUTO_RESULT_LOG__")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Find similar images: single dir (all pairs), two dirs (cross pairs), or two images (score).",
    )
    parser.add_argument(
        "paths",
        type=Path,
        nargs="*",
        default=[],
        help="1 dir / 2 dirs / 2 files (ignored when -r is used)",
    )
    parser.add_argument(
        "--level", "-l",
        type=int,
        choices=[1, 2, 3],
        default=2,
        help="Similarity level for dir mode: 1=strict, 2=medium, 3=loose (default: 2)",
    )
    parser.add_argument(
        "--threshold", "-t",
        type=int,
        default=None,
        metavar="N",
        help="Override Hamming distance threshold (default: from level)",
    )
    parser.add_argument(
        "--no-recursive",
        action="store_true",
        help="Do not scan subdirectories (dir mode only)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show progress bar, percentage and current image(s) being hashed/compared (dir mode only)",
    )
    parser.add_argument(
        "--dedupe",
        choices=["keep-newer", "keep-older"],
        default=None,
        metavar="MODE",
        help="Single-dir only: delete duplicates by EXIF time. keep-newer=delete older, keep-older=delete newer",
    )
    parser.add_argument(
        "--yes", "-y",
        action="store_true",
        help="Do not ask for confirmation when using --dedupe (delete files)",
    )
    parser.add_argument(
        "--copy-similar",
        action="store_true",
        help="Copy all similar images to similar_images_<timestamp> in current working directory",
    )
    parser.add_argument(
        "-r", "--read-from-file",
        type=Path,
        default=None,
        metavar="FILE",
        help="Read pairs from text file; each line: 'path1, path2' (comma space). Output comparison result per line.",
    )
    parser.add_argument(
        "--keep-in-line",
        choices=["first", "second"],
        default=None,
        metavar="WHICH",
        help="With -r: for similar pairs, keep first or second path in each line and delete the other (use with --yes to skip confirm)",
    )
    parser.add_argument(
        "--log-file",
        type=Path,
        default=None,
        metavar="FILE",
        help="With -r: also write comparison status (current pair) to this log file",
    )
    parser.add_argument(
        "--result-log",
        nargs="?",
        type=Path,
        default=None,
        const=AUTO_RESULT_LOG_SENTINEL,
        metavar="FILE",
        help=(
            "Write detected similar-image results and summary to this log file; "
            "omit FILE to auto-name by folder + timestamp (dir/two-dir/two-image modes)."
        ),
    )
    parser.add_argument(
        "--fast",
        action="store_true",
        help="Single-dir only: when recursive, compare only within the same subfolder (faster); use with --time-window to limit by filename timestamp",
    )
    parser.add_argument(
        "--time-window",
        type=str,
        default=None,
        metavar="DUR",
        help="Single-dir only: only compare two images if filename timestamps (YYYYMMDD_HHMMSS) are within this duration. Accepts seconds (e.g. 3600) or forms like 1d-2h-3s.",
    )
    parser.add_argument(
        "--threads",
        type=int,
        default=None,
        metavar="N",
        help="Single-dir only: number of threads. With --fast: one task per subfolder; else: parallel hash by file and parallel compare. Default: auto from CPU count",
    )
    parser.add_argument(
        "--exif-time-window",
        type=str,
        default=None,
        metavar="DUR",
        help="Single-dir only: only keep pairs with EXIF times within this duration (e.g. 3600, 1d-2h-3s). With --fast: sort by EXIF and compare only within window (faster). Without --fast: filter results after scan.",
    )
    parser.add_argument(
        "--del-similar-over",
        type=int,
        default=None,
        metavar="PCT",
        help="Single-dir only: delete images in EXIF-based groups where similarity >= PCT (0-100), keeping oldest EXIF per group",
    )
    parser.add_argument(
        "--force-del-dup",
        action="store_true",
        help="With --del-similar-over: do not ask for confirmation before deleting similar images",
    )
    args = parser.parse_args()

    paths = [p.resolve() for p in args.paths]

    result_log = None
    if getattr(args, "result_log", None) is not None:
        log_target = args.result_log
        if log_target == AUTO_RESULT_LOG_SENTINEL:
            # Auto-generate log file name based on input paths.
            base = "similar_images"
            if len(paths) == 1 and paths[0].is_dir():
                base = paths[0].name or base
            elif len(paths) == 2 and paths[0].is_dir() and paths[1].is_dir():
                base = f"{paths[0].name}_vs_{paths[1].name}"
            elif len(paths) == 2 and paths[0].is_file() and paths[1].is_file():
                base = paths[0].stem or "similar_pair"
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            log_target = Path.cwd() / f"{base}_{ts}.log"
        else:
            log_target = log_target.resolve()
        try:
            result_log = open(log_target, "a", encoding="utf-8")
        except OSError as e:
            print(f"Warning: cannot open result log file {log_target}: {e}", file=sys.stderr)
            result_log = None

    def _log_result(*msgs: str) -> None:
        if result_log is None:
            return
        for m in msgs:
            try:
                result_log.write(m + "\n")
            except OSError:
                pass
        try:
            result_log.flush()
        except OSError:
            pass

    # Directory mode: always show progress on stdout; use tqdm when --verbose else simple line-by-line
    progress_cb = _make_progress_callback(args.verbose) if args.verbose else _make_simple_progress_callback()
    th = args.threshold if args.threshold is not None else LEVEL_DEFAULTS.get(args.level, 15)

    # --- -r mode: read "path1, path2" per line from file and compare ---
    if args.read_from_file is not None:
        rpath = args.read_from_file.resolve()
        if not rpath.is_file():
            print(f"Error: not a file: {rpath}", file=sys.stderr)
            return 1
        lines: list[tuple[Path, Path]] = []
        with open(rpath, encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f, 1):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split(", ")]
                if len(parts) < 2:
                    print(f"Warning: line {i}: expected 'path1, path2', skipped: {line[:60]}", file=sys.stderr)
                    continue
                p1_str = ", ".join(parts[:-1]).strip()
                p2_str = parts[-1].strip()
                p1, p2 = Path(p1_str).resolve(), Path(p2_str).resolve()
                lines.append((p1, p2))
        if not lines:
            print("No valid pairs found in file.")
            return 0
        log_file = None
        if args.log_file is not None:
            try:
                log_file = open(args.log_file.resolve(), "a", encoding="utf-8")
            except OSError as e:
                print(f"Warning: cannot open log file {args.log_file}: {e}", file=sys.stderr)

        def _status_out(*msgs: str) -> None:
            for m in msgs:
                print(m, flush=True)
                if log_file is not None:
                    try:
                        log_file.write(m + "\n")
                        log_file.flush()
                    except OSError:
                        pass
            sys.stdout.flush()

        try:
            results = []
            total = len(lines)
            for idx, (p1, p2) in enumerate(lines):
                _status_out(f"[{idx + 1}/{total}] Comparing: {p1}", f"         <-> {p2}")
                res = compare_two_images(p1, p2)
                if res is None:
                    results.append((p1, p2, None, None))
                else:
                    sim, hamming = res
                    results.append((p1, p2, sim, hamming))
            similar_by_threshold = th
            total_compared = len(results)
            num_similar = sum(
                1 for (_, _, s, h) in results
                if s is not None and h is not None and h <= similar_by_threshold
            )
            to_delete: list[Path] = []
            if args.keep_in_line and results:
                for (p1, p2, sim, hamming) in results:
                    if sim is not None and hamming is not None and hamming <= similar_by_threshold:
                        keep = p1 if args.keep_in_line == "first" else p2
                        drop = p2 if args.keep_in_line == "first" else p1
                        to_delete.append(drop)
            deleted_count = 0
            if to_delete and args.keep_in_line:
                if not args.yes:
                    print(f"About to delete {len(to_delete)} file(s) (keep {args.keep_in_line} in each similar pair):")
                    for p in to_delete[:20]:
                        print(f"  {p}")
                    if len(to_delete) > 20:
                        print(f"  ... and {len(to_delete) - 20} more")
                    try:
                        r = input("Proceed? [y/N]: ").strip().lower()
                    except EOFError:
                        r = "n"
                    if r != "y" and r != "yes":
                        to_delete = []
                    else:
                        for p in to_delete:
                            try:
                                p.unlink(missing_ok=True)
                            except OSError as e:
                                print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                        deleted_count = len(to_delete)
                        print(f"Deleted {deleted_count} file(s).")
                else:
                    for p in to_delete:
                        try:
                            p.unlink(missing_ok=True)
                        except OSError as e:
                            print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                    deleted_count = len(to_delete)
                    print(f"Deleted {deleted_count} file(s).")
            if args.json:
                out = []
                for p1, p2, sim, hamming in results:
                    row = {"path_a": str(p1), "path_b": str(p2)}
                    if sim is not None and hamming is not None:
                        row["similarity"] = round(sim, 4)
                        row["hamming_distance"] = hamming
                        row["similar"] = hamming <= similar_by_threshold
                    else:
                        row["error"] = "could not compare"
                    out.append(row)
                payload = {
                    "results": out,
                    "summary": {
                        "total_compared": total_compared,
                        "similar_count": num_similar,
                        "deleted_count": deleted_count,
                    },
                }
                print(json.dumps(payload, ensure_ascii=False, indent=2))
                return 0
            print(f"Level {args.level} (threshold={th}), {len(results)} pair(s) from file:\n")
            for p1, p2, sim, hamming in results:
                if sim is not None and hamming is not None:
                    similar_mark = " [similar]" if hamming <= similar_by_threshold else ""
                    print(f"  {_format_similarity(sim)} (Hamming {hamming}){similar_mark}")
                else:
                    print("  (compare failed)")
                print(f"    {p1}")
                print(f"    {p2}")
                print()
            _status_out("--- Summary ---", f"Total compared: {total_compared} pair(s)", f"Similar: {num_similar} pair(s)")
            if deleted_count > 0:
                _status_out(f"Deleted: {deleted_count} file(s)")
            return 0
        finally:
            if log_file is not None:
                try:
                    log_file.close()
                except OSError:
                    pass

    if not args.read_from_file and len(paths) not in (1, 2):
        print("Error: provide 1 directory, 2 directories, or 2 image files (or use -r to read pairs from file).", file=sys.stderr)
        return 1

    # --- Two-image mode: 2 args, both files ---
    if len(paths) == 2 and paths[0].is_file() and paths[1].is_file():
        result = compare_two_images(paths[0], paths[1])
        if result is None:
            print("Error: could not load one or both images.", file=sys.stderr)
            return 1
        sim, hamming = result
        if args.json:
            payload = {
                "similarity": sim,
                "similarity_pct": round(sim * 100, 2),
                "hamming_distance": hamming,
            }
            print(json.dumps(payload, ensure_ascii=False))
            _log_result(json.dumps(payload, ensure_ascii=False))
        else:
            line1 = f"Similarity: {_format_similarity(sim)} (Hamming distance: {hamming})"
            line2 = f"  {paths[0]}"
            line3 = f"  {paths[1]}"
            print(line1)
            print(line2)
            print(line3)
            _log_result(line1, line2, line3)
        return 0

    # --- Two-directory mode: 2 args, both directories ---
    if len(paths) == 2 and paths[0].is_dir() and paths[1].is_dir():
        if args.dedupe:
            print("Error: --dedupe is only supported in single-directory mode.", file=sys.stderr)
            return 1
        pairs = compare_dirs(
            paths[0], paths[1],
            level=args.level,
            threshold=args.threshold,
            recursive=not args.no_recursive,
            progress_callback=progress_cb,
        )
        if getattr(args, "result_log", None) is not None:
            if not pairs:
                _log_result("No similar image pairs found between the two directories.")
            else:
                _log_result(
                    f"Level {args.level} (threshold={th}), {len(pairs)} similar pair(s) between dirs:",
                )
                for p1, p2, s in pairs:
                    _log_result(f"{_format_similarity(s)}\t{p1}\t{p2}")
        if args.copy_similar and pairs:
            unique = set()
            for p1, p2, _ in pairs:
                unique.add(p1.resolve())
                unique.add(p2.resolve())
            out_dir = _copy_similar_to_cwd(unique, Path.cwd())
            print(f"Copied {len(unique)} image(s) to {out_dir}")
        if args.json:
            out = [{"path_a": str(p1), "path_b": str(p2), "similarity": round(s, 4)} for p1, p2, s in pairs]
            payload = out
            print(json.dumps(payload, ensure_ascii=False, indent=2))
            if getattr(args, "result_log", None) is not None:
                _log_result(json.dumps(payload, ensure_ascii=False))
            return 0
        if not pairs:
            print("No similar image pairs found between the two directories.")
            return 0
        print(f"Level {args.level} (threshold={th}), {len(pairs)} similar pair(s) between dirs:\n")
        for p1, p2, s in pairs:
            print(f"  {_format_similarity(s)}  {p1}")
            print(f"           {p2}")
            print()
        return 0

    # --- Single-directory mode: 1 arg, directory (recursive by default) ---
    if len(paths) == 1 and paths[0].is_dir():
        root = paths[0]
        recursive = not args.no_recursive
        # When --fast without --time-window: default to 1 second (only compare files with filename timestamps within 1s)
        time_window = _parse_duration_seconds(args.time_window, "--time-window")
        if args.fast and recursive and time_window is None:
            time_window = 1
        effective_threads = args.threads if args.threads is not None else default_threads()
        if not args.json:
            mode = "recursively (fast: same folder only)" if (recursive and args.fast) else "recursively" if recursive else "(current dir only)"
            if time_window is not None:
                mode += f", time-window={time_window}s"
            if effective_threads > 1:
                mode += f", threads={effective_threads}"
            print(f"Scanning directory {mode}: {root}\n", flush=True)
        # In --fast mode with --exif-time-window, we pass it so similarity uses EXIF sort + sliding window (fewer comparisons).
        exif_window_cli = _parse_duration_seconds(args.exif_time_window, "--exif-time-window")
        exif_window_for_fast = exif_window_cli if (args.fast and recursive and exif_window_cli is not None) else None
        pairs = find_similar_pairs_with_scores(
            root,
            level=args.level,
            threshold=args.threshold,
            recursive=recursive,
            progress_callback=progress_cb,
            fast_same_folder_only=args.fast and recursive,
            time_window_seconds=time_window,
            exif_time_window_seconds=exif_window_for_fast,
            num_threads=args.threads,
        )
        # Optional EXIF-based time window (post-filter when not used in fast path): only keep pairs whose EXIF capture times
        # differ by at most args.exif_time_window seconds. Pairs without EXIF are ignored.
        if exif_window_cli is not None and pairs and exif_window_for_fast is None:
            exif_window = max(0, exif_window_cli)
            dt_cache: dict[Path, datetime | None] = {}

            def _get_dt(p: Path) -> datetime | None:
                if p not in dt_cache:
                    dt_cache[p] = get_exif_datetime(p)
                return dt_cache[p]

            filtered_pairs: list[tuple[Path, Path, float]] = []
            for p1, p2, s in pairs:
                dt1 = _get_dt(p1)
                dt2 = _get_dt(p2)
                if dt1 is None or dt2 is None:
                    continue
                if abs((dt1 - dt2).total_seconds()) <= exif_window:
                    filtered_pairs.append((p1, p2, s))
            pairs = filtered_pairs
        if args.copy_similar and pairs:
            unique = set()
            for p1, p2, _ in pairs:
                unique.add(p1.resolve())
                unique.add(p2.resolve())
            out_dir = _copy_similar_to_cwd(unique, Path.cwd())
            print(f"Copied {len(unique)} image(s) to {out_dir}")
        # 100% similar treated as duplicates (only these enter duplicate-delete flow)
        duplicate_pairs = [(p1, p2, s) for (p1, p2, s) in pairs if s == 1.0]
        dup_groups = pairs_to_groups(duplicate_pairs) if duplicate_pairs else []
        dup_to_delete = get_files_to_delete_from_groups(dup_groups, "older") if dup_groups else []

        if args.dedupe and pairs:
            groups = pairs_to_groups(pairs)
            to_delete = get_files_to_delete_from_groups(groups, args.dedupe.replace("keep-", ""))
            if not to_delete:
                pass
            else:
                if not args.yes:
                    print(f"About to delete {len(to_delete)} file(s) (keep {args.dedupe} by EXIF):")
                    for p in to_delete[:20]:
                        print(f"  {p}")
                    if len(to_delete) > 20:
                        print(f"  ... and {len(to_delete) - 20} more")
                    try:
                        r = input("Proceed? [y/N]: ").strip().lower()
                    except EOFError:
                        r = "n"
                    if r != "y" and r != "yes":
                        print("Aborted.")
                        return 0
                for p in to_delete:
                    try:
                        p.unlink(missing_ok=True)
                    except OSError as e:
                        print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                print(f"Deleted {len(to_delete)} file(s).")
        if args.json:
            out = [{"path_a": str(p1), "path_b": str(p2), "similarity": round(s, 4), "duplicate": s == 1.0} for p1, p2, s in pairs]
            summary = {
                "total_similar_pairs": len(pairs),
                "duplicate_pairs_100": len(duplicate_pairs),
                "duplicate_groups": len(dup_groups),
            }
            payload = {"results": out, "summary": summary}
            print(json.dumps(payload, ensure_ascii=False, indent=2))
            if getattr(args, "result_log", None) is not None:
                _log_result(json.dumps(payload, ensure_ascii=False))
            return 0
        if not pairs:
            print("No similar image pairs found in directory.")
            if getattr(args, "result_log", None) is not None:
                _log_result("No similar image pairs found in directory.")
            return 0
        header = f"Level {args.level} (threshold={th}), recursive={not args.no_recursive}, {len(pairs)} similar pair(s):"
        print(header + "\n")
        if getattr(args, "result_log", None) is not None:
            _log_result(header)
        for p1, p2, s in pairs:
            dup_mark = " [duplicate]" if s == 1.0 else ""
            line1 = f"  {_format_similarity(s)}{dup_mark}  {p1}"
            line2 = f"           {p2}"
            print(line1)
            print(line2)
            print()
            if getattr(args, "result_log", None) is not None:
                _log_result(line1, line2)
        summary_lines = [
            "--- Summary ---",
            f"Total similar pairs: {len(pairs)}",
            f"Duplicate (100% similar): {len(duplicate_pairs)} pair(s) in {len(dup_groups)} group(s)",
        ]
        for l in summary_lines:
            print(l)
        if getattr(args, "result_log", None) is not None:
            _log_result(*summary_lines)
        # Optional similarity-threshold deletion: delete images from EXIF-based groups
        # when similarity >= args.del_similar_over (percent), keeping oldest EXIF per group.
        if args.del_similar_over is not None and pairs:
            pct = max(0, min(100, int(args.del_similar_over)))
            cutoff = pct / 100.0
            high_pairs = [(p1, p2, s) for (p1, p2, s) in pairs if s >= cutoff]
            if high_pairs:
                high_groups = pairs_to_groups(high_pairs)
                to_delete_sim = get_files_to_delete_from_groups(high_groups, "older")
            else:
                to_delete_sim = []
            if to_delete_sim:
                print(
                    f"About to delete {len(to_delete_sim)} file(s) from groups with similarity >= {pct}% "
                    "(keeps oldest EXIF per group)."
                )
                if not args.force_del_dup and not args.yes:
                    for p in to_delete_sim[:20]:
                        print(f"  {p}")
                    if len(to_delete_sim) > 20:
                        print(f"  ... and {len(to_delete_sim) - 20} more")
                    try:
                        r = input("Proceed? [y/N]: ").strip().lower()
                    except EOFError:
                        r = "n"
                    if r != "y" and r != "yes":
                        print("Aborted similarity-threshold deletion.")
                        to_delete_sim = []
                if to_delete_sim:
                    for p in to_delete_sim:
                        try:
                            p.unlink(missing_ok=True)
                        except OSError as e:
                            print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                    print(f"Deleted {len(to_delete_sim)} file(s) with similarity >= {pct}%.")
        # If a similarity threshold was requested, we skip the separate 100% duplicate delete prompt
        # because those duplicates are already covered by the threshold.
        if dup_to_delete and args.del_similar_over is None:
            print(f"  -> {len(dup_to_delete)} file(s) can be removed (keeps oldest EXIF per group).")
            if not args.yes:
                try:
                    r = input("Delete duplicate images? [y/N]: ").strip().lower()
                except EOFError:
                    r = "n"
                if r == "y" or r == "yes":
                    for p in dup_to_delete:
                        try:
                            p.unlink(missing_ok=True)
                        except OSError as e:
                            print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                    print(f"Deleted {len(dup_to_delete)} duplicate file(s).")
                else:
                    print("Skipped deletion.")
            else:
                for p in dup_to_delete:
                    try:
                        p.unlink(missing_ok=True)
                    except OSError as e:
                        print(f"Warning: could not delete {p}: {e}", file=sys.stderr)
                print(f"Deleted {len(dup_to_delete)} duplicate file(s).")
        return 0

    # Invalid argument combination
    if len(paths) == 1:
        p = paths[0]
        if p.is_file():
            print("Error: single path must be a directory. For two images use: find-similar <img1> <img2>", file=sys.stderr)
        else:
            print(f"Error: not a directory (or path does not exist): {p}", file=sys.stderr)
            print("Tip: use one directory path for recursive similar-image search; add a space before options, e.g. \"C:\\Photos\\DCIM\" --verbose", file=sys.stderr)
        return 1
    if len(paths) == 2:
        print("Error: both paths must be two directories or two image files.", file=sys.stderr)
        return 1
    print("Error: provide 1 directory, or 2 directories, or 2 image files.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
