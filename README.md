# iSense / iScanMR10

โปรเจกต์นี้เป็นแอป Qt/QML + C++ สำหรับระบบ receiver, recorder, map/DF, DoA viewer และ network configuration บน hardware ตระกูล Jetson/Orin.

ไฟล์โปรเจกต์หลักคือ `iScanMR10.pro` และ entry point คือ `main.cpp` ซึ่งโหลด QML จาก `qrc:/main.qml`.

## โหมดสำคัญของโปรเจกต์

โปรเจกต์นี้มีโหมดที่ต้องแยกกันชัดเจน 2 ชั้น:

1. **Hardware Version**
   - `HW_5G`
   - `HW_NONE_5G`

2. **Build Platform / Architecture**
   - `PLATFORM_JETSON`
   - `PLATFORM_X86`

สองส่วนนี้ไม่ใช่เรื่องเดียวกัน:

- `HW_5G / HW_NONE_5G` ใช้บอกว่า feature ฝั่ง hardware มี 5G modem หรือไม่
- `PLATFORM_JETSON / PLATFORM_X86` ใช้บอกว่า build ด้วย mkspec ชุดไหน และควรรวม source/lib ฝั่ง Jetson หรือ desktop

## Hardware Version

Hardware version ถูกกำหนดใน `iScanMR10.pro`

สถานะปัจจุบันของโปรเจกต์นี้ตั้งใจ hard-code เป็น 5G:

```qmake
CONFIG+=HW_5G
```

เมื่อใช้ `HW_5G` จะได้ define:

```cpp
HARDWARE_VERSION_5G
HARDWARE_HAS_5G=1
```

ผลกระทบหลัก:

- `main.cpp` expose ค่า `HardwareHas5G=true` และ `HardwareVersionName="5G"` ให้ QML
- `Wifi5GSetting.qml` แสดงและเปิดใช้งาน 5G/Cellular controls
- `NetworkController` เปิด logic ที่ใช้ `mmcli` / `nmcli` สำหรับ cellular
- `Wifi5GController` ส่งสถานะ hardware version กลับให้หน้า WiFi/5G

### เปลี่ยนเป็น HW_NONE_5G

ถ้าต้องการ build รุ่นไม่มี 5G ให้แก้ใน `iScanMR10.pro` จาก:

```qmake
CONFIG+=HW_5G
```

เป็น:

```qmake
CONFIG+=HW_NONE_5G
```

เมื่อใช้ `HW_NONE_5G` จะได้ define:

```cpp
HARDWARE_VERSION_NONE_5G
HARDWARE_HAS_5G=0
```

ผลกระทบหลัก:

- `HardwareHas5G=false`
- QML ยังแสดงหน้า WiFi ได้ แต่ section 5G/Cellular จะไม่ถูก render เพื่อให้ layout ปรับขนาดตาม hardware mode
- `NetworkController` จะไม่สั่งงาน cellular จริง และจะส่งข้อความว่า build นี้เป็น `HW_NONE_5G`

> หมายเหตุ: โปรเจกต์นี้ตั้งใจใช้ hard-code ใน `.pro` ดังนั้นไม่ควรใส่ `HW_5G` และ `HW_NONE_5G` พร้อมกัน

## Platform / Architecture

`PLATFORM_JETSON` และ `PLATFORM_X86` ถูกเลือกจาก qmake mkspec ใน `iScanMR10.pro`

```qmake
linux-jetson-orin-g++ {
    DEFINES += PLATFORM_JETSON
    SOURCES = $$JETSON_SOURCES
    HEADERS = $$JETSON_HEADERS
} else: linux-g++ {
    DEFINES += PLATFORM_X86
    SOURCES = $$X86_SOURCES
    HEADERS = $$X86_HEADERS
}
```

### PLATFORM_JETSON

ใช้เมื่อ build ด้วย mkspec:

```bash
qmake iScanMR10.pro -spec linux-jetson-orin-g++
make
```

หรือถ้า environment ของ Qt Creator / qmake ตั้ง mkspec เป็น Jetson อยู่แล้ว สามารถ build จาก IDE ได้โดยตรง

เมื่อเข้า `PLATFORM_JETSON`:

- เปิด source ฝั่ง `iRecordManage`
- เปิด GPIO / DSP / audio hardware control
- ใช้ library ฝั่ง Jetson เช่น `gpiod`, `pj*`, `alsa`, `gps`, `Geographic`
- `main.cpp` จะสร้าง backend recorder (`mainwindowsiRec`) และ expose `Backend`, `mainwindows`, `fileReader`
- cursor ถูกซ่อน และ runtime env ถูกตั้งไปทาง embedded/eglfs

