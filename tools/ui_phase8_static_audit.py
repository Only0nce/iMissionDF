#!/usr/bin/env python3
from __future__ import annotations
import argparse, collections, re, sys, xml.etree.ElementTree as ET
from pathlib import Path

MIGRATED_QML = [
    "MainPage.qml", "HomeDisplay.qml", "RadioScanner.qml", "SpectrumGLPlot.qml",
    "Setting.qml", "ServiceEndpointsPage.qml", "VpnPage.qml", "Wifi5GSetting.qml", "Wifi5GView.qml",
    "DoaViewer/ViewerPage.qml", "DoaViewer/TopBar.qml", "DoaViewer/DoaControlPanel.qml",
    "DoaViewer/FftControlPanel.qml", "DoaViewer/DoaTonePanel.qml",
    "iScreenDFqml/pages/QMLMap.qml", "iScreenDFqml/pages/DoaHistoryViewer.qml",
    "iScreenDFqml/pages/SideSettingsDrawer.qml",
    "iRecordManage/RecordFiles.qml", "iRecordManage/WaveEditor.qml", "iRecordManage/TapBarRecordFiles.qml",
    "iRecordManage/RegisterDevice.qml", "iRecordManage/RegisterNewDevice.qml", "iRecordManage/EditDeviceLists.qml",
    "iRecordManage/AddNewDevice.qml", "iRecordManage/LogDataFIles.qml", "iRecordManage/PlayerController.qml",
    "iRecordManage/ExportFilesRecord.qml", "iRecordManage/MonitorDisplay.qml", "iRecordManage/CPUused.qml",
    "iRecordManage/RAMused.qml", "iRecordManage/StorageUsed.qml", "iRecordManage/UpTimeUsed.qml",
    "ui/Theme.qml", "ui/HmiButton.qml", "ui/HmiPanel.qml", "ui/HmiStatusPill.qml", "ui/HmiNavButton.qml",
    "ui/HmiControlTile.qml", "ui/HmiMetricChip.qml", "ui/HmiTextField.qml", "ui/HmiHorizontalScroll.qml",
]
QRC_LITERAL_RE = re.compile(r'''["']qrc:/([^"']+)["']''')
HARD_ERROR_RE = re.compile(
    r"\b(?:ReferenceError|TypeError|SyntaxError)\b|is not a type|module .+ is not installed|"
    r"Cannot assign to non-existent property|Unable to assign|Binding loop detected|QQmlApplicationEngine failed",
    re.IGNORECASE,
)

def qrc_inventory(root: Path):
    tree = ET.parse(root / "qml.qrc")
    actual, logical = [], set()
    for elem in tree.iter("file"):
        rel = (elem.text or "").strip()
        if not rel: continue
        alias = elem.get("alias") or rel
        actual.append(rel); logical.add("/" + alias.lstrip("/"))
    return actual, logical

def structural_balance(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    stack=[]; line=1; i=0; quote=None; block=False; line_comment=False
    regex=False; regex_class=False; prev_sig=""
    pairs={"{":"}", "[":"]", "(":")"}; closings=set(pairs.values())
    regex_prefix=set("=([{,:;!&|?+-*%^~<>")
    while i < len(text):
        ch=text[i]; nxt=text[i+1] if i+1 < len(text) else ""
        if ch=="\n":
            line+=1; line_comment=False
            if regex: return False, f"unterminated regex literal at line {line-1}"
            i+=1; continue
        if line_comment: i+=1; continue
        if block:
            if ch=="*" and nxt=="/": block=False; i+=2
            else: i+=1
            continue
        if quote:
            if ch=="\\": i+=2; continue
            if ch==quote: quote=None
            i+=1; continue
        if regex:
            if ch=="\\": i+=2; continue
            if ch=="[": regex_class=True; i+=1; continue
            if ch=="]" and regex_class: regex_class=False; i+=1; continue
            if ch=="/" and not regex_class:
                regex=False; i+=1
                while i < len(text) and text[i].isalpha(): i+=1
                prev_sig="/"
                continue
            i+=1; continue
        if ch=="/" and nxt=="/": line_comment=True; i+=2; continue
        if ch=="/" and nxt=="*": block=True; i+=2; continue
        if ch=="/" and (prev_sig in regex_prefix or prev_sig==""):
            regex=True; regex_class=False; i+=1; continue
        if ch in ('"', "'"): quote=ch; i+=1; continue
        if ch in pairs: stack.append((ch,line))
        elif ch in closings:
            if not stack or pairs[stack[-1][0]] != ch: return False, f"unexpected {ch!r} at line {line}"
            stack.pop()
        if not ch.isspace(): prev_sig=ch
        i+=1
    if quote: return False, "unterminated string"
    if regex: return False, "unterminated regex literal"
    if block: return False, "unterminated block comment"
    if stack: return False, f"unclosed {stack[-1][0]!r} from line {stack[-1][1]}"
    return True, "ok"

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("root", nargs="?", default="."); ap.add_argument("--runtime-log")
    a=ap.parse_args(); root=Path(a.root).resolve(); errors=[]
    actual, logical=qrc_inventory(root)
    missing=[r for r in actual if not (root/r).is_file()]
    dupes=[k for k,v in collections.Counter(actual).items() if v>1]
    errors += [f"qml.qrc missing file: {r}" for r in missing]
    errors += [f"qml.qrc duplicate file: {r}" for r in dupes]
    for rel in MIGRATED_QML:
        p=root/rel
        if not p.is_file(): errors.append(f"migrated QML missing: {rel}"); continue
        ok,why=structural_balance(p)
        if not ok: errors.append(f"structure: {rel}: {why}")
        text=p.read_text(encoding="utf-8", errors="replace")
        for m in QRC_LITERAL_RE.finditer(text):
            ref="/"+m.group(1)
            if ref.endswith("/"): continue
            if ref not in logical:
                line=text.count("\n",0,m.start())+1
                errors.append(f"unresolved qrc URL: {rel}:{line}: qrc:{ref}")
    main=(root/"MainPage.qml").read_text(errors="replace")
    drawer=re.search(r"SideSettingsDrawer\s*\{.*?\n\s*z:\s*(\d+)", main, re.S)
    popup=re.search(r"PopupSettingDrawer\s*\{.*?\n\s*z:\s*(\d+)", main, re.S)
    if not drawer or not popup: errors.append("could not verify MainPage drawer/modal z-order")
    else:
        dz,pz=int(drawer.group(1)),int(popup.group(1))
        if not (dz>3 and dz<pz): errors.append(f"invalid drawer z-order: drawer={dz}, modal={pz}")
    if a.runtime_log:
        lp=Path(a.runtime_log)
        if not lp.is_file(): errors.append(f"runtime log not found: {lp}")
        else:
            for n,line in enumerate(lp.read_text(errors="replace").splitlines(),1):
                if HARD_ERROR_RE.search(line): errors.append(f"runtime:{n}: {line.strip()}")
    print(f"[PHASE8] root={root}")
    print(f"[PHASE8] qrc resources={len(actual)} missing={len(missing)} duplicates={len(dupes)}")
    print(f"[PHASE8] migrated QML checked={len(MIGRATED_QML)}")
    if errors:
        for e in errors: print(f"[FAIL] {e}")
        print(f"[PHASE8] FAIL ({len(errors)} issue(s))"); return 1
    print("[PHASE8] PASS"); return 0
if __name__=="__main__": sys.exit(main())
