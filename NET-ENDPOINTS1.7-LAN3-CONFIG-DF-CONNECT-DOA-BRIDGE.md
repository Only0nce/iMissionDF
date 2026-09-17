# NET-ENDPOINTS1.7 — LAN3 Config / DF Connect / DoA Live Bridge

## Objective

Keep the network model deliberately simple and separated by responsibility:

- **LAN3** configures the remote RFSoC `end0` interface.
- **DF Server IP** selects the RFSoC DF/control endpoint that iScan connects to.
- **DoA Viewer** displays live data received through the existing iScreenDF local bridge; it does not open a second remote RFSoC connection in the integrated application.

No RFSoC protocol fields are changed.

## Final ownership model

### LAN3 = CONFIG

Existing path retained:

`Setting.qml -> Mainwindows::applyLanSettings(index=2) -> Network2 row 3 -> updateNetworkDfDevice("end0", ...) -> iScreenDF::onUpdateNetworkDfDevice() -> setIpConfig JSON`

The existing `NetworkController::applyNetworkConfig("end0", ...)` path also persists `lan.rfsoc1` in `/etc/network_config.json` and intentionally skips local `nmcli` for `end*` interfaces.

Exact RFSoC packet remains:

```json
{
  "menuID": "setIpConfig",
  "ifname": "end0",
  "ip": "...",
  "netmask": "...",
  "gateway": "...",
  "dns1": "...",
  "dns2": "..."
}
```

LAN3 does **not** mutate `Parameter.ipdfserver`.

### DF Server IP = CONNECT

Existing remote connection owner retained:

`ServiceEndpointsPage.qml -> iScreenDF::connectToDFserver(ip)`

This path:

1. updates the runtime `m_ipdfServer`,
2. persists `Parameter.ipdfserver`,
3. connects `TcpClientDF` to `<ip>:5555`,
4. points GPSD to `<ip>:2947` using the existing behavior.

NET-ENDPOINTS1.7 additionally reuses the existing atomic `/etc/network_config.json` writer to mirror this endpoint as:

```json
{
  "endpoints": {
    "dfServerIp": "192.168.99.8"
  }
}
```

`Parameter.ipdfserver` remains the authoritative startup source. The JSON entry is a persistent mirror, not a second connection owner.

DF Server IP does **not** send `setIpConfig` and does **not** mutate LAN3/`Network2.id=3`.

## DoA Viewer live-data path

The source already contains the intended bridge:

`RFSoC :5555 -> TcpClientDF -> iScreenDF::updateFromTcpServer() -> TcpServerDF :9000 -> DoaClient -> ViewerPage.qml`

`iScreenDF::updateFromTcpServer()` broadcasts every received RFSoC JSON object to the local TCP clients before handling its own menu-specific state. `DoaClient` parses `DoAResult` packets and updates the QML properties used by `ViewerPage.qml`, including DoA angle/confidence, MUSIC spectrum, RF FFT and related state.

The defect was that the integrated `DoaClient` had no explicit product-owned local endpoint and the UI could point it elsewhere. NET-ENDPOINTS1.7 makes the integrated application use:

`127.0.0.1:9000`

The standalone DoaViewer diagnostic program keeps its editable host/port controls; only the integrated iScan context locks them to the local bridge.

## Files changed

Product code:

- `NetworkController.h`
- `NetworkController.cpp`
- `Mainwindows.cpp`
- `main.cpp`
- `ServiceEndpointsPage.qml`
- `iScreenDFqml/pages/SideSettingsDrawer.qml`
- `DoaViewer/TopBar.qml`

No changes are made to:

- `DatabaseDF::updateNetworkfromDisplay()` LAN3 row mapping,
- RFSoC `setIpConfig` JSON field names,
- `TcpClientDF` queue/reconnect behavior,
- LAN4/end1,
- LAN1/LAN2 local networking,
- DoA DSP/algorithm parsing.

## Expected runtime behavior

### Configure LAN3

1. Enter Admin mode.
2. Edit LAN3.
3. Apply.
4. `Network2.id=3` is updated.
5. `/etc/network_config.json -> lan.rfsoc1` is updated.
6. Exact `setIpConfig(ifname=end0, ...)` is sent/queued over the current DF control connection.
7. DF Server IP is unchanged.

### Connect DF Server

1. Set DF Server IP in Endpoints.
2. Apply.
3. `/etc/network_config.json -> endpoints.dfServerIp` is updated.
4. `Parameter.id=1.ipdfserver` is updated by the existing backend.
5. `TcpClientDF` connects to `<DF Server IP>:5555`.
6. GPSD follows the existing `<DF Server IP>:2947` behavior.
7. LAN3/end0 configuration is unchanged.

### Open DoA Viewer

1. Enter DoA Viewer.
2. Integrated `DoaClient` connects to `127.0.0.1:9000`.
3. RFSoC packets received by `TcpClientDF` are rebroadcast by `TcpServerDF`.
4. `DoaClient::parseJsonLine()` consumes `DoAResult` and the Viewer displays those live received values.

## Bench verification

Useful logs:

```text
[ENDPOINTS][APPLY] DF control target= 192.168.99.8
[ENDPOINTS][FILE] saved endpoints.dfServerIp= 192.168.99.8
[LAN][RFSoC-TCP] connectToHost 192.168.99.8:5555
[LAN][RFSoC-TCP] connected 192.168.99.8:5555
[DOA][BRIDGE] integrated viewer target=127.0.0.1:9000
[TcpServerDF] New client from "127.0.0.1" : <ephemeral-port>
```

When LAN3 is applied:

```text
[LAN][APPLY] index= 2 iface= end0 ...
[updateNetworkfromDisplay] Updated Network2 row id = 3
[iScreenDF][setIpConfig][JSON] = {"menuID":"setIpConfig","ifname":"end0",...}
[LAN][RFSoC][setIpConfig] DISPATCHED ...
```

## Target validation still required

This environment does not contain the target Qt 5.15.2/qmake toolchain, so target compilation and hardware RFSoC connectivity are not claimed here. Build and bench-test on the Jetson target.