### PLATFORM_X86

ใช้เมื่อ build ด้วย mkspec:

```bash
qmake iScanMR10.pro -spec linux-g++
make
```

หรือบนเครื่อง desktop ที่ `qmake -query QMAKE_SPEC` เป็น `linux-g++`

เมื่อเข้า `PLATFORM_X86`:

- ใช้ source ชุด X86
- ไม่สร้าง recorder backend ที่อยู่ใต้ `#ifdef PLATFORM_JETSON`
- ไม่สั่ง GPIO/DSP hardware path
- cursor เป็น mouse cursor ปกติ
- runtime env จะพยายามใช้ Wayland หรือ XCB ตาม desktop session

## Build ที่ใช้ในงานนี้

สถานะที่ตั้งใจใช้ในโปรเจกต์นี้:

```text
Hardware Version: HW_5G
Platform: PLATFORM_JETSON
```

ถึงแม้จะ build บนคอมพิวเตอร์ แต่ถ้าใช้ mkspec `linux-jetson-orin-g++` โปรเจกต์จะถือว่าเป็น `PLATFORM_JETSON`.

ตัวอย่าง command:

```bash
qmake iScanMR10.pro -spec linux-jetson-orin-g++
make
```

## จุดแก้ค่าหลัก

### 1. เปลี่ยน 5G / non-5G

ไฟล์:

```text
iScanMR10.pro
```

แก้บรรทัด:

```qmake
CONFIG+=HW_5G
```

หรือ:

```qmake
CONFIG+=HW_NONE_5G
```

### 2. เปลี่ยน platform build

เลือกผ่าน qmake spec:

```bash
qmake iScanMR10.pro -spec linux-jetson-orin-g++
```

หรือ:

```bash
qmake iScanMR10.pro -spec linux-g++
```

### 3. ตรวจว่า qmake ปัจจุบันใช้ mkspec อะไร

```bash
qmake -query QMAKE_SPEC
```

ผลลัพธ์ตัวอย่าง:

```text
linux-g++
```

หรือ:

```text
linux-jetson-orin-g++
```

## Backend สำคัญ

### Mainwindows

ไฟล์:

```text
Mainwindows.h
Mainwindows.cpp
```

เป็น backend หลักที่เชื่อม QML กับระบบ receiver, OpenWebRX, scan profile, recorder bridge, network, VPN และ hardware control.

QML ส่งคำสั่งเข้ามาผ่าน signal:

```qml
qmlCommand(string msg)
```

และ C++ รับที่:

```cpp
Mainwindows::cppSubmitTextFiled(const QString &qmlJson)
```

### NetworkController

ไฟล์:

```text
NetworkController.h
NetworkController.cpp
```

รับผิดชอบ network จริง:

- LAN config
- WiFi state/scan/connect/disconnect/toggle
- WiFi saved profile lookup, forget profile, advanced IPv4 config
- Cellular/LTE status based on `rmnet_mhi0.1`, fallback `rmnet_mhi0`, `AT+CSQ`, and `mmcli`
- NTP/timezone helper

คำสั่งระบบที่เกี่ยวข้อง:

- `nmcli`
- `mmcli`
- `ifconfig`
- `ip`
- `socat`
- `/etc/network_config.json`
- `/etc/systemd/timesyncd.conf`

WiFi/LTE backend รุ่นนี้อ้างอิง behavior จาก web application local path:

```text
/home/only/Documents/remote/api.php
/home/only/Documents/remote/assets/js/wifi.js
```

ค่าตั้งต้นของ WiFi interface คือ:

```text
wlP9p1s0
```

แต่ backend จะ resolve จาก `nmcli -t -f DEVICE,TYPE device status` ก่อน ถ้าไม่เจอชื่อ exact จะ fallback แบบ case-insensitive และสุดท้ายใช้ WiFi device ตัวแรกที่ NetworkManager เห็น.

### Wifi5GController

ไฟล์:

```text
Wifi5GController.h
Wifi5GController.cpp
```

เป็น adapter สำหรับหน้า:

```text
Wifi5GPage.qml
```

หน้าที่คือรับ `menuID` จาก QML แล้วเรียก `NetworkController` เพื่อส่ง JSON response กลับ QML ผ่าน `Mainwindows::cppCommand`.

`Wifi5GController` ไม่ควร copy logic `nmcli/mmcli` จาก `NetworkController` เพราะจะทำให้ source of truth ซ้ำกัน

## QML สำคัญ

### main.qml

Root QML window ของแอป

### MainPage.qml

หน้า container หลัก ใช้ `StackView` สำหรับเปลี่ยนหน้า radio/map/DoA/WiFi/recorder

### Wifi5GView.qml

