"""Compare public C++ enum values and error masks with a local Python SDK.

Usage: python -B tests/check_python_types.py E:/GHand-Python-SDK
Only the standalone enum modules are loaded; no device or transport is opened.
"""
from pathlib import Path
import ast
import re
import runpy
import sys

root = Path(__file__).resolve().parents[1]
python_types = Path(sys.argv[1]) / "src/ghand/types"
header = "\n".join(
    (root / "include/ghand/types" / name).read_text(encoding="utf-8")
    for name in ("enums.h", "errors.h", "data.h")
)
source = (root / "src/types.cc").read_text(encoding="utf-8")
baseline = runpy.run_path(str(python_types / "enums.py"))
baseline.update(runpy.run_path(str(python_types / "errors.py")))
checked = 0
for name in (
    "JointId", "RS485BaudRate", "CANFDBitTiming", "TactileSensorId",
    "HandRuntimeState", "HandControlMode", "HandControlState",
    "JointControlMode", "JointLoopType", "JointControlState", "JointState",
    "JointErrorCode",
):
    match = re.search(r"enum class " + name + r"\s*:[^{]+\{([^}]+)\}", header)
    assert match, name
    members = {}
    value = -1
    for member in re.sub(r"//[^\n]*", "", match[1]).split(","):
        if not member.strip():
            continue
        parts = member.strip().split("=")
        value = int(parts[1].strip(), 0) if len(parts) == 2 else value + 1
        members[parts[0].strip()] = value
    for symbol, member in baseline[name].__members__.items():
        cpp_symbol = symbol.replace("BAUD_", "RATE_") if name == "RS485BaudRate" else symbol
        assert members.get(cpp_symbol) == member.value, (name, symbol, members.get(cpp_symbol), member.value)
        checked += 1

for name in ("HandWarning", "HandFault", "HandCriticalError"):
    assert re.search(r"const " + name + " " + name + r"::NONE\{\}", source)
    for symbol, member in baseline[name].__members__.items():
        if member.value == 0:
            continue
        match = re.search(name + "::" + symbol + r" = MakeHandErrorFlag<" + name + r">\((\d+)\)", source)
        assert match and 1 << int(match[1]) == member.value, (name, symbol)
        checked += 1

fields_checked = 0
for cls in ast.parse((python_types / "data.py").read_text(encoding="utf-8")).body:
    if not isinstance(cls, ast.ClassDef):
        continue
    match = re.search(r"struct " + cls.name + r"\s*\{(.*?)\n\};", header, re.S)
    assert match, cls.name
    for field in cls.body:
        if isinstance(field, ast.AnnAssign):
            assert re.search(r"\b" + field.target.id + r"\s*(?:=|;|\{)", match[1]), (cls.name, field.target.id)
            fields_checked += 1

print(f"Python/C++ public types: {checked} enum values/error flags and {fields_checked} data members matched")
