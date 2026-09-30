# iScanMR10 Light Mode / Remote UI Fix V2

ฐานที่ใช้แก้: `src.tar(4).xz`

## เป้าหมาย

แก้ปัญหา Light Mode ที่ปุ่ม/ไอคอน/label จางจนมองไม่เห็น และป้องกันหน้า Remote Groups / Remote SDRs ล้างข้อมูลจาก payload ที่ไม่ใช่ข้อมูลจริง
โดยไม่แก้ backend, protocol, database หรือ RF/SDR control logic

## ไฟล์ที่แก้

- `ui/Theme.qml`
  - เพิ่ม token สำหรับ navigation / drawer / remote row ให้ Light Mode มี contrast จริง
- `ui/HmiNavButton.qml`
  - ปุ่ม navigation ใช้พื้น/เส้น/ตัวอักษรชัดเจนทั้ง Light/Dark
- `iScreenDFqml/pages/SideSettingsDrawer.qml`
  - ปุ่ม LOCAL/REMOTES, MAP ONLINE/OFFLINE, Select Mode ชัดขึ้นใน Light Mode
- `iScreenDFqml/pages/PillSegmentBar.qml`
  - segmented control ใต้ Select Mode อ่านง่ายขึ้นใน Light Mode
- `iScreenDFqml/sidepanels/SideGroup.qml`
  - validate payload ก่อน clear model เพื่อไม่ให้ข้อมูลหายจาก payload ผิดชนิด/partial broadcast
  - เพิ่ม empty-state ที่อ่านได้
- `iScreenDFqml/sidepanels/SideRemote.qml`
  - ส่ง `darkMode` เข้า `RemoteSdrItem`
  - แก้ panel/ปุ่มให้ใช้ theme token
  - เพิ่ม empty-state ที่อ่านได้
- `iScreenDFqml/sidepanels/GroupCard.qml`
  - เอา hard-coded dark background ออกจาก device delegate
  - ส่ง `darkMode` เข้า nested `RemoteSdrItem`
- `iScreenDFqml/sidepanels/RemoteSdrItem.qml`
  - row background/text/border ใช้ theme token
  - เพิ่ม status dot และแก้ `root.deviceName` ที่ผิด scope
- `iScreenDFqml/sidepanels/RemoteGroupsListItem.qml`
  - เพิ่ม Theme binding ให้ component นี้ไม่อ้างตัวแปรสีที่ไม่มีนิยาม

## Validation ที่รันแล้ว

```bash
python3 tools/ui_r14_theme_audit.py .
python3 tools/ui_phase8_static_audit.py .
```

ผลลัพธ์: PASS ทั้งคู่

## วิธีทดสอบบนเครื่องจริง

1. Build ตาม flow เดิมของโปรเจกต์
2. เปิด Dark Mode ตรวจว่า UI เดิมยังอ่านง่าย
3. สลับ Light Mode แล้วตรวจ:
   - LOCAL/REMOTES เห็นชัด
   - MAP ONLINE/OFFLINE เห็นชัด
   - RADIO / MAP / DOA / NETWORK / RECORDER / DIAGNOSTIC เห็นชัด
   - segmented bar ใต้ Select Mode เห็นชัด
   - Remote Groups / Remote SDRs ยังมีข้อมูล ไม่หาย
4. สลับ Light/Dark 10 รอบ แล้วตรวจว่า model ไม่กลายเป็นว่าง

## หมายเหตุ

รอบนี้เป็น UI-safe fix: ไม่แตะ backend, protocol, database, websocket command, RF tuning, spectrum/waterfall, หรือ recorder logic
