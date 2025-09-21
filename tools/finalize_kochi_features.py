import argparse
import csv
import hashlib
import json
import pathlib


def sha256_bytes(b: bytes) -> str:
    return "sha256:" + hashlib.sha256(b).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True, help="Kochi features CSV (timestamp + feature columns)")
    ap.add_argument("--seq_len", type=int, default=64)
    ap.add_argument("--emit_from", type=int, default=64)
    ap.add_argument("--pad_value", type=float, default=0.0)
    ap.add_argument("--out", default="configs/features/kochi_v1_spec.json")
    args = ap.parse_args()

    p = pathlib.Path(args.csv)
    with open(p, newline="") as fh:
        reader = csv.reader(fh)
        header = next(reader)

    names = [c for c in header if c.lower() not in ("ts", "timestamp", "bar_index", "time")]

    spec = {
        "family": "KOCHI",
        "version": "v1",
        "input_dim": len(names),
        "seq_len": int(args.seq_len),
        "emit_from": int(args.emit_from),
        "pad_value": float(args.pad_value),
        "names": names,
        "provenance": {
            "source_csv": str(p),
            "header_hash": sha256_bytes(",".join(header).encode()),
        },
        "ops": {"note": "Kochi features supplied externally; no op list"},
    }

    outp = pathlib.Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text(json.dumps(spec, indent=2))
    print(f"✅ Wrote Kochi feature spec → {outp} (F={len(names)}, T={args.seq_len})")


if __name__ == "__main__":
    main()


