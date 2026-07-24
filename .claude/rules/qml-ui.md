# QML and UI Rules

- Preserve navigation, focus, gestures, touch behavior, and user-visible contracts.
- Preserve signal names, parameters, object names, context properties, and JSON fields.
- Avoid binding loops, hidden mutable state, and imperative writes that break bindings.
- Avoid unnecessary redraw, model churn, allocation, and heavy JavaScript in render-critical paths.
- Bound high-rate spectrum, waterfall, log, and network updates.
- Keep view components presentation-focused and system commands in C++ controllers.
- Provide loading, empty, disconnected, error, and retry states.
- Maintain readable contrast, scalable text, keyboard/focus behavior, and practical touch targets.
- Verify design-mode fallbacks do not mask runtime failures.
