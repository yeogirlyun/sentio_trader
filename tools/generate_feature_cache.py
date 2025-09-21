#!/usr/bin/env python3
import argparse, json, hashlib, pathlib, numpy as np
import pandas as pd
import sentio_features as sf

def spec_with_hash(p):
    raw = pathlib.Path(p).read_bytes()
    spec = json.loads(raw)
    spec["content_hash"] = "sha256:" + hashlib.sha256(json.dumps(spec, sort_keys=True).encode()).hexdigest()
    return spec

def load_bars(csv_path):
    df = pd.read_csv(csv_path, low_memory=False)
    if "ts" in df.columns:
        ts = pd.to_datetime(df["ts"], utc=True, errors="coerce").astype("int64") // 10**9
    elif "ts_nyt_epoch" in df.columns:
        ts = df["ts_nyt_epoch"].astype("int64")
    elif "ts_utc_epoch" in df.columns:
        ts = df["ts_utc_epoch"].astype("int64")
    else:
        raise ValueError("No timestamp column found. Available columns: %s" % list(df.columns))
    mask = ts.notna()
    ts = ts[mask].astype(np.int64)
    df = df.loc[mask]
    return (
        ts.to_numpy(np.int64),
        df["open"].astype(float).to_numpy(),
        df["high"].astype(float).to_numpy(),
        df["low"].astype(float).to_numpy(),
        df["close"].astype(float).to_numpy(),
        df["volume"].astype(float).to_numpy(),
    )

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--symbol", required=True, help="Base ticker, e.g. QQQ")
    ap.add_argument("--bars", required=True, help="CSV with columns: ts,open,high,low,close,volume")
    ap.add_argument("--spec", required=True, help="feature_spec_55.json")
    ap.add_argument("--outdir", default="data", help="output dir for features files")
    args = ap.parse_args()

    outdir = pathlib.Path(args.outdir); outdir.mkdir(parents=True, exist_ok=True)
    spec = spec_with_hash(args.spec); spec_json = json.dumps(spec, sort_keys=True)
    ts, o, h, l, c, v = load_bars(args.bars)

    print(f"[FeatureCache] Building features for {args.symbol} with {len(ts)} bars...")
    X = sf.build_features_from_spec(args.symbol, ts, o, h, l, c, v, spec_json).astype(np.float32)
    N, F = X.shape
    names = [f.get("name", f'{f["op"]}_{f.get("source","")}_{f.get("window","")}_{f.get("k","")}') for f in spec["features"]]

    print(f"[FeatureCache] Generated features: {N} rows x {F} features")
    print(f"[FeatureCache] Feature stats: min={X.min():.6f}, max={X.max():.6f}, mean={X.mean():.6f}, std={X.std():.6f}")

    csv_path = outdir / f"{args.symbol}_RTH_features.csv"
    header = "bar_index,timestamp," + ",".join(names)
    M = np.empty((N, F+2), dtype=np.float32)
    M[:, 0] = np.arange(N).astype(np.float64)
    M[:, 1] = ts.astype(np.float64)
    M[:, 2:] = X
    np.savetxt(csv_path, M, delimiter=",", header=header, comments="", fmt="%.6f")
    print(f"✅ CSV saved: {csv_path}")

    npy_path = outdir / f"{args.symbol}_RTH_features.npy"
    np.save(npy_path, X, allow_pickle=False)
    print(f"✅ NPY saved: {npy_path}")

    meta = {
        "schema_version":"1.0",
        "symbol": args.symbol,
        "rows": int(N), "cols": int(F),
        "feature_names": names,
        "spec_hash": spec["content_hash"],
        "emit_from": int(spec["alignment_policy"]["emit_from_index"])
    }
    json.dump(meta, open(outdir / f"{args.symbol}_RTH_features.meta.json","w"), indent=2)
    print(f"✅ META saved: {outdir / (args.symbol + '_RTH_features.meta.json')}")

if __name__ == "__main__":
    main()
