import os
import sys
import subprocess
from pathlib import Path

in_path = Path("C:/Users/dimel/Downloads/dataset (1)/dataset")
exe = Path("./build/csv_labeler.exe")
out_path = Path("./results")

if not exe.exists():
    print("❌ Executable not found. Please build the project first.")
    sys.exit(1)

out_path.mkdir(parents=True, exist_ok=True)
csv_files = list(in_path.glob("*.csv"))

if not csv_files:
    print(f"⚠️ Keine CSV-Dateien gefunden in: {in_path}")
    sys.exit(0)

for csv_file in csv_files:
    out_file = out_path / csv_file.name
    print(f"\n📄 Processing: {csv_file.name}")
    print(f"   Output -> {out_file}")

    cmd = [str(exe), str(csv_file), str(out_file)]

    # Starte Prozess mit direkter Ausgabe
    process = subprocess.Popen(
        cmd,
        cwd=exe.parent,
        stdout=sys.stdout,
        stderr=sys.stderr,
        text=True,
        bufsize=1  # Zeilenweise Ausgabe
    )

    process.wait()
    if process.returncode == 0:
        print("✅ Done.")
    else:
        print(f"❌ Fehler bei {csv_file.name}")
