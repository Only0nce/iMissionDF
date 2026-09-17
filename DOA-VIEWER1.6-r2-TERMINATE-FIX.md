# DOA-VIEWER1.6-r2 — StackView / SceneGraph Terminate Fix

## Motivation

Hardware test of DOA-VIEWER1.6 showed the DoA page became faster, but opening the
DoA Viewer could terminate with:

```text
[R13-FATAL] signal=11 ... qt_event=77(UpdateRequest)
event_receiver=QQuickWindowQmlImpl ... checkpoint=AUDIO_WRITE thread_checkpoint=WS_BINARY_RETURN
```

The important clue is `qt_event=UpdateRequest` on the `QQuickWindow`, not the
sticky audio checkpoint. The failure happens while Qt Quick is processing a
scenegraph update during navigation into the DoA page.

## Changes

1. `DoaViewer/ViewerPage.qml`
   - Removed `anchors.fill: parent` from the StackView-managed root page.
   - Replaced it with width/height bindings to the parent.
   - This removes the runtime warning:
     `StackView has detected conflicting anchors. Transitions may not execute properly.`

2. `FftWaterfallTextureItem`
   - Added destructor and `releaseResources()` to stop delayed budget-timer
     update requests when the item/window scenegraph is being released.
   - Replaced manual texture deletion / static_cast node reuse with conservative
     full-node replacement. `QSGSimpleTextureNode` now destroys its owned texture
     exactly once.
   - This avoids stale/dangling texture access during StackView transitions or
     QQuickWindow UpdateRequest processing.

3. `FftLineGraphItem`
   - Added destructor and `releaseResources()` to stop delayed budget-timer
     update requests and clear pending render state during page/window teardown.

## Scope

No changes to:

- CH1 -> RX/Home spectrum mapping
- CH2..CH6 -> DF CH1..CH5 mapping
- RFSoC protocol
- MUSIC/ESPRIT/Polar processing
- Network/database endpoint logic

## Expected acceptance result

Open `MAP`, switch to `DOA`, leave it running, switch back/forth several times.
The previous `QQuickWindow UpdateRequest` fatal should not recur.
