# DOA-VIEWER1.1 — CH1 RX / CH2..CH6 DF Logical Display Sources
Date: 2026-09-17
Baseline: NET-ENDPOINTS1.10 DB-authoritative endpoint source

## Goal
Extend the existing DoA Viewer ADC CH dropdown from five displayed choices to six logical display sources without changing the coherent five-channel DoA/MUSIC backend.

Mapping:

- CH1 -> AstraRX/Home RX FFT acquisition, rendered with the existing DoA Viewer `FftPlot.qml` + `WaterfallCanvas.qml`.
- CH2 -> existing DF ADC CH1 (physical channel 0).
- CH3 -> existing DF ADC CH2 (physical channel 1).
- CH4 -> existing DF ADC CH3 (physical channel 2).
- CH5 -> existing DF ADC CH4 (physical channel 3).
- CH6 -> existing DF ADC CH5 (physical channel 4).

The DoA polar/MUSIC result remains the existing coherent five-channel result regardless of the selected display source.

## Architecture

### CH1 / RX source
`WebSocketClient::spectrumDisplayFrame(QVector<float>)` remains the production-native AstraRX/Home acquisition path.

A new Mainwindows bridge is active only while DoA Viewer CH1 is visible/selected. It:

1. takes the same native Home spectrum frame,
2. peak-pools it to 1024 display bins,
3. throttles the QML bridge to about 30 fps,
4. exposes magnitude + center/sample-rate metadata,
5. lets ViewerPage generate the frequency axis only when geometry changes.

No second RF receiver, WebSocket, FFT DSP chain, or RF tune owner is created.

### Shared FFT ownership
The old `WebSocketClient::setFftUiActive(bool)` single-owner gate could let Home and DoA Viewer accidentally suspend each other. It is now backed by named consumer leases:

- `home-spectrum`
- `doa-viewer-rx`

The FFT receive/decode path stays active while at least one consumer owns a lease. Existing Home QML keeps using the old API unchanged.

### DF mapping
ViewerPage owns the logical source selection. CH1 never sends `setAdcChannel`. CH2..CH6 map to the legacy physical 0..4 channel contract exactly.

## UI behavior
The visual renderer is still the DoA Viewer renderer.

- CH1 title: `RF FFT Spectrum (CH1 / RX)`
- CH2 title: `RF FFT Spectrum (CH2 / DF1)`
- ...
- CH6 title: `RF FFT Spectrum (CH6 / DF5)`

CH1 intentionally disables DoA click-to-offset and the DF target-band overlay because RX/Home can have a much wider sample-rate domain. CH2..CH6 preserve the existing DF click/offset behavior.

## Deliberately unchanged
- DoAClient RFSoC protocol
- physical ADC numbering 0..4
- MUSIC / ESPRIT calculation
- DoaPolarPlot
- array/coherence logic
- DF target offset/BW semantics for DF channels
- Network Settings / endpoints
- AstraRX RF/source tuning ownership

## Runtime acceptance
1. Open DoA Viewer. Dropdown must contain CH1..CH6.
2. CH1 must show the current AstraRX/Home spectrum and waterfall using DoA Viewer visuals.
3. Change Home/AstraRX center/sample-rate; CH1 X-axis must follow that source metadata.
4. Selecting CH1 must log `no_setAdcChannel=1` and must not send a physical ADC command.
5. CH2 must reproduce old DF CH1; CH6 must reproduce old DF CH5.
6. Rapidly switch CH1 -> CH6 -> CH2 -> CH1. No stale mapping/crash is allowed.
7. DoA Polar must continue updating in every selection.
8. Leaving DoA Viewer must release only `doa-viewer-rx`; Home FFT must remain active if Home owns its lease.
