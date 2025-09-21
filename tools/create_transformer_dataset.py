#!/usr/bin/env python3
import argparse
import csv
import sys
from pathlib import Path
from datetime import datetime, timezone


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Create merged QQQ 1-minute training dataset "
            "(historical + future tracks) in Polygon schema: "
            "timestamp,symbol,open,high,low,close,volume"
        )
    )
    parser.add_argument(
        "--hist", type=Path, required=True,
        help=(
            "Path to historical equities CSV "
            "(e.g., data/equities/QQQ_RTH_NH.csv)"
        ),
    )
    parser.add_argument(
        "--future-dir", type=Path, required=True,
        help=(
            "Directory containing future_qqq_track_*.csv files"
        ),
    )
    parser.add_argument(
        "--output", type=Path, required=True,
        help=(
            "Output merged CSV path "
            "(e.g., data/equities/QQQ_1min_merged.csv)"
        ),
    )
    parser.add_argument(
        "--symbol", type=str, default="QQQ",
        help="Symbol to use for historical rows (default: QQQ)",
    )
    return parser.parse_args()


def normalize_ts_iso_to_utc(ts: str) -> str:
    """Normalize ISO-8601 timestamp with offset to UTC 'YYYY-MM-DD HH:MM:SS'.
    Falls back to raw string if parsing fails.
    """
    try:
        # Example: 2022-09-15T09:30:00-04:00
        dt = datetime.fromisoformat(ts)
        if dt.tzinfo is None:
            # Treat naive as UTC
            dt = dt.replace(tzinfo=timezone.utc)
        dt_utc = dt.astimezone(timezone.utc)
        return dt_utc.strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return ts


def read_historical(hist_path: Path, symbol: str):
    """Yield rows in polygon schema from historical equities file with header:
    ts_utc,ts_nyt_epoch,open,high,low,close,volume
    """
    with hist_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        required = {"ts_utc", "open", "high", "low", "close", "volume"}
        if not required.issubset(reader.fieldnames or {}):
            raise RuntimeError(
                "Historical CSV missing required columns: "
                f"{required}"
            )
        for row in reader:
            ts = normalize_ts_iso_to_utc(row["ts_utc"])  # normalize to UTC
            yield [
                ts,
                symbol,
                row["open"],
                row["high"],
                row["low"],
                row["close"],
                row["volume"],
            ]


def read_future_tracks(future_dir: Path):
    """Yield rows from future track CSVs in polygon schema:
    timestamp,symbol,open,high,low,close,volume
    """
    tracks = sorted(future_dir.glob("future_qqq_track_*.csv"))
    for path in tracks:
        with path.open("r", newline="") as f:
            reader = csv.reader(f)
            _ = next(reader, None)  # header
            # expect polygon schema
            for row in reader:
                if not row:
                    continue
                yield row[:7]


def write_merged(output_path: Path, rows):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "timestamp",
                "symbol",
                "open",
                "high",
                "low",
                "close",
                "volume",
            ]
        )
        for row in rows:
            writer.writerow(row)


def main() -> int:
    args = parse_args()

    # Collect rows keyed by timestamp to deduplicate
    merged = {}

    # 1) Historical first (will be kept on duplicates)
    hist_count = 0
    for row in read_historical(args.hist, args.symbol):
        ts = row[0]
        if ts not in merged:
            merged[ts] = row
            hist_count += 1

    # 2) Future tracks (only add if timestamp not already present)
    fut_count = 0
    for row in read_future_tracks(args.future_dir):
        ts = row[0]
        if ts not in merged:
            merged[ts] = row
            fut_count += 1

    # Sort by timestamp key lexicographically
    sorted_rows = [merged[k] for k in sorted(merged.keys())]

    write_merged(args.output, sorted_rows)

    print(f"Wrote merged dataset: {args.output}")
    print(f"  Historical rows used: {hist_count}")
    print(f"  Future track rows used: {fut_count}")
    print(
        f"  Total rows: {len(sorted_rows)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())


