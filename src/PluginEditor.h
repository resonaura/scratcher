#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"
#include "SampleManager.h"

//==============================================================================
// Colour theme
//==============================================================================
struct ScratcherTheme
{
    juce::Colour bg        { 0xff000000 };  // black
    juce::Colour bgDark    { 0xff000000 };  // black
    juce::Colour bgPanel   { 0xff080808 };  // near-black
    juce::Colour deckA     { 0xff3772ff };  // синька
    juce::Colour deckB     { 0xfffdca40 };  // крысиный
    juce::Colour accent    { 0xff3772ff };  // синька (primary accent)
    juce::Colour textBright{ 0xffe6e8e6 };  // побелка-белка
    juce::Colour textDim   { 0xff6a6c6a };  // dimmed text
    juce::Colour groove    { 0xff1a1a1a };  // subtle groove
    juce::Colour lit       { 0xfffdca40 };  // крысиный (lit/active)
};

//==============================================================================
// LAF
//==============================================================================
class ScratcherLAF : public juce::LookAndFeel_V4
{
public:
    ScratcherLAF();
    void setTheme(const ScratcherTheme& t) { theme = t; }

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float min, float max,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int,
                      juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getLabelFont(juce::Label&) override;

private:
    ScratcherTheme theme;
    juce::Font     pixelFont { juce::FontOptions{} };
    juce::Font     uiFont    { juce::FontOptions{} };
};

//==============================================================================
// VU meter (shared with flopster style)
//==============================================================================
class ScratcherVuMeter : public juce::Component
{
public:
    void setLevel(float linearPeak) { level = linearPeak; }
    void setColour(juce::Colour c)  { barColour = c; }
    void paint(juce::Graphics& g) override;

private:
    float        level     = 0.f;
    juce::Colour barColour { 0xff4a6fff };
};

//==============================================================================
// WaveformOverview — mini waveform display for a single deck
//==============================================================================
class WaveformOverview : public juce::Component
{
public:
    void setWaveformPeaks(const std::vector<float>& peaks);
    void setPlayheadNorm(float norm)  { playheadNorm = norm; repaint(); }
    void setLoopBounds(float s, float e) { loopStart = s; loopEnd = e; repaint(); }
    void setTrimBounds(float s, float e) { trimStart = s; trimEnd = e; repaint(); }
    void setColour(juce::Colour c)    { waveColour = c; }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp  (const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    std::function<void(float)>         onSeek;        // normalised seek position
    std::function<void(float, float)>  onTrimChanged; // (trimStart, trimEnd) normalised

private:
    float handleAt(float x) const; // returns 0=none, 1=trimStart, 2=trimEnd, 3=playhead

    std::vector<float> peaks;
    float playheadNorm = 0.f;
    float loopStart    = 0.f;
    float loopEnd      = 1.f;
    float trimStart    = 0.f;
    float trimEnd      = 1.f;
    juce::Colour waveColour { 0xff4a6fff };

    int   draggingHandle = 0;  // 0=none, 1=trimStart, 2=trimEnd
};

//==============================================================================
// VinylComponent — spinning vinyl platter
//==============================================================================
class VinylComponent : public juce::Component, private juce::Timer
{
public:
    VinylComponent();
    ~VinylComponent() override;

    void setDeckColour(juce::Colour c) { deckColour = c; }
    void setSpeed(float s)             { displaySpeed = s; }
    void setPlaying(bool p)            { isPlaying = p; }
    void setHandOnRecord(bool h)       { handOnRecord = h; }
    void addPlayheadDelta(float delta) {
        // Both DeckPanel and VinylComponent timers run on the message thread,
        // so load+store is safe here (no actual data race).
        float prev = pendingHeadDelta.load(std::memory_order_relaxed);
        pendingHeadDelta.store(prev + delta, std::memory_order_relaxed);
    }

    // Mouse-scratch callbacks (set by DeckPanel)
    std::function<void(bool)>         onTouch;  // true=press, false=release
    std::function<void(float, float)> onDrag;   // deltaX, pixelsPerSecond

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp  (const juce::MouseEvent& e) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildVinylImage();

    float        rotationAngle = 0.f;
    float        displaySpeed  = 0.f;
    bool         isPlaying     = false;
    bool         handOnRecord  = false;
    std::atomic<float> pendingHeadDelta { 0.f };
    juce::Colour deckColour    { 0xff4a6fff };
    float        glowIntensity = 0.f;

