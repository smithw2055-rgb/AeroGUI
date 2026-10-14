# Run every sample once. --verify exits after the first frame.
# HelloWpf and EngineOverlay have no verify mode: stay up, then we close them.
import subprocess
import time
from pathlib import Path

ROOT = Path(r"C:\Projects\AeroGUI-R")
BUILD = ROOT / "build" / "samples"

VERIFY = [
    "HelloWorld/AeroHelloWorld",
    "Localization/AeroLocalization",
    "Menu3D/AeroMenu3D",
    "Scoreboard/AeroScoreboard",
    "ControlGallery/AeroControlGallery",
    "ApplicationTutorial/AeroApplicationTutorial",
    "BackgroundBlur/AeroBackgroundBlur",
    "BlendTutorial/AeroBlendTutorial",
    "BrushShaders/AeroBrushShaders",
    "Buttons/AeroButtons",
    "Commands/AeroCommands",
    "CustomAnimation/AeroCustomAnimation",
    "CustomControl/AeroCustomControl",
    "CustomRender/AeroCustomRender",
    "DataBinding/AeroDataBinding",
    "Integration/AeroIntegration",
    "Inventory/AeroInventory",
    "Login/AeroLogin",
    "QuestLog/AeroQuestLog",
    "TicTacToe/AeroTicTacToe",
    "UserControl/AeroUserControl",
    "VideoEffect/AeroVideoEffect",
]
LAUNCH = [
    "HelloWpf/AeroHelloWpf",
    "EngineOverlay/AeroEngineOverlay",
]

def exe(rel):
    sample, name = rel.split("/")
    return BUILD / sample / "RelWithDebInfo" / (name + ".exe")

results = []

def run_verify(rel):
    path = exe(rel)
    if not path.exists():
        results.append((rel, "MISSING"))
        return
    proc = subprocess.run(
        [str(path), "--verify"],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=45,
        encoding="utf-8",
        errors="replace",
    )
    err = (proc.stderr or "").strip().splitlines()
    tail = err[-2:] if err else []
    results.append((rel, f"exit {proc.returncode}", " | ".join(tail)))

def run_launch(rel):
    path = exe(rel)
    if not path.exists():
        results.append((rel, "MISSING"))
        return
    proc = subprocess.Popen(
        [str(path)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(4)
    code = proc.poll()
    if code is None:
        proc.kill()
        proc.wait(timeout=10)
        results.append((rel, "running"))
    else:
        err = proc.stderr.read().decode("utf-8", "replace").strip().splitlines()
        results.append((rel, f"exit {code}", " | ".join(err[-2:])))

for rel in VERIFY:
    try:
        run_verify(rel)
    except subprocess.TimeoutExpired:
        results.append((rel, "TIMEOUT"))
    print(results[-1][0], results[-1][1], flush=True)

for rel in LAUNCH:
    try:
        run_launch(rel)
    except Exception as exc:
        results.append((rel, f"error {exc}"))
    print(results[-1][0], results[-1][1], flush=True)

out = ROOT / "out" / "build" / "sample-run.txt"
lines = []
for item in results:
    lines.append(" ".join(item))
out.write_text("\n".join(lines), encoding="utf-8")
print("---")
print("\n".join(lines))
