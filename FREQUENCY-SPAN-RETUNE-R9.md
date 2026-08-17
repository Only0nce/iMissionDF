# Qt5 AstraRX span-aware tuning R9

## Contract
- AstraRX is the only RF/source center owner.
- In-span receiver tuning sends only `dspcontrol.offset_freq`.
- Out-of-span tuning sends only `setfrequency`; QML does not mutate its confirmed `centerFreq` before AstraRX confirms.
- `center_freq + start_offset_freq == start_freq` is the receiver state contract.
- The Qt5 endpoint is `/ws/qt5`; only one WebSocket connection is opened.

## Span decision
Qt uses the same guard rule as AstraRX: `limit = sample_rate/2 - demod_bandwidth/2`.
For 7.680000 MHz sample rate the raw half-span is 3.840000 MHz.

## Out-of-span transaction
1. QML records a pending center request and shows the requested receiver.
2. Qt sends one `setfrequency` command.
3. AstraRX `SourceManager::tune_active_source()` sends RFSoC `SETFREQ` and updates persistence only after hardware/source success.
4. AstraRX resets the per-client DSP offset to zero and sends Qt5 frequency config.
5. Qt applies the confirmed center/receiver snapshot and clears pending state.
6. AstraRX broadcasts native status so Active RF Source / RF Center updates immediately in the browser.
7. Error or 3-second timeout restores the last confirmed AstraRX snapshot.
