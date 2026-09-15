# UX-KB2.1 — WiFi Connection Recovery

Baseline: UX-KB2 built from the authoritative `src.tar(20260914-101931).xz` source line.

## Problem

The WiFi page could show zero networks / lose previously visible connections when the page started or when a transient scan failed. A previous screenshot also showed `Device 'wlan0' not found`.

Two behaviors were responsible:

1. `Wifi5GView.qml` started with `wifiIface = "wlan0"` while the backend resolves the product WiFi interface dynamically (preferred `wlP9p1s0`, otherwise the first real NetworkManager WiFi device).
2. `Wifi5GPage.qml::activateWifiPageRuntime()` sent `wifi_config`, `wifi_state`, and `scan` back-to-back. `wifi_config` is asynchronous, so state/scan could run before the actual interface was resolved.
3. Any failed scan replaced `wifiList` with an empty list, destroying the last known-good connection list.
4. The IPv4 editor floated over most of the Available Networks pane, making other WiFi entries appear to disappear while configuration was open.

## Fix

### Interface-first startup

WiFi runtime now resolves `wifi_config` first. Status and scan start only after the actual interface is known.

If config resolution takes longer than 3 seconds, the page falls back to an empty interface string, which asks `NetworkController::resolveWifiInterface()` to auto-detect the real NetworkManager WiFi device rather than using a stale hard-coded device.

### Last-good scan retention

A transient `nmcli`/interface scan error no longer replaces `wifiList` with `[]`. The page keeps the last successful scan and shows the error message with `showing previous scan results`.

A successful scan with zero networks still correctly clears the list.

### Persistent connection visibility

When WiFi IPv4 configuration is open:

- the current WiFi card remains visible on the left;
- the editor stays inside the upper part of the Available Networks pane;
- the WiFi list moves below the editor instead of being hidden behind it.

### Diagnostics

`NetworkController::scanWifiPage()` now logs:

```
[WiFi][SCAN] requested= ... resolved= ...
```

and on failure:

```
[WiFi][SCAN] failed requested= ... resolved= ... error= ...
```

## Preserved contracts

- WiFi connect/disconnect/forget/password protocol unchanged.
- NetworkManager remains the WiFi implementation owner.
- WiFi IPv4 apply backend unchanged.
- JSON persistence unchanged.
- LAN1-LAN4 / Network2 / RFSoC TCP code unchanged.
- 5G backend behavior unchanged.

## Expected startup log

The page should first resolve the interface:

```
[WiFiStartup] wifi_config resolved interface: wlP9p1s0
[WiFiStartup] interface resolved: wlP9p1s0
[WiFi][SCAN] requested= wlP9p1s0 resolved= wlP9p1s0
```

The exact interface may differ if NetworkManager reports a different real WiFi device.
