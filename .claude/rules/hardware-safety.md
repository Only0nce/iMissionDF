# Hardware Safety Rules

- Identify whether each command only observes state or can change hardware/system state.
- Obtain approval before GPIO output, RF/TX, DMA, SPI/I2C writes, register writes,
  reset, power, modem, audio routing, firmware, or network-interface changes.
- Verify target model, hardware revision, firmware, active hardware selector, and platform guard.
- Preserve register maps, widths, signedness, endianness, timing, reset behavior, and device ownership.
- Define expected measurements, instrumentation, timeout, abort conditions, and rollback before testing.
- Start with harmless read-only status and identification commands.
- Never claim hardware success without direct target evidence.
