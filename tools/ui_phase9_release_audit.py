#!/usr/bin/env python3
from __future__ import annotations

import argparse
import collections
import hashlib
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

QRC_LITERAL_RE = re.compile(r'''["']qrc:/([^"']+)["']''')
HARD_RUNTIME_RE = re.compile(
    r"\b(?:ReferenceError|TypeError|SyntaxError)\b|"
    r"is not a type|module .+ is not installed|"
    r"Cannot assign to non-existent property|Unable to assign|"
    r"Binding loop detected|QQmlApplicationEngine failed|"
    r"Cannot load component|No such file or directory",
    re.IGNORECASE,
)
LAYOUT_WARNING_RE = re.compile(
    r"anchors on an item that is managed by a layout|"
    r"Detected anchors on an item that is managed by a layout|"
    r"QML .*: Binding loop",
    re.IGNORECASE,
)

REQUIRED_HMI = {
    "ui/Theme.qml",
    "ui/HmiButton.qml",
    "ui/HmiControlTile.qml",
    "ui/HmiHorizontalScroll.qml",
    "ui/HmiMetricChip.qml",
    "ui/HmiNavButton.qml",
    "ui/HmiPanel.qml",
    "ui/HmiStatusPill.qml",
    "ui/HmiTextField.qml",
}

# Files that are allowed to differ from the authoritative 2026-09-22 baseline.
# This deliberately excludes C++/headers and backend protocol implementation.
ALLOWED_PRODUCT_CHANGES = {
    "DoaViewer/DoaControlPanel.qml",
    "DoaViewer/DoaTonePanel.qml",
    "DoaViewer/FftControlPanel.qml",
    "DoaViewer/TopBar.qml",
    "DoaViewer/ViewerPage.qml",
    "HomeDisplay.qml",
    "MainPage.qml",
    "RadioScanner.qml",
    "ServiceEndpointsPage.qml",
    "Setting.qml",
    "SpectrumGLPlot.qml",
    "VpnPage.qml",
    "Wifi5GSetting.qml",
    "Wifi5GView.qml",
    "iRecordManage/AddNewDevice.qml",
    "iRecordManage/CPUused.qml",
    "iRecordManage/CalendarPopup.qml",
    "iRecordManage/EditDeviceLists.qml",
    "iRecordManage/ExportFilesRecord.qml",
    "iRecordManage/LogDataFIles.qml",
    "iRecordManage/MonitorDisplay.qml",
    "iRecordManage/PlayerController.qml",
    "iRecordManage/PopUPDeletedFileWave.qml",
    "iRecordManage/RAMused.qml",
    "iRecordManage/RecordFiles.qml",
    "iRecordManage/RegisterDevice.qml",
    "iRecordManage/RegisterNewDevice.qml",
    "iRecordManage/StorageUsed.qml",
    "iRecordManage/TapBarRecordFiles.qml",
    "iRecordManage/TumblerDateTime.qml",
    "iRecordManage/UpTimeUsed.qml",
    "iRecordManage/WaveEditor.qml",
    "iScreenDFqml/pages/DoaHistoryViewer.qml",
    "iScreenDFqml/pages/QMLMap.qml",
    "iScreenDFqml/pages/SideSettingsDrawer.qml",
    "iScreenDFqml/popuppanels/CancelButtonPopupSettingDrawer.qml",
    "iScreenDFqml/sidepanels/GroupCard.qml",
    "iScreenDFqml/sidepanels/SideRemote.qml",
    "qml.qrc",
    "ui/Theme.qml",
}

ALLOWED_ADDED_PRODUCT_FILES = REQUIRED_HMI - {"ui/Theme.qml"}

