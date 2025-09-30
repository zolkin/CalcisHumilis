# Generates .pio.h headers for all .pio files under ./src/hw/pio/
# Uses `pioasm -o c-sdk <in.pio> <out.pio.h>`
# You can override the pioasm binary via env var PIOASM=/full/path/to/pioasm

Import("env")
import os
import subprocess
from pathlib import Path
import shutil
import sys

PROJECT_DIR = Path(env["PROJECT_DIR"])
PIO_DIR     = PROJECT_DIR / "src" / "hw" / "pio"     # scan here (recursive)
OUTPUT_MODE = "c-sdk"                                # pioasm output mode
PIOASM_BIN  = os.environ.get("PIOASM") or shutil.which("pioasm")

def compile_pio(pio_file: Path, hdr_file: Path):
    hdr_file.parent.mkdir(parents=True, exist_ok=True)
    cmd = [PIOASM_BIN, "-o", OUTPUT_MODE, str(pio_file), str(hdr_file)]
    print(f"[pioasm] {pio_file.relative_to(PROJECT_DIR)} -> {hdr_file.relative_to(PROJECT_DIR)}")
    subprocess.check_call(cmd)

def needs_rebuild(pio_file: Path, hdr_file: Path) -> bool:
    if not hdr_file.exists():
        return True
    return pio_file.stat().st_mtime > hdr_file.stat().st_mtime

def ensure_all_headers(*args, **kwargs):
    if not PIOASM_BIN:
        raise RuntimeError(
            "pioasm not found. Install Pico SDK tools or set env var PIOASM to the full path."
        )

    if not PIO_DIR.exists():
        # Nothing to do—allow build to continue quietly.
        print(f"[pioasm] Directory not found: {PIO_DIR}. Skipping.")
        return

    pio_files = list(PIO_DIR.rglob("*.pio"))
    if not pio_files:
        print(f"[pioasm] No .pio files under {PIO_DIR}.")
        return

    for pio in pio_files:
        hdr = pio.with_suffix(pio.suffix + ".h")  # foo.pio -> foo.pio.h
        try:
            if needs_rebuild(pio, hdr):
                compile_pio(pio, hdr)
        except subprocess.CalledProcessError as e:
            # Surface a friendly error and stop the build
            raise RuntimeError(f"pioasm failed for {pio} (exit {e.returncode})") from e

# Run before building the firmware
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", ensure_all_headers)