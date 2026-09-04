# Scratcher VST — TODO / Fix Checklist

## Issues (prioritised)

### Issue 8 — Default sample HOLD mode; PLAY disabled until real sample loads
- [x] `DeckProcessor.h`: add `atomDefaultSample` atomic, `hasSampleLoaded()` method
- [x] `DeckProcessor.cpp` `generateDefaultSample()`: call `hold()` at end, set `atomDefaultSample = true`
- [x] `DeckProcessor.cpp` `loadFromBuffer()`: set `atomDefaultSample = false`
- [x] `PluginProcessor.cpp` `prepareToPlay()`: remove `setLoop` + `play()` after `generateDefaultSample()`
- [x] `PluginEditor.cpp` `DeckPanel` constructor: disable `btnPlay` when no real sample
- [x] `PluginEditor.cpp` `DeckPanel::onSampleLoaded()`: enable `btnPlay`, then play
- [x] `PluginEditor.cpp` `DeckPanel::timerCallback()`: keep btnPlay enabled state in sync

### Issue 1 — Default plays 1/4 sample instead of full
- [x] `PluginEditor.cpp` DeckPanel constructor: change `cmbAutoPreset.setSelectedId(2…)` → `setSelectedId(1…)`
- [x] Change `btnL1` text from "1/4" → "1 BAR", `btnL2` from "1/2" → "2 BAR"
- [x] `buttonClicked`: FULL button uses `getTrimEnd()`/`getTrimStart()`; bar buttons use bars
- [x] `DeckProcessor.h`: add `getTrimStart()` / `getTrimEnd()` (covered by Issue 2)

### Issue 3 — Vinyl rotation tied to actual playback position
- [x] `PluginEditor.h` `VinylComponent`: add `addPlayheadDelta()`, `pendingHeadDelta`, `prevHeadDeltaConsumed`
- [x] `PluginEditor.h` `DeckPanel`: add `prevPlayheadNorm` member
- [x] `PluginEditor.cpp` `DeckPanel::timerCallback()`: compute headDelta, call `vinyl.addPlayheadDelta()`
- [x] `PluginEditor.cpp` `VinylComponent::timerCallback()`: use headDelta for rotation

### Issue 4 — Better vinyl scratch physics
- [x] `DeckProcessor.cpp` processBlock: remove `atomSpeed` multiplier from `rawSpeed`
- [x] Faster direction change: `smooth = 0.10` (was 0.25)
- [x] `DeckProcessor.h`: add `handReleaseCoastBlocks`, `HAND_COAST_BLOCKS`, `prevHandState`
- [x] `DeckProcessor.cpp` processBlock: coast logic on hand release, track `prevHandState`

### Issue 5 — Crossfader MIDI binding not wired up
- [x] `PluginEditor.h` `CrossfaderStrip`: add `Button::Listener` inheritance + `buttonClicked` decl
- [x] `PluginEditor.cpp` CrossfaderStrip constructor: `btnMidi.addListener(this)`, curve btn listeners
- [x] `PluginEditor.cpp`: implement `CrossfaderStrip::buttonClicked()`

### Issue 6 — Per-deck MIDI scratch binding
- [x] `PluginProcessor.h`: add `midiScratchCCA/B`, `midiScratchLearningA/B`, `lastScratchCCValueA/B`
- [x] `PluginProcessor.cpp` processBlock MIDI loop: per-deck learn + per-deck CC scratch
- [x] `PluginProcessor.cpp`: route `setExternalScratchSpeed` per-deck vs global
- [x] `PluginEditor.h` `DeckPanel`: add `updateMidiScratchButton()` decl
- [x] `PluginEditor.cpp` `DeckPanel::buttonClicked`: use per-deck learning flags
- [x] `PluginEditor.cpp`: implement `DeckPanel::updateMidiScratchButton()`
- [x] `PluginEditor.h` editor: add `updateMidiScratchButtons()` decl
- [x] `PluginEditor.cpp`: implement `ScratcherAudioProcessorEditor::updateMidiScratchButtons()`

### Issue 7 — Slicer waveform view + audio data-race fix
- [x] `DeckProcessor.h`: replace non-atomic `loopStart/End/cuePoint/loopEnabled` with atomics
- [x] `DeckProcessor.cpp`: update all uses to atomic load/store
- [x] `DeckProcessor.h`: add `getLoopStartSample()`, `getLoopEndSample()`, `isLoopEnabled()`
- [x] `PluginEditor.h`: add `SlicerWaveformView` class
- [x] `PluginEditor.h` editor: add `slicerWaveformView` member
- [x] `PluginEditor.cpp`: implement `SlicerWaveformView`
- [x] `PluginEditor.cpp` editor constructor: add + hide `slicerWaveformView`
- [x] `PluginEditor.cpp` `layoutComponents()`: toggle scope/slicerWaveformView
- [x] `PluginEditor.cpp` `btnSlicer` handler: call `resized()`

### Issue 9 — CUE behavior fix
- [x] `DeckProcessor.h`: add `getPlayheadSample()` method
- [x] `PluginEditor.cpp` `DeckPanel::buttonClicked`: fix CUE logic (set cue when stopped, return when playing)

