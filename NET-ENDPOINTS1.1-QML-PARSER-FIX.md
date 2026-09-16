# NET-ENDPOINTS1.1 — QML Parser Fix

Base: NET-ENDPOINTS1 Service Endpoints Tab.

## Runtime failure

The target reported:

```text
QQmlComponent: Component is not ready
qrc:/MainPage.qml:74:5: QML StackView: push: qrc:/Setting.qml:776 Unexpected token `;'
qrc:/Setting.qml:776 Unexpected token `;'
qrc:/Setting.qml:777 Unexpected token `;'
```

## Root cause

`Setting.qml` contained compact button declarations with a semicolon immediately after a QML child object:

```qml
Behavior on scale { NumberAnimation { duration: 90 } };
```

`Behavior` is a QML child object declaration, not a JavaScript statement. The trailing semicolon is rejected by the Qt 5 QML parser.

The same compact-object pattern also existed in several `VpnPage.qml` blocks (`Behavior`, sibling `Text`/`Button`/`Item` objects). Those were normalized in the same revision so the next lazy-loaded page does not fail after `Setting.qml` is fixed.

## Fix

- Reformat LAN Apply confirmation buttons in `Setting.qml` into normal multiline QML syntax.
- Remove semicolons after `Behavior` child declarations.
- Normalize affected `VpnPage.qml` child-object declarations and dialog rows to QML-safe syntax.
- No behavior, role policy, service endpoint contract, VPN state logic, LAN logic, database behavior, or RFSoC protocol was changed.

## Product files changed

- `Setting.qml`
- `VpnPage.qml`

## Validation

This environment does not provide the target Qt/qmake/qmllint toolchain, so validation here is structural/static. The target build should verify:

1. Network Settings opens without `Unexpected token ';'`.
2. Endpoints tab opens.
3. VPN tab opens without parser errors.
4. LAN Apply confirmation still animates and calls the same existing apply path.
