#!/usr/bin/env python3
import argparse
import json
import pathlib
import numpy as np
import pandas as pd

import sentio_features as sf


def load_bars_csv(csv_path: str):
    df = pd.read_csv(csv_path, low_memory=False)
    if "ts" in df.columns:
        ts = pd.to_datetime(df["ts"], utc=True, errors="coerce").astype("int64") // 10**9
    elif "ts_nyt_epoch" in df.columns:
        ts = df["ts_nyt_epoch"].astype("int64")
    elif "ts_utc_epoch" in df.columns:
        ts = df["ts_utc_epoch"].astype("int64")
    else:
        raise ValueError("No timestamp column found in bars CSV")
    # Drop any bad rows
    mask = ts.notna()
    ts = ts[mask].astype(np.int64)
    df = df.loc[mask]
    o = df["open"].astype(float).to_numpy()
    h = df["high"].astype(float).to_numpy()
    l = df["low"].astype(float).to_numpy()
    c = df["close"].astype(float).to_numpy()
    v = df["volume"].astype(float).to_numpy()
    return ts.to_numpy(np.int64), o, h, l, c, v


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--symbol", required=True)
    ap.add_argument("--bars", required=True)
    ap.add_argument("--outdir", default="data")
    args = ap.parse_args()

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    ts, o, h, l, c, v = load_bars_csv(args.bars)
    cols = sf.kochi_feature_names()
    X = sf.build_kochi_features(ts, o, h, l, c, v)

    # Save CSV: bar_index,timestamp,<features>
    csv_path = outdir / f"{args.symbol}_KOCHI_features.csv"
    header = "bar_index,timestamp," + ",".join(cols)
    N, F = int(X.shape[0]), int(X.shape[1])
    M = np.empty((N, F + 2), dtype=np.float64)
    M[:, 0] = np.arange(N, dtype=np.float64)
    M[:, 1] = ts.astype(np.float64)
    M[:, 2:] = X
    np.savetxt(csv_path, M, delimiter=",", header=header, comments="",
               fmt="%.10g")

    # Save NPY (feature matrix only)
    npy_path = outdir / f"{args.symbol}_KOCHI_features.npy"
    np.save(npy_path, X.astype(np.float32), allow_pickle=False)

    # Save META
    meta = {
        "schema_version": "1.0",
        "symbol": args.symbol,
        "rows": int(N),
        "cols": int(F),
        "feature_names": cols,
        "emit_from": 0,
        "kind": "kochi_features",
    }
    meta_path = outdir / f"{args.symbol}_KOCHI_features.meta.json"
    json.dump(meta, open(meta_path, "w"), indent=2)
    print(f"✅ Wrote: {csv_path}\n✅ Wrote: {npy_path}\n✅ Wrote: {meta_path}")


if __name__ == "__main__":
    main()


