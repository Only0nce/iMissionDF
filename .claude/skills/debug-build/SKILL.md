---
name: debug-build
description: Diagnose qmake or compiler failures using the authoritative project file and a clean isolated build directory.
argument-hint: "[build error or target]"
---

# Purpose

Find the first causal build failure without changing unrelated source.

# Procedure

1. Record Git status, Qt/qmake/compiler versions, platform, and active hardware selector.
2. Confirm `iScanMR10.pro` is authoritative.
3. Reproduce in a dedicated build directory using the intended mkspec.
4. Capture the first meaningful configure/compiler/linker error.
5. Distinguish missing host dependencies, target toolchain problems, generated-file
   staleness, qmake source-list errors, and source defects.
6. Apply a fix only when explicitly requested; otherwise report diagnosis.
7. Never install packages automatically or substitute CMake.

# Required Output

## Environment
## Reproduction Command
## First Causal Error
## Root Cause
## Proposed Fix
## Validation Needed