### Issue 2 — Sample trim knobs
- [x] `DeckProcessor.h`: add `atomTrimStart/End`, `setTrimStart/End()`, `getTrimStart/End()`, norm getters
- [x] `DeckProcessor.cpp` processBlock: apply trim constraints to loop boundary
- [x] `PluginProcessor.cpp` `createParameterLayout()`: add trim params a_trimStart/End, b_trimStart/End
- [x] `PluginProcessor.h`: add `rawATrimStart/End`, `rawBTrimStart/End` pointers
- [x] `PluginProcessor.cpp` constructor: cache trim param pointers
- [x] `PluginProcessor.cpp` processBlock: apply trim to decks
- [x] `PluginEditor.h` `DeckPanel`: add trim sliders + labels + attachments
- [x] `PluginEditor.cpp` DeckPanel constructor: setup trim sliders
- [x] `PluginEditor.cpp` DeckPanel resized: layout trim sliders
- [x] `PluginEditor.cpp` DeckPanel timerCallback: update waveform trim bounds
- [x] `PluginEditor.h` `WaveformOverview`: add `trimStart/End`, `setTrimBounds()`
- [x] `PluginEditor.cpp` `WaveformOverview::paint()`: draw trim dimming + separators

### Additional fixes
- [x] Slicer deck B `slicerActiveNote` tracking in processBlock
- [x] `MidiLearnManager.h`: add `cancelLearn()` method

## Round 2 (2026-03-22)

### Trim UX — draggable handles on waveform (replace sliders)
- [x] `PluginEditor.h` `WaveformOverview`: add `mouseDrag`, `mouseUp`, `mouseMove`, `onTrimChanged` callback, `draggingHandle` state
- [x] `PluginEditor.h` `DeckPanel`: remove `slTrimStart/End`, `lblTrimStart/End`, `attachTrimStart/End`
- [x] `PluginEditor.cpp` `WaveformOverview`: implement handle detection (8px grab zone), drag → `onTrimChanged`, resize cursor hint
- [x] `PluginEditor.cpp` `WaveformOverview::paint()`: render grab diamonds at trim handles
- [x] `PluginEditor.cpp` `DeckPanel` constructor: wire `waveform.onTrimChanged` → `proc.apvts.getParameter(...)->setValueNotifyingHost()`
- [x] `PluginEditor.cpp` `DeckPanel::resized()`: remove trim slider layout

### Slicer — push behaviour + zoom/scroll + per-slice envelope UI
- [x] `PluginEditor.h` `SlicerWaveformView`: add `zoomLevel`, `scrollOffset`, `selectedDeck/Slice`, env drag state, `mouseWheelMove`, `mouseDoubleClick`, helpers `normToX`/`xToNorm`
- [x] `PluginEditor.cpp` `SlicerWaveformView::paint()`: render via zoom/scroll, ENV_STRIP_H bottom strip per deck showing ATK/DCY for selected slice
- [x] `PluginEditor.cpp` `SlicerWaveformView::mouseDrag()`: push-neighbour logic when dragging slice past adjacent; env strip drag adjusts attack/decay ms
- [x] `PluginEditor.cpp` `SlicerWaveformView::mouseDown()`: hit-test env strip vs waveform; click slice = select; click empty = deselect
- [x] `PluginEditor.cpp` `SlicerWaveformView::mouseWheelMove()`: Ctrl+wheel = zoom (anchored to cursor), plain wheel = scroll
- [x] `PluginEditor.cpp` `SlicerWaveformView::mouseDoubleClick()`: reset zoom to 1×

### SlicePad — remove drag-to-adjust
- [x] `PluginEditor.h` `SlicePad`: remove `onAdjust`, `dragging`, `dragStartX`, `dragAccumPx`, `PX_PER_FULL`, `mouseDrag`
- [x] `PluginEditor.cpp` `SlicePad`: simplified `mouseDown` (trigger or learn only), removed `mouseDrag`
- [x] `PluginEditor.cpp` `SlicerPanel` constructor: removed `onAdjust` lambdas

### Per-slice attack/decay envelopes
- [x] `DeckProcessor.h`: add `atomSliceAttackSamples`, `atomSliceDecaySamples`, `fadeInTotal`, `setSliceAttackMs()`, `setSliceDecayMs()`, `getSampleRate()`
- [x] `DeckProcessor.cpp` `seekToSample()`: uses `atomSliceAttackSamples` for fade-in length
- [x] `DeckProcessor.cpp` processBlock: reads `decaySamples` once per block; applies linear fade-out near loop end
- [x] `PluginProcessor.h`: add `sliceAttackMsA/B[8]`, `sliceDecayMsA/B[8]`
- [x] `PluginProcessor.cpp` constructor: init attack=10ms, decay=20ms for all slices
- [x] `PluginProcessor.cpp` processBlock MIDI: `setSliceAttackMs`/`setSliceDecayMs` before slice trigger
- [x] `PluginEditor.cpp` `SlicerPanel` `onTrigger` lambdas: `setSliceAttackMs`/`setSliceDecayMs` before trigger

### Preset save/restore — loaded file paths
- [x] `SampleManager.h`: add `lastLoadedFile` to `DeckInfo`, add `getLastLoadedFile()` method
- [x] `SampleManager.cpp` `run()`: store `req.file` as `lastLoadedFile`
- [x] `PluginProcessor.cpp` `getStateInformation()`: save `filePathA/B` + slice positions + attack/decay ms
- [x] `PluginProcessor.cpp` `setStateInformation()`: restore slice data + async reload from saved paths

### Drag & drop audio files onto decks
- [x] `PluginEditor.h` `DeckPanel`: add `juce::FileDragAndDropTarget` inheritance, `isInterestedInFileDrag`, `filesDropped`, `fileDragEnter/Exit`, `fileDragOver` state
- [x] `PluginEditor.cpp` `DeckPanel::paint()`: highlight border + "DROP AUDIO" when `fileDragOver`
- [x] `PluginEditor.cpp` `DeckPanel::isInterestedInFileDrag()`: accept wav/mp3/aiff/ogg/flac/m4a
- [x] `PluginEditor.cpp` `DeckPanel::filesDropped()`: call `sampleMgr.loadFile()` with the dropped file
