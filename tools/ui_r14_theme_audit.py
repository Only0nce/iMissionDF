#!/usr/bin/env python3
from pathlib import Path
import re, sys, xml.etree.ElementTree as ET

root = Path(sys.argv[1] if len(sys.argv) > 1 else '.').resolve()
qrc = root / 'qml.qrc'
errors = []

if not qrc.exists():
    print('[R1.4] FAIL: qml.qrc missing')
    sys.exit(2)

tree = ET.parse(qrc)
resources = [(n.text or '').strip() for n in tree.findall('.//file')]
missing = [p for p in resources if p and not (root / p).exists()]
dupes = sorted({p for p in resources if p and resources.count(p) > 1})
qmls = [p for p in resources if p.endswith('.qml') and (root / p).exists()]

if missing: errors.append('missing qrc resources: ' + ', '.join(missing[:12]))
if dupes: errors.append('duplicate qrc resources: ' + ', '.join(dupes[:12]))
if 'ui/HmiComboBox.qml' not in resources:
    errors.append('ui/HmiComboBox.qml is not registered in qml.qrc')

# All application dropdowns should use the controlled HMI implementation.
raw_combo = []
for rel in qmls:
    if rel == 'ui/HmiComboBox.qml':
        continue
    text = (root / rel).read_text(errors='ignore')
    if re.search(r'(^|[^A-Za-z0-9_])ComboBox\s*\{', text):
        raw_combo.append(rel)
if raw_combo:
    errors.append('raw ComboBox remains in qrc QML: ' + ', '.join(raw_combo))

contracts = {
    'MainPage.qml': [
        'HomeDisplay {',
        'darkMode: mainPage.darkTheme',
    ],
    'HomeDisplay.qml': [
        'property bool darkMode: true',
        'Theme { id: hmiTheme; darkMode: homeDisplay.darkMode }',
        'darkMode: homeDisplay.darkMode',
    ],
    'RadioScanner.qml': [
        'property bool darkMode: true',
        'Theme { id: hmiTheme; darkMode: scanpage.darkMode }',
        'darkMode: hmiTheme.darkMode',
    ],
    'SpectrumGLPlot.qml': [
        'property bool darkMode: true',
        'Theme { id: theme; darkMode: root.darkMode }',
    ],
    'main.qml': [
        'Material.theme: mainPage.darkTheme ? Material.Dark : Material.Light',
        'Material.foreground: windowTheme.text',
    ],
    'ui/HmiComboBox.qml': [
        'color: root.enabled ? theme.text : theme.muted',
        'color: theme.panel',
        'Material.foreground: theme.text',
    ],
}
for rel, needles in contracts.items():
    p = root / rel
    if not p.exists():
        errors.append(f'{rel} missing')
        continue
    text = p.read_text(errors='ignore')
    for needle in needles:
        if needle not in text:
            errors.append(f'{rel}: missing contract: {needle}')


# Runtime-hardening contracts from the target log supplied during R1.4.
main_qml = (root / 'main.qml').read_text(errors='ignore')
if re.search(r'(^|\n)\s*TapBarRecordFiles\s*\{', main_qml):
    errors.append('main.qml must not keep a hidden permanent TapBarRecordFiles instance')

record_files = (root / 'iRecordManage/RecordFiles.qml').read_text(errors='ignore')
if re.search(r'\bText\s*\{[^{}]*\bfontPixelSize\s*:', record_files, re.S):
    errors.append('RecordFiles.qml has fontPixelSize on raw Text; use font.pixelSize')

waterfall = (root / 'DoaViewer/WaterfallCanvas.qml').read_text(errors='ignore')
if 'id: nativeSubmitTimer' not in waterfall or 'nativeSubmitTimer.restart()' not in waterfall:
    errors.append('WaterfallCanvas lifecycle-safe native submit Timer contract missing')

map_qml = (root / 'iScreenDFqml/pages/QMLMap.qml').read_text(errors='ignore')
if 'id: doaDeferredRefreshTimer' not in map_qml or 'doaDeferredRefreshTimer.restart()' not in map_qml:
    errors.append('QMLMap lifecycle-safe deferred DoA refresh Timer contract missing')

log_files = (root / 'iRecordManage/LogDataFIles.qml').read_text(errors='ignore')
if 'model: logdataFileRecorder.visible ? listFileRecord : null' not in log_files:
    errors.append('LogDataFIles must detach listFileRecord while hidden')

log_devices = (root / 'iRecordManage/LogRegisterDevice.qml').read_text(errors='ignore')
if 'model: logregisterDevice.visible ? listoFDevice : null' not in log_devices:
    errors.append('LogRegisterDevice must detach listoFDevice while hidden')

print(f'[R1.4] qrc resources={len(resources)} qml={len(qmls)} missing={len(missing)} duplicates={len(dupes)} rawComboBox={len(raw_combo)}')
if errors:
    for e in errors:
        print('[FAIL]', e)
    sys.exit(1)
print('[R1.4] PASS')
