---
name: plan-change
description: Produce a source-verified implementation, compatibility, validation, and rollback plan before a non-trivial change.
argument-hint: "[requested change]"
---

# Purpose

Plan the smallest coherent change that preserves project contracts.

# Procedure

1. Establish current behavior and reproduce or locate the requirement.
2. Trace affected C++, QML, thread, process, protocol, persistence, and hardware paths.
3. List invariants and compatibility requirements.
4. Compare viable options and reject unnecessary rewrites.
5. Identify exact files and ordered implementation steps.
6. Define static, build, test, runtime, target, and hardware validation separately.
7. Define rollback and unresolved approval points.
8. Do not implement while this skill is active unless the user explicitly asks to continue.

# Required Output

## Current Behavior
## Root Cause or Design Need
## Contracts
## Options
## Selected Plan
## Validation Gates
## Rollback
## Open Decisions
