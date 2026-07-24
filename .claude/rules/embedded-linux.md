# Embedded Linux Rules

- Separate desktop behavior, target behavior, and service behavior.
- Use argument lists rather than shell composition for external processes where possible.
- Set timeouts, bound output, check exit status, and clean up child processes.
- Handle partial socket I/O, framing, reconnect backoff, and interface disappearance.
- Verify service users, groups, file/device permissions, startup order, restart policy, and logs.
- Treat sysfs, device files, system configuration, and network configuration as state-changing.
- Preserve atomic file writes, ownership, modes, and rollback copies for persistent configuration.
- Do not assume host package paths, mkspecs, sysroots, devices, or services exist on another machine.
