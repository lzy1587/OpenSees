"""Fail-closed smoke test for an isolated, newly built OpenSeesPy module."""

import importlib.util
import csv
import ctypes
import os
from pathlib import Path
import sys


root = Path(__file__).resolve().parents[1]
pyd = Path(os.environ.get("VSFSIB_CUSTOM_PYD", str(root / "Win64" / "bin" / "opensees.pyd")))
if not pyd.is_file():
    raise SystemExit(f"NOT BUILT: {pyd}")
if str(pyd.resolve()).lower().startswith(str(Path(sys.prefix).resolve()).lower()):
    raise SystemExit("Refusing to test a module inside the Python installation")
python_dlls = Path(sys.base_prefix) / "DLLs"
_dll_directories = []
if python_dlls.is_dir():
    _dll_directories.append(os.add_dll_directory(str(python_dlls)))
    tcl = python_dlls / "tcl86t.dll"
    if tcl.is_file():
        ctypes.WinDLL(str(tcl))
_dll_directories.append(os.add_dll_directory(str(pyd.parent.resolve())))

spec = importlib.util.spec_from_file_location("opensees", str(pyd))
if spec is None or spec.loader is None:
    raise SystemExit("Cannot create a loader for the isolated opensees.pyd")
ops = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ops)
print("Loaded module path:", ops.__file__)
print("ops.version():", ops.version())
ops.wipe()
ops.model("basic", "-ndm", 1, "-ndf", 1)
ops.uniaxialMaterial("VSFSIBBreakaway", 1, 4296.2, 102.679180, 77.009385)
print("Material creation command: PASS")
ops.testUniaxialMaterial(1)
rows = []
for displacement_mm in (0.0, 10.0, 20.0, 23.9, 24.0, 60.0):
    ops.setStrain(displacement_mm / 1000.0)
    rows.append((displacement_mm, float(ops.getStress()), float(ops.getTangent())))
output = Path(os.environ.get("VSFSIB_SMOKE_CSV", str(root / "CI_MATERIAL_SMOKE_TEST.csv")))
output.parent.mkdir(parents=True, exist_ok=True)
with output.open("w", newline="", encoding="utf-8") as stream:
    writer = csv.writer(stream)
    writer.writerow(("displacement_mm", "force_kN", "tangent_kN_per_m"))
    writer.writerows(rows)
assert abs(rows[1][1] / 0.01 - 4296.2) < 1e-5
assert abs(rows[3][1] - 102.679180) < 1e-4
assert abs(rows[4][1] - 77.009385) < 1e-4
assert abs(rows[5][1] - 77.009385) < 1e-4
print("Monotonic smoke test: PASS", output)
