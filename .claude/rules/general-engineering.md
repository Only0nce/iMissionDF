# General Engineering Rules

- Establish root cause or design intent before modification.
- Preserve existing behavior and interfaces outside the requested scope.
- Prefer the smallest coherent change; avoid speculative rewrites.
- Verify documentation, comments, and historical assumptions against current source.
- Explain trade-offs, compatibility impact, failure modes, and rollback.
- Do not install or upgrade dependencies without approval.
- Do not edit generated files or machine-local state.
- Keep these evidence levels separate:
  1. static review;
  2. configuration;
  3. compilation;
  4. unit/integration tests;
  5. runtime testing;
  6. simulation;
  7. target execution;
  8. physical hardware validation.
- Report exact files, commands, results, unknowns, and validation not performed.
