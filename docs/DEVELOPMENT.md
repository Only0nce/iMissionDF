# Development Environment

## Verified Build

iScanMR10 is a qmake Qt 5 project. `iScanMR10.pro` is authoritative.

Inspect the local environment:

```bash
./scripts/verify-dev-env.sh
./scripts/bootstrap-dev.sh
```

The bootstrap script reports missing dependencies and example package commands;
it does not install packages or modify global Claude configuration.

## Desktop Build

```bash
./scripts/build-desktop.sh
```

Defaults:

- qmake: `qmake` (override with `QMAKE_BIN`);
- mkspec: `linux-g++`;
- build directory: `build-claude-desktop` (override with `BUILD_DIR`);
- parallelism: `nproc` (override with `BUILD_JOBS`).

The development branch currently selects `HW_5G` in `iScanMR10.pro`, even for a
desktop build. Platform guards still select `PLATFORM_X86` for `linux-g++`.
Changing the hardware selector is a source change and must be reviewed.

## Jetson/Orin Target Build

```bash
TARGET_QMAKE=/path/to/target/qmake ./scripts/build-target.sh
```

Optional overrides:

```bash
TARGET_QMAKE_SPEC=linux-jetson-orin-g++
TARGET_BUILD_DIR=/path/to/build
BUILD_JOBS=4
```

The target qmake must know the project mkspec, sysroot, compiler, Qt libraries,
and target dependencies. A successful target compile still does not validate
GPIO, buses, RF, audio, modem, DSP, recorder, or other physical behavior.

## Expected Tools and Libraries

Core tools:

- Git
- Claude Code (for the shared AI workflow)
- qmake and Qt 5 development packages
- GNU Make
- a C++17 compiler
- Python 3 for project Claude hooks
- pkg-config

The project also references multimedia, WebSockets, SQL, OpenGL, OpenSSL, ALSA,
GPS, GeographicLib, GPIO, and target-specific libraries. Exact target locations
belong in the target toolchain/sysroot, not committed machine paths.

## Local Configuration

Use `.claude/settings.local.json` for machine-only Claude permissions or
preferences. Use the process environment for the optional
`ISCAN_NETWORK_ADMIN_PASSWORD_SHA256` override. Do not place the real value in
`.env.example`, Git, shell history, logs, or issue comments.

Each developer authenticates Claude Code with their own account. Do not copy
`~/.claude`, `~/.claude.json`, credential files, sessions, or plugin caches.

## Generated and Historical Local Files

The development branch historically tracks a generated `Makefile`,
`.qmake.stash`, `iScanMR10.pro.user`, and `.qtc_clangd` cache entries. New
instances are ignored. Existing tracked copies remain until a separate cleanup
reviews deletion impact.
