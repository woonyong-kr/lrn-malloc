import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
results = []
# A deterministic mixed allocation/reallocation stress trace, separate from the lab fixtures.
import random

rng = random.Random(731)
active = set()
ops = []
for i in range(3000):
    ident = rng.randrange(64)
    if ident not in active:
        active.add(ident)
        ops.append(f"a {ident} {rng.randint(1, 8192)}")
    elif rng.random() < 0.45:
        active.remove(ident)
        ops.append(f"f {ident}")
    else:
        ops.append(f"r {ident} {rng.randint(1, 16384)}")
for ident in sorted(active):
    ops.append(f"f {ident}")
stress = root / ".build/stress.rep"
stress.write_text(f"0\n64\n{len(ops)}\n1\n" + "\n".join(ops) + "\n")
for trace in [*sorted((root / "malloc-lab/traces").glob("*-bal.rep")), stress]:
    for engine in ("avl", "list"):
        result = subprocess.run(
            [str(root / ".build" / f"trace-{engine}"), str(trace.relative_to(root))],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        record = json.loads(result.stdout)
        record["engine"] = engine
        results.append(record)
        print(json.dumps(record), flush=True)
(root / ".build/results.json").write_text(json.dumps(results, indent=2) + "\n")