    bool              isDragging   = false;
    juce::Point<float> lastDragPos;

    // Pre-built (no allocation in paint())
    juce::Image  vinylImage;
    bool         imageBuilt = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VinylComponent)
};

//==============================================================================
// XYPadComponent — scratch (X) + crossfader (Y) interactive pad
//==============================================================================
class XYPadComponent : public juce::Component,
                       private juce::KeyListener,
                       private juce::Timer
{
public:
    XYPadComponent(ScratcherAudioProcessor& proc, ScratcherLAF& laf);
    ~XYPadComponent() override;

    void setMouseModeActive(bool active);
    bool isMouseModeActive() const { return mouseModeActive; }

    void setScratchPos(float x) { scratchPosNorm = x; repaint(); }
    void setCrossfaderPos(float y) { crossfaderNorm = y; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void resized() override;

    // KeyListener for Cmd+R / Ctrl+R exit (juce::KeyListener interface)
    using juce::Component::keyPressed;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    std::function<void(bool)> onMouseModeChanged;
    std::function<void(float dx, float dy)> onScratchMove; // dx = scratch, dy = xfader

private:
    void timerCallback() override;  // plays preset scratch steps
    void triggerPresetScratch();    // called on click without drag

    ScratcherAudioProcessor& proc;

    bool  mouseModeActive = false;
    bool  isDragging      = false;
    float scratchPosNorm  = 0.5f;
    float crossfaderNorm  = 0.5f;
    float scratchVelocity = 0.f;
    float totalDragDist   = 0.f;   // accumulated drag distance to detect click

    juce::Point<float> lastMousePos;

    // Trail for motion feedback
    static constexpr int TRAIL_LEN = 30;
    juce::Point<float>   trail[TRAIL_LEN] = {};
    int                  trailIdx = 0;

    // Preset scratch playback
    int presetType   = 0;   // cycles 0..3 on each click
    int presetStep   = 0;   // current step index
    // Each preset is stored as a flat array of dx values, 20ms per step.
    // Positive = forward, negative = back, 0 = pause.
    static const float PRESET_BABY[];
    static const float PRESET_FORWARD[];
    static const float PRESET_FLARE[];
    static const float PRESET_CRAB[];
    static const int   PRESET_LEN[];
    static const float* PRESETS[];
    static const int   NUM_PRESETS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPadComponent)
};

//==============================================================================
// CrossfaderStrip — horizontal fader with curve controls
//==============================================================================
class CrossfaderStrip : public juce::Component,
                        private juce::Slider::Listener,
                        private juce::Button::Listener
{
public:
    CrossfaderStrip(ScratcherAudioProcessor& proc, ScratcherLAF& laf);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void sliderValueChanged(juce::Slider* s) override;
    void buttonClicked(juce::Button* b) override;
    void drawCurvePreview(juce::Graphics& g, juce::Rectangle<float> area);

    ScratcherAudioProcessor& proc;
    ScratcherLAF& laf;

    juce::Slider xfaderSlider;
    juce::TextButton btnCP, btnSC, btnLin;
    juce::TextButton btnCurveDown, btnCurveUp;
    juce::TextButton btnMidi;
    juce::Label lblCurveValue;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xfaderAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossfaderStrip)
};

//==============================================================================
// DeckPanel — controls for one deck
//==============================================================================
class DeckPanel : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Button::Listener,
                  private juce::Timer
{
public:
    DeckPanel(ScratcherAudioProcessor& proc, ScratcherLAF& laf,
              DeckProcessor& deck, SampleManager::DeckIndex deckIdx,
              juce::Colour colour);
    ~DeckPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void onSampleLoaded();           // called by editor after sample load
    void updateMidiScratchButton();  // update MIDI scratch button text from per-deck CC

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { fileDragOver = true;  repaint(); }
    void fileDragExit(const juce::StringArray&) override            { fileDragOver = false; repaint(); }

private:
    bool fileDragOver = false;

    VinylComponent     vinyl;
    WaveformOverview   waveform;
    ScratcherVuMeter   vu;

private:
    void buttonClicked(juce::Button* b) override;
    void openFileBrowser();

    ScratcherAudioProcessor&   proc;
    ScratcherLAF&              laf;
    DeckProcessor&             deck;
    SampleManager::DeckIndex   deckIdx;
    juce::Colour               deckColour;

    juce::TextButton btnPlay, btnPause, btnStop, btnCue, btnHold;
    juce::TextButton btnLoad;
    juce::TextButton btnLoop;
    juce::TextButton btnNudgeL, btnNudgeR;
    juce::ComboBox   cmbAutoPreset;
    juce::Label      lblSampleName, lblBpm, lblDuration;

    juce::Slider     slVolume, slPitch, slSpeed;
    juce::Label      lblVolume, lblPitch, lblSpeed;

    juce::TextButton btnMidiVol, btnMidiScratch, btnMidiCue;
    juce::TextButton btnSync;
    juce::TextButton btnL1, btnL2, btnL4;   // loop length: 1 bar, 2 bar, full

    int wheelReleaseCountdown = 0;  // timer ticks until motor re-engages
    float prevPlayheadNorm    = 0.f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachVol, attachPitch, attachSpeed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckPanel)
};