Pure UI สำหรับหน้า WiFi/5G:

- ไม่มี `Connections`
- ไม่เรียก `networkController`, `mainWindows`, หรือ context property ใดๆ
- รับข้อมูลผ่าน property และส่ง event ออกด้วย signal เท่านั้น
- มี mock default เพื่อเปิดใน Qt Creator Design mode ได้

### Wifi5GPage.qml

Runtime wrapper สำหรับหน้า WiFi และ 5G settings

ไฟล์นี้เป็นจุดที่ bind backend เข้ากับ `Wifi5GView.qml`:

ใช้ contract แบบ JSON ผ่าน `mainWindows.cppSubmitTextFiled()` และรอ response ผ่าน `mainWindows.cppCommand`.

QML รับค่า `HardwareHas5G` / `HardwareVersionName` และ `networkController` จาก `main.cpp` และรับซ้ำจาก `Wifi5GController` ตอน `getWifi5GPage`.
ถ้า `HardwareHas5G=false` หน้า QML จะซ่อน section 5G/Cellular และปรับ grid ให้ WiFi ใช้พื้นที่เต็มแถว.

ถ้าเปิดผ่าน Qt Creator Design mode แล้วไม่มี `networkController` / `mainWindows`, wrapper จะใช้ mock data แทนเพื่อไม่ให้ QML Puppet preview พัง.

### Wifi5GSetting.qml

Compatibility wrapper ที่ยังคงชื่อ component เดิมไว้ให้จุดเรียกเดิมในโปรเจคใช้งานต่อได้ โดยภายในโหลด `Wifi5GPage`.

menuID หลัก:

```text
getWifi5GPage
wifi_state
scan
join
disconnect
forget
advinfo
apply_ipv4
wifi_toggle
lte_state
cellularConnect
cellularDisconnect
listModems
```

ยังรองรับชื่อเดิมเพื่อ compatibility:

```text
wifiScan
wifiStatus
wifiConnect
wifiDisconnect
cellularStatus
```

ข้อมูลหลักที่ `scan` คืนให้ QML:

```text
key
ssid
bssid
channel
frequency
band
signal
secure
security
active
known
profile_name
device
```

ข้อมูลหลักที่ `lte_state` คืนให้ QML:

```text
sim_status
operator
signal
registration_state
access_technology
imei
iccid
ip_address
gateway
device
note
```

## Runtime paths ที่ควรรู้

โปรเจกต์นี้ผูกกับ path/runtime ของ target device หลายจุด:

```text
/etc/network_config.json
/var/lib/openwebrx/settings.json
/var/lib/openwebrx/scanpreset.json
/var/www/html/uploads
/var/www/html/vpnfile
/etc/openvpn/client/myvpn.conf
/tmp/alsarecd_id_1.log
/home/orinnx/saveFileName/filesNameWave.txt
```

การรันบน desktop/X86 อาจไม่ครบ feature เพราะ path, service, device node และ library บางตัวมีเฉพาะ target hardware

## วิธีตรวจสอบก่อน build

ตรวจ mkspec:

```bash
qmake -query QMAKE_SPEC
```

ตรวจ Qt version:

```bash
qmake -v
```

ตรวจ dependency ผ่าน pkg-config:

```bash
pkg-config --modversion openssl alsa geographiclib libgps
```

ถ้า build บน desktop แล้วเจอ error เช่น:

```text
Unknown module(s) in QT: multimedia
No package 'libgps' found
```

แปลว่า environment เครื่องนั้นยังขาด dependency สำหรับ build เต็ม ไม่ได้แปลว่า source logic ของ mode selector ผิด

## ข้อควรระวังในการแก้โค้ด

- อย่าแก้ `PLATFORM_JETSON` ด้วย manual define ถ้าไม่จำเป็น ให้เลือกผ่าน qmake mkspec
- ถ้าจะเปลี่ยน 5G/non-5G ให้แก้ `CONFIG+=HW_5G` หรือ `CONFIG+=HW_NONE_5G` ใน `iScanMR10.pro`
- อย่าใส่ `HW_5G` และ `HW_NONE_5G` พร้อมกัน
- ถ้าเพิ่ม source/header ใหม่ ต้องเพิ่มทั้ง `X86_SOURCES/X86_HEADERS` และ `JETSON_SOURCES/JETSON_HEADERS`
- อย่า log password หรือ secret จาก QML JSON ลง console/syslog
- Logic ที่สั่ง `nmcli/mmcli` ควรอยู่ใน `NetworkController` เป็นหลัก
- Class adapter เช่น `Wifi5GController` ควรจัดการ routing/JSON contract ไม่ควรถือ state ซ้ำกับ `NetworkController`
