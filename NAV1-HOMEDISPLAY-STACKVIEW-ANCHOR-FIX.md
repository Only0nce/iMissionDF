# NAV1 - HomeDisplay StackView Anchor Fix

## Symptom

When navigating from the RADIO/Spectrum page to Network Settings, Qt Quick printed:

```
qrc:/HomeDisplay.qml:13:1: QML HomeDisplay: StackView has detected conflicting anchors. Transitions may not execute properly.
```

## Root cause

`HomeDisplay.qml` is the root item managed by the outer `StackView` in `MainPage.qml`:

```qml
StackView {
    id: loader
    anchors.fill: parent
    initialItem: "qrc:/HomeDisplay.qml"
}
```

The `HomeDisplay` root also had `anchors.fill: parent`. `StackView` owns the page geometry during push/pop transitions, so anchoring the page root conflicts with the transition-controlled geometry.

## Fix

Removed only the root-level `anchors.fill: parent` from `HomeDisplay.qml`.

No internal anchors were changed. No Spectrum/Waterfall lifecycle, Network Settings logic, LAN R-LAN2 validation, audio path, or navigation behavior was changed.

## Expected result

Navigating RADIO/Spectrum -> NETWORK SETTINGS no longer emits the HomeDisplay conflicting-anchor warning, and StackView transitions can control page geometry normally.

## Verification

1. Start on RADIO/Spectrum.
2. Open the side navigation.
3. Navigate to NETWORK SETTINGS.
4. Confirm no `QML HomeDisplay: StackView has detected conflicting anchors` warning.
5. Navigate back to RADIO and verify Spectrum/Waterfall resume according to the existing `runtimeActive` contract.
6. Confirm audio behavior is unchanged.
