V18 - Select Mode icon size refinement

Changed file:
- iScreenDFqml/pages/SideSettingsDrawer.qml

What changed:
1. Increased Select Mode icon size after the drawer was widened.
2. Kept icon size responsive to tile width so it grows when the drawer grows.
3. Added a cap so icons do not hit the tile border, overlap labels, or collide with other components.
4. Slightly increased Select Mode tile height to preserve spacing.
5. Reduced icon/label column spacing a little to keep the layout balanced.

Expected result:
- Icons look larger and clearer.
- Labels still stay inside their tiles.
- No overlap with the 4-icon bar or RF Receiver Configuration.
