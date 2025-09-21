#!/usr/bin/env python3
import argparse
import pathlib
import sys
from typing import Tuple


def read_bars(path: pathlib.Path):
    import pandas as pd
    # Try with header detection; polygon files often have no header
    try:
        df = pd.read_csv(path, header=None)
        # Heuristic: 7 columns: ts,symbol,open,high,low,close,volume
        if df.shape[1] < 7:
            # Retry with header row
            df = pd.read_csv(path)
    except Exception:
        df = pd.read_csv(path)

    if df.shape[1] >= 7:
        cols = ["ts", "symbol", "open", "high", "low", "close", "volume"] + [f"extra{i}" for i in range(df.shape[1]-7)]
        df.columns = cols[:df.shape[1]]
    elif df.shape[1] == 6:
        df.columns = ["ts", "open", "high", "low", "close", "volume"]
        df["symbol"] = path.stem.split("_")[0]
        df = df[["ts","symbol","open","high","low","close","volume"]]
    else:
        raise ValueError(f"Unexpected column count in {path}: {df.shape[1]}")

    # Normalize ts to string and index
    df["ts"] = df["ts"].astype(str)
    df = df.set_index("ts").sort_index()
    return df


def align_intersection(df1, df2, df3, df4=None):
    idx = df1.index.intersection(df2.index).intersection(df3.index)
    if df4 is not None:
        idx = idx.intersection(df4.index)
    idx = idx.sort_values()
    if df4 is not None:
        return df1.loc[idx], df2.loc[idx], df3.loc[idx], df4.loc[idx]
    else:
        return df1.loc[idx], df2.loc[idx], df3.loc[idx]


def write_bars(path: pathlib.Path, df) -> None:
    # Preserve original polygon-like format: ts,symbol,open,high,low,close,volume
    out = df.reset_index()[["ts","symbol","open","high","low","close","volume"]]
    out.to_csv(path, index=False)


def derive_out(path: pathlib.Path, suffix: str) -> pathlib.Path:
    stem = path.stem
    if stem.endswith(".csv"):
        stem = stem[:-4]
    return path.with_name(f"{stem}_{suffix}.csv")


def main():
    ap = argparse.ArgumentParser(description="Align QQQ/TQQQ/SQQQ minute bars by timestamp intersection.")
    ap.add_argument("--qqq", required=True)
    ap.add_argument("--tqqq", required=True)
    ap.add_argument("--sqqq", required=True)
    # PSQ removed from family - moderate sell signals now use SHORT QQQ
    ap.add_argument("--suffix", default="ALIGNED")
    args = ap.parse_args()

    qqq_p = pathlib.Path(args.qqq)
    tqqq_p = pathlib.Path(args.tqqq)
    sqqq_p = pathlib.Path(args.sqqq)

    import pandas as pd
    pd.options.mode.chained_assignment = None

    df_q = read_bars(qqq_p)
    df_t = read_bars(tqqq_p)
    df_s = read_bars(sqqq_p)

    a_q, a_t, a_s = align_intersection(df_q, df_t, df_s)
    assert list(a_q.index) == list(a_t.index) == list(a_s.index)

    out_q = derive_out(qqq_p, args.suffix)
    out_t = derive_out(tqqq_p, args.suffix)
    out_s = derive_out(sqqq_p, args.suffix)

    write_bars(out_q, a_q)
    write_bars(out_t, a_t)
    write_bars(out_s, a_s)

    print_files = [f"→ {out_q}", f"→ {out_t}", f"→ {out_s}"]

    n = len(a_q)
    print(f"Aligned bars: {n}")
    for file_path in print_files:
        print(file_path)


if __name__ == "__main__":
    main()


