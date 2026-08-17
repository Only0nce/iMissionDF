# Frequency Original-Architecture R7

Authoritative QML state remains the project's original model:

- `centerFreq` = RF/source center
- `offsetFrequency` = DSP receiver offset
- `freqScan` = large receiver-frequency readout
- receiver displayed frequency = `centerFreq + offsetFrequency`

No new `receiverFrequency` QML property is introduced.

Fixes:

- backend `start_offset_freq` is pulled into `offsetFrequency`
- backend apply is guarded by `backendFrequencySync`
- `updateCenterFreq()` no longer forces `freqScan = centerFreq`
- legacy 30 MHz reset bounce is disabled
- duplicate mouse `setOffsetFrequency()` command path removed
- startup calls `updateCenterFreq()` after QML signal hookup
- `Mainwindows::onCenterFreqChanged()` no longer calls direct legacy
  `requestSetFreqAsync()`; AstraRX remains RF-center owner
