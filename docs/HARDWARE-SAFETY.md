# Hardware Safety

## Approval Boundary

Repository inspection, source review, and desktop compilation are read-only
with respect to target hardware. Obtain explicit approval before running any
command that can:

- enable RF transmission or change tuning/output state;
- write GPIO, SPI, I2C, device registers, DMA, or DSP configuration;
- reset or power-cycle a device or modem;
- change ALSA routing/volume on deployed equipment;
- alter network interfaces, routes, Wi-Fi, cellular, VPN, NTP, or firewall state;
- install firmware, services, packages, or persistent configuration.

## Required Review

Before active testing, document:

1. target model, hardware revision, firmware, and connected equipment;
2. active `HW_5G`/`HW_NONE_5G` selector and qmake platform;
3. exact command or UI action;
4. state that will change;
5. safe initial conditions and operator precautions;
6. expected output or measurement;
7. timeout and stop conditions;
8. rollback and recovery procedure;
9. logs/measurements to retain without secrets.

## Validation Order

1. Static source and contract review.
2. Desktop compile where applicable.
3. Target compile with the correct toolchain.
4. Read-only target identification and status.
5. Simulation or loopback where available.
6. Approved low-risk state change.
7. Instrumented full hardware test.
8. Long-duration and fault-recovery test.

Do not skip directly from compilation to uncontrolled target operation.

## Evidence

Hardware success requires direct evidence from the correct target. Record the
command/action, expected result, observed result, target logs, instrument
measurements, duration, and any rollback used. Static review, compilation, and
one successful run are not long-duration or production proof.

## Secrets and Network Credentials

Never record plaintext Wi-Fi/VPN passwords, tokens, private keys, cellular
credentials, or password hashes in Git or test logs. Report values as
`[REDACTED]`. Use environment or approved secret management and rotate any
credential-equivalent material that has been committed historically.