//==============================================================================
// ScopeDisplay — real-time oscilloscope
//==============================================================================
class ScopeDisplay : public juce::Component
{
public:
    void updateScope(ScratcherAudioProcessor& proc);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> scopeSamples { 256, 0.f };
    int lastScopePos = 0;
};

//==============================================================================
// GrossBeatPanel — envelope editor + slot buttons
//==============================================================================
class GrossBeatPanel : public juce::Component,
                       private juce::Button::Listener,
                       private juce::Timer
{
public:
    GrossBeatPanel(ScratcherAudioProcessor& proc, ScratcherLAF& laf);
    ~GrossBeatPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    void buttonClicked(juce::Button* b) override;
    void updateEnvEditor();

    ScratcherAudioProcessor& proc;
    ScratcherLAF& laf;

    juce::TextButton   btnTimeEnv, btnVolEnv;
    juce::TextButton   slotBtns[8];
    juce::TextButton   btnEnable;
    juce::TextButton   btnLoop1, btnLoop2, btnLoop4;
    juce::TextButton   btnPreset;

    EnvelopeEditorComponent envEditor;
    bool showingTimeEnv = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrossBeatPanel)
};

//==============================================================================
// SlicerWaveformView — shows both deck waveforms with coloured slice markers
// Displayed in the centre area when the slicer is active.
//==============================================================================
class SlicerWaveformView : public juce::Component, private juce::Timer
{
public:
    SlicerWaveformView(ScratcherAudioProcessor& proc, ScratcherLAF& laf);
    ~SlicerWaveformView() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    void refreshWaveforms();

    void mouseDown      (const juce::MouseEvent& e) override;
    void mouseDrag      (const juce::MouseEvent& e) override;
    void mouseUp        (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    ScratcherAudioProcessor& proc;

    // Slice dragging
    int   draggingDeck  = -1;
    int   draggingSlice = -1;
    float dragStartX    = 0.f;

    // Zoom / scroll (1.0 = full view, scrollOffset in 0..1-1/zoom)
    float zoomLevel    = 1.f;
    float scrollOffset = 0.f;  // leftmost visible position in normalised sample coords

    // Selected slice for envelope editing
    int   selectedDeck  = -1;
    int   selectedSlice = -1;

    // Envelope strip drag (bottom 24px of view)
    // dragEnvType: 0=none, 1=attack, 2=decay
    int   dragEnvType   = 0;
    float dragEnvStartX = 0.f;
    int   dragEnvStartMs = 0;

    // Helpers
    int   getSliceAtX(float x, int deck, float& outNorm) const;
    float normToX(float norm) const;   // apply zoom/scroll → pixel X
    float xToNorm(float x)   const;   // pixel X → normalised sample pos (0..1)

    static constexpr float ENV_STRIP_H = 22.f;  // height of envelope strip at bottom of each row

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlicerWaveformView)
};

//==============================================================================
// SlicePad — one slice pad: trigger on click, drag to adjust start, learn note
//==============================================================================
class SlicePad : public juce::Component
{
public:
    SlicePad() = default;

    // Callbacks set by SlicerPanel
    std::function<void()>  onTrigger;
    std::function<void()>  onLearnNote;

    void setIndex(int i)         { idx = i; repaint(); }
    void setMidiNote(int n)      { midiNote = n; repaint(); }
    void setActive(bool a)       { isActive = a; repaint(); }
    void setLearning(bool l)     { isLearning = l; repaint(); }
    void setNormStart(float n)   { normStart = n; repaint(); } // 0..1 in full sample

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp  (const juce::MouseEvent& e) override;
    void paint(juce::Graphics& g) override;

