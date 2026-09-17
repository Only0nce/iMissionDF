# DOA-VIEWER1.7-r5 — CH1 FFT Toggle Parity

## Purpose

Make logical CH1 (AstraRX/Home RX spectrum source) follow the same user-facing FFT ON/OFF behavior as CH2..CH6 DF channels.

Before this revision, the FFT switch was disabled when CH1 was selected. CH1 was treated as always enabled whenever the RX source was available. That meant the user could not hide/suspend the CH1 FFT/Waterfall path from the DoA page.

## Scope

Changed only QML control/routing logic:

- `DoaViewer/FftControlPanel.qml`
- `DoaViewer/TopBar.qml`
- `DoaViewer/ViewerPage.qml`

No RFSoC protocol, DF channel mapping, CUDA renderer, Waterfall texture, database, network, or MUSIC/ESPRIT algorithm behavior is changed.

## Behavior

- CH1 FFT switch is now enabled when the RX/Home source is available.
- CH1 OFF disables both Spectrum and Waterfall in the DoA page.
- CH1 OFF calls `mainWindows.setDoaRxSpectrumActive(false)` through the existing consumer lease path, so the secondary DoA RX FFT consumer is suspended.
- CH1 ON re-enables the RX FFT consumer and republishes the latest frame when available.
- CH2..CH6 continue to use `doaClient.spectrumEnabled` exactly as before.
- CH1 ON/OFF state is persisted in `Settings` under category `FftWaterfall` as `rxFftEnabled`.

## Test checklist

1. Open DoA Viewer, select CH1.
2. Toggle FFT OFF.
   - Spectrum shows FFT OFF.
   - Waterfall shows WATERFALL OFF.
   - Log should show `[DOA-RX-FFT] OFF ...`.
   - FFT runtime should suspend consumer `doa-viewer-rx` if no other consumer is active.
3. Toggle FFT ON.
   - CH1 spectrum and waterfall resume.
   - Log should show `[DOA-RX-FFT] ON ...`.
4. Switch CH2..CH6 and confirm their FFT switch still controls RFSoC/DF `doaClient.spectrumEnabled`.
5. Navigate MAP -> DOA and confirm saved CH1 FFT state is restored.
