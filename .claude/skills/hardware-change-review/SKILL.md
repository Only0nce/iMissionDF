---
name: hardware-change-review
description: Classify hardware impact and require an approval-based target validation and rollback plan.
argument-hint: "[proposed hardware-facing change]"
---

# Procedure

1. Identify affected hardware, firmware, drivers, services, and application paths.
2. Classify every command as read-only or state-changing.
3. Trace GPIO, SPI/I2C, RF/TX, DMA, DSP, audio, modem, reset, power, and network effects.
4. Verify timing, widths, register maps, ownership, target guards, and fallback behavior.
5. Define prerequisites, instrumentation, expected results, stop conditions, and rollback.
6. Obtain approval before active target testing.
7. Never claim success without direct hardware evidence.

# Required Output

## Hardware Scope
## State Changes
## Safety Risks
## Approval Gate
## Validation Plan
## Stop and Rollback Plan