    static juce::String noteToName(int note);

private:
    int   idx        = 0;
    int   midiNote   = 36;
    bool  isActive   = false;
    bool  isLearning = false;
    float normStart  = 0.f;
    // No JUCE_DECLARE_NON_COPYABLE — stored by value in SlicerPanel's array
};

//==============================================================================
// SlicerPanel — 2×8 pads (Deck A + Deck B) with transient-aware slicing
//==============================================================================
class SlicerPanel : public juce::Component, private juce::Timer
{
public:
    SlicerPanel(ScratcherAudioProcessor& proc, ScratcherLAF& laf);
    ~SlicerPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    // Called from editor when slice data changes
    void refreshPads();

private:
    ScratcherAudioProcessor& proc;
    SlicePad padsA[8];
    SlicePad padsB[8];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlicerPanel)
};

//==============================================================================
// OnboardingOverlay — first-run tutorial
//==============================================================================
class OnboardingOverlay : public juce::Component, private juce::Button::Listener
{
public:
    OnboardingOverlay();
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onDismiss;

private:
    void buttonClicked(juce::Button* b) override;
    int step = 0;
    juce::TextButton btnNext, btnSkip;

    static const int NUM_STEPS = 3;
    static const char* STEPS[NUM_STEPS];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnboardingOverlay)
};

//==============================================================================
// Main editor
//==============================================================================
class ScratcherAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Button::Listener,
      private juce::Timer,
      private juce::KeyListener
{
public:
    ScratcherAudioProcessorEditor(ScratcherAudioProcessor&);
    ~ScratcherAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Called by processor when a sample finishes loading
    void onSampleLoaded(SampleManager::DeckIndex d);

    void updateMidiScratchButton();   // global MIDI scratch button
    void updateMidiScratchButtons();  // per-deck MIDI scratch buttons
    void updateSlicerPads();

private:
    void timerCallback() override;
    void buttonClicked(juce::Button* b) override;
    using juce::AudioProcessorEditor::keyPressed;
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;

    void layoutComponents();
    void applyScaling(int scalePercent);
    void updateVuMeters();
    void checkOnboarding();

    ScratcherAudioProcessor& proc;
    ScratcherLAF             laf;
    ScratcherTheme           theme;

    // Main sections
    DeckPanel        deckPanelA, deckPanelB;
    CrossfaderStrip  crossfader;
    XYPadComponent   xyPad;
    ScopeDisplay     scope;
    GrossBeatPanel   grossBeat;
    SlicerPanel      slicerPanel;
    SlicerWaveformView slicerWaveformView;

    // Top bar
    juce::ComboBox   cmbPreset;
    juce::TextButton btnSavePreset, btnLoadPreset;
    juce::ComboBox   cmbScale;
    juce::TextButton btnMouseMode;
    juce::TextButton btnTap;
    juce::TextButton btnSlicer;
    juce::TextButton btnMidiScratch;
    juce::TextButton btnHelp;

    // Physics knobs
    juce::Slider  knobInertia, knobFriction, knobScratchSens, knobScratchSmooth;
    juce::Label   lblInertia, lblFriction, lblScratchSens, lblScratchSmooth;
    juce::Slider  knobCrackle, knobWarp, knobVinylNoise, knobOutputGain;
    juce::Label   lblCrackle, lblWarp, lblVinylNoise, lblOutputGain;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachInertia, attachFriction, attachScratchSens, attachScratchSmooth,
        attachCrackle, attachWarp, attachVinylNoise, attachOutputGain;

    // FX knobs (reverb / delay / autopan)
    juce::Slider  knobRevSize, knobRevDamp, knobRevWet;
    juce::Label   lblRevSize, lblRevDamp, lblRevWet;
    juce::Slider  knobDelTime, knobDelFb, knobDelWet;
    juce::Label   lblDelTime, lblDelFb, lblDelWet;
    juce::Slider  knobAPanRate, knobAPanDepth;
    juce::Label   lblAPanRate, lblAPanDepth;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachRevSize, attachRevDamp, attachRevWet,
        attachDelTime, attachDelFb, attachDelWet,
        attachAPanRate, attachAPanDepth;

    // MIDI learn indicator labels
    juce::Label  lblMidiStatus;

    // Onboarding
    std::unique_ptr<OnboardingOverlay> onboarding;

    // Scale
    int scalePercent = 100;

    // Base size (before scaling)
    static constexpr int BASE_WIDTH  = 900;
    static constexpr int BASE_HEIGHT = 680;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScratcherAudioProcessorEditor)
};
