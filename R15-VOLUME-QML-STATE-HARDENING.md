# R15 Volume / QML State Hardening

Revision: `20260817-volume-qml-state-hardening-r15`

## Scope

R15 is based on the user-supplied Qt source tree (`src.tar(1).xz`), which already contains the R14 frequency transaction hardening and uses the local AstraRX endpoint `ws://127.0.0.1:8074/ws/qt5`.

AstraRX source and the R14 frequency algorithm are intentionally unchanged.

## Root-risk addressed

The volume UI had multiple writers for the same QML state:

- `HomeDisplay.scanVolLevel`, `scanAudioLevel`, and `scanVolLevelHeadphone` were authoritative properties.
- Rotary handling changed those properties and then imperatively assigned the drawer properties again, which can remove QML bindings.
- `MyDrawer` instantiated both `VolDrawer` and `VolumeDrawer`. Both listened to `slider.onValueChanged`, so programmatic slider updates could write back into the same parent state.
- Both drawer components also mutated their own `mute` property even though it was externally bound to `scanMuteOn`.
- C++ getter/setter ownership did not match the actual channels: speaker writes CH4 but the getter read CH1; headphone writes CH2 but the getter read CH3.
- Speaker hardware retains the legacy policy where requested values below 165 use effective hardware value 50. Comparing QML against the effective value would therefore never converge for values in that range.

## R15 design

### Single authoritative QML volume state

`HomeDisplay` remains the source of truth:

- `scanVolLevel`
- `scanVolLevelHeadphone`
- `scanAudioLevel`
- `scanMuteOn`

Rotary handling only changes these properties. It no longer assigns `ctrl.ctrlLevel`, `ctrl.audioLevel`, or `ctrl.headphoneCtrlLevel` directly.

### Passive drawer components

`VolDrawer` and `VolumeDrawer` are now presentation components:

- programmatic slider changes no longer change mute state;
- mute buttons emit `muteToggleRequested()` instead of writing `mute` or directly controlling SQL/WebSocket state;
- the two drawers can both exist without both acting as state writers.

### User-only slider writeback

`MyDrawer` uses `Slider.moved` rather than `valueChanged` for speaker/headphone/software-volume writeback. Therefore a value changed by QML binding updates the view but does not write the same value back into `HomeDisplay`.

Only the currently active drawer (`itemShow == 1` or `itemShow == 5`) accepts user slider movement.

### One mute owner

`MyDrawer.toggleVolumeMute()` performs the existing mute/SQL behaviour once. A single `Connections` object mirrors `wsClient.mutedChanged` into `scanMuteOn`.

The child drawers no longer break the external `mute: scanMuteOn` binding.

### Requested vs effective hardware volume

C++ now tracks:

- `requestedSpeakerVolume`
- `requestedHeadphoneVolume`

The speaker setter preserves the existing effective hardware policy:

```
requested < 165 -> effective CH4 = 50
requested >= 165 -> effective CH4 = requested
```

but `getSpeakerVolume1()` reports the requested state. This allows QML comparison to converge without changing the existing hardware output mapping.

Headphone requested state is tied to CH2, matching the actual setter path.

### Diagnostics

New persistent checkpoints:

- `1000 VOLUME_ROTARY`
- `1001 VOLUME_STATE`
- `1002 VOLUME_HARDWARE`
- `1003 VOLUME_SOFTWARE`
- `1004 VOLUME_DRAWER`

Examples expected in `/tmp/iScanMR10-debug.log`:

```
domain=VOLUME event=qml state detail=rotary-speaker
domain=VOLUME event=qml state detail=speaker-volume-state
domain=VOLUME event=speaker hardware apply
domain=VOLUME event=qml state detail=drawer-open
```

If a crash remains, `thread_checkpoint` will now show whether it happened while the main thread was processing volume state, drawer state, or hardware apply.

## Behaviour intentionally preserved

- Volume ranges and step size.
- Speaker CH4 output path.
- Headphone CH2 output path.
- Speaker `<165 -> effective 50` policy.
- Software volume call to `WebSocketClient::setVolumePercent()`.
- Mute/SQL policy used by the existing volume drawer button.
- Current Rx persistence through `maybeUpdateCurrentRx()`.
- R14 frequency transaction behaviour.
- AstraRX source/backend behaviour.

## Build validation limitation

The current execution environment does not contain the user's Qt 5.15.2/qmake Jetson toolchain, so this package does not claim a Jetson compile pass. Static structural and contract validation is included in `R15-STATIC-VALIDATION.txt` and `VERIFY-VOLUME-QML-R15.sh`.