BASELINE_KNOWN_QRC_REFS = {
    ("iScreenDFqml/pages/DaqStatusBox.qml", "/images/lock.png"),
    ("iScreenDFqml/pages/DaqStatusBox.qml", "/images/unlock.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/marker.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/cardinal-point.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/gps.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/disable.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/pin-map.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/pin_disable.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/moon-and-stars.png"),
    ("iScreenDFqml/pages/Maptest.qml", "/images/sun_theme.png"),
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def qrc_inventory(root: Path):
    tree = ET.parse(root / "qml.qrc")
    actual: list[str] = []
    logical: set[str] = set()
    for elem in tree.iter("file"):
        rel = (elem.text or "").strip()
        if not rel:
            continue
        alias = elem.get("alias") or rel
        actual.append(rel)
        logical.add("/" + alias.lstrip("/"))
    return actual, logical


def structural_balance(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    stack = []
    line = 1
    i = 0
    quote = None
    block = False
    line_comment = False
    regex = False
    regex_class = False
    prev_sig = ""
    pairs = {"{": "}", "[": "]", "(": ")"}
    closings = set(pairs.values())
    regex_prefix = set("=([{,:;!&|?+-*%^~<>")

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if ch == "\n":
            line += 1
            line_comment = False
            if regex:
                return False, f"unterminated regex literal at line {line - 1}"
            i += 1
            continue
        if line_comment:
            i += 1
            continue
        if block:
            if ch == "*" and nxt == "/":
                block = False
                i += 2
            else:
                i += 1
            continue
        if quote:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
            i += 1
            continue
        if regex:
            if ch == "\\":
                i += 2
                continue
            if ch == "[":
                regex_class = True
                i += 1
                continue
            if ch == "]" and regex_class:
                regex_class = False
                i += 1
                continue
            if ch == "/" and not regex_class:
                regex = False
                i += 1
                while i < len(text) and text[i].isalpha():
                    i += 1
                prev_sig = "/"
                continue
            i += 1
            continue
        if ch == "/" and nxt == "/":
            line_comment = True
            i += 2
            continue
        if ch == "/" and nxt == "*":
            block = True
            i += 2
            continue
        if ch == "/" and (prev_sig in regex_prefix or prev_sig == ""):
            regex = True
            regex_class = False
            i += 1
            continue
        if ch in ('"', "'"):
            quote = ch
            i += 1
            continue
        if ch in pairs:
            stack.append((ch, line))
        elif ch in closings:
            if not stack or pairs[stack[-1][0]] != ch:
                return False, f"unexpected {ch!r} at line {line}"
            stack.pop()
        if not ch.isspace():
            prev_sig = ch
        i += 1

    if quote:
        return False, "unterminated string"
    if regex:
        return False, "unterminated regex literal"
    if block:
        return False, "unterminated block comment"
    if stack:
        return False, f"unclosed {stack[-1][0]!r} from line {stack[-1][1]}"
    return True, "ok"


def compare_baseline(root: Path, baseline: Path):
    changed, added, removed = [], [], []
    root_files = {
        p.relative_to(root).as_posix(): p
        for p in root.rglob("*")
        if p.is_file()
        and ".phase9-build" not in p.parts
        and not p.relative_to(root).as_posix().startswith("tools/")
        and not p.relative_to(root).as_posix().startswith("docs/")
        and p.name not in {"UI_RELEASE_MANIFEST.sha256", "UI_RELEASE_METADATA.txt"}
    }
    base_files = {
        p.relative_to(baseline).as_posix(): p
        for p in baseline.rglob("*")
        if p.is_file()
    }
    for rel in sorted(root_files.keys() & base_files.keys()):
        if sha256(root_files[rel]) != sha256(base_files[rel]):
            changed.append(rel)
    added = sorted(root_files.keys() - base_files.keys())
    removed = sorted(base_files.keys() - root_files.keys())
    return changed, added, removed


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".")
    ap.add_argument("--baseline", help="Extracted authoritative source baseline for freeze comparison")
    ap.add_argument("--runtime-log", help="Qt/QML runtime log captured on target")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    errors: list[str] = []
    warnings: list[str] = []

    try:
        actual, logical = qrc_inventory(root)
    except Exception as exc:
        print(f"[PHASE9][FAIL] cannot parse qml.qrc: {exc}")
        return 2

    missing = [rel for rel in actual if not (root / rel).is_file()]
    duplicates = [k for k, v in collections.Counter(actual).items() if v > 1]
    errors += [f"qml.qrc missing file: {rel}" for rel in missing]
    errors += [f"qml.qrc duplicate file: {rel}" for rel in duplicates]

    qml_resources = [rel for rel in actual if rel.endswith(".qml")]
    for rel in qml_resources:
        p = root / rel
        if not p.is_file():
            continue
        ok, why = structural_balance(p)
        if not ok:
            errors.append(f"QML structure: {rel}: {why}")
        text = p.read_text(encoding="utf-8", errors="replace")
        for m in QRC_LITERAL_RE.finditer(text):
            ref = "/" + m.group(1)
            if ref.endswith("/"):
                continue
            if ref not in logical:
                line = text.count("\n", 0, m.start()) + 1
                if (rel, ref) in BASELINE_KNOWN_QRC_REFS:
                    warnings.append(f"baseline-known unresolved qrc URL: {rel}:{line}: qrc:{ref}")
                else:
                    errors.append(f"unresolved qrc URL: {rel}:{line}: qrc:{ref}")

    actual_set = set(actual)
    for rel in sorted(REQUIRED_HMI):
        if rel not in actual_set:
            errors.append(f"required HMI resource not bundled: {rel}")

    # Modal layering contract introduced by the new shell.
    main_page = (root / "MainPage.qml").read_text(errors="replace")
    drawer = re.search(r"SideSettingsDrawer\s*\{.*?\n\s*z:\s*(\d+)", main_page, re.S)
    popup = re.search(r"PopupSettingDrawer\s*\{.*?\n\s*z:\s*(\d+)", main_page, re.S)
    if not drawer or not popup:
        errors.append("could not verify MainPage drawer/modal z-order")
    else:
        dz, pz = int(drawer.group(1)), int(popup.group(1))
        if not (dz > 3 and dz < pz):
            errors.append(f"invalid MainPage drawer z-order: drawer={dz}, modal={pz}")

    # Recorder runtime bugs fixed in Phase 9.
    delete_popup = (root / "iRecordManage/PopUPDeletedFileWave.qml").read_text(errors="replace")
    if "deviceCombo.rebuild()" in delete_popup:
        errors.append("Delete Files popup still calls removed deviceCombo.rebuild()")
    if "deviceNumBox.rebuild()" not in delete_popup:
        errors.append("Delete Files popup does not rebuild active deviceNumBox")
    if "tumblerDateTime.today()" in delete_popup:
        errors.append("Delete Files popup still calls nonexistent tumblerDateTime.today()")
    if "tumblerDateTime.setTodayAll()" not in delete_popup:
        errors.append("Delete Files popup does not reset custom range with setTodayAll()")

    # Baseline comparison: UI-only freeze must not mutate production backend.
    changed = added = removed = []
    if args.baseline:
        baseline = Path(args.baseline).resolve()
        if not baseline.is_dir():
            errors.append(f"baseline directory not found: {baseline}")
        else:
            changed, added, removed = compare_baseline(root, baseline)
            unexpected_changed = sorted(set(changed) - ALLOWED_PRODUCT_CHANGES)
            unexpected_added = sorted(set(added) - ALLOWED_ADDED_PRODUCT_FILES)
            if unexpected_changed:
                errors += [f"unexpected baseline modification: {x}" for x in unexpected_changed]
            if unexpected_added:
                errors += [f"unexpected product file added: {x}" for x in unexpected_added]
            if removed:
                errors += [f"baseline file removed: {x}" for x in removed]

    if args.runtime_log:
        lp = Path(args.runtime_log)
        if not lp.is_file():
            errors.append(f"runtime log not found: {lp}")
        else:
            for n, line in enumerate(lp.read_text(errors="replace").splitlines(), 1):
                if HARD_RUNTIME_RE.search(line):
                    errors.append(f"runtime:{n}: {line.strip()}")
                elif LAYOUT_WARNING_RE.search(line):
                    warnings.append(f"runtime:{n}: {line.strip()}")

    print(f"[PHASE9] root={root}")
    print(f"[PHASE9] qrc resources={len(actual)} qml={len(qml_resources)} missing={len(missing)} duplicates={len(duplicates)}")
    if args.baseline:
        print(f"[PHASE9] baseline changed={len(changed)} added={len(added)} removed={len(removed)}")
    if warnings:
        for w in warnings:
            print(f"[WARN] {w}")
    if errors:
        for e in errors:
            print(f"[FAIL] {e}")
        print(f"[PHASE9] FAIL ({len(errors)} issue(s), {len(warnings)} warning(s))")
        return 1

    print(f"[PHASE9] PASS ({len(warnings)} warning(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
