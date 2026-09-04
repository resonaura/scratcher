#pragma once
#include <JuceHeader.h>
#include "DeckProcessor.h"
#include "SampleManager.h"
#include "MidiLearnManager.h"
#include "EnvelopeEditor.h"
#include "CrossfaderMath.h"
#include <array>
#include <atomic>

// Forward declaration to avoid circular include
class ScratcherAudioProcessorEditor;

//==============================================================================
class ScratcherAudioProcessor final
    : public juce::AudioProcessor,
      public juce::AudioProcessorValueTreeState::Listener,
      private juce::AsyncUpdater
{
public:
    ScratcherAudioProcessor();
    ~ScratcherAudioProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Scratcher"; }
    bool  acceptsMidi()  const override { return true; }
    bool  producesMidi() const override { return true; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int  getNumPrograms()    override { return 8; }
    int  getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& paramID, float newValue) override;

    //==========================================================================
    // Public subsystems (accessed by editor)
    juce::AudioProcessorValueTreeState apvts;
    DeckProcessor   deckA, deckB;
    SampleManager   sampleMgr;
    MidiLearnManager midiLearn;

    // Gross Beat patterns: 8 time + 8 volume
    std::array<EnvelopePattern, 8> timePatterns;
    std::array<EnvelopePattern, 8> volPatterns;
    std::atomic<int> activeTimeSlot { 0 };
    std::atomic<int> activeVolSlot  { 0 };
    std::atomic<bool> grossBeatEnabled { false };

    // Slicer
    std::atomic<bool> slicerEnabled    { false };
    std::atomic<int>  slicerActiveNote { -1 };   // held MIDI note, or -1

    // Per-slice start positions (sample index) for each deck
    std::array<std::atomic<int>, 8> sliceStartsA;
    std::array<std::atomic<int>, 8> sliceStartsB;

    // Per-slice MIDI note assignments (default A=36..43, B=48..55)
    std::array<std::atomic<int>, 8> sliceNotesA;
    std::array<std::atomic<int>, 8> sliceNotesB;

    // MIDI note learn state for slices
    std::atomic<int> sliceLearningDeck  { -1 };  // 0=A, 1=B, -1=none
    std::atomic<int> sliceLearningIndex { -1 };  // 0-7

    // Called after a sample loads to (re)compute slice start points
    void recomputeSlices(int deck);  // deck: 0=A, 1=B

    // Notify editor that slice data changed
    std::function<void(int deck)> onSlicesRecomputed;

    // MIDI scratch — global (both decks)
    std::atomic<int>  midiScratchCC      { -1 };   // -1=off, 0-127=CC#
    std::atomic<bool> midiScratchLearning { false };

    // Per-deck scratch MIDI CC
    std::atomic<int>  midiScratchCCA      { -1 };
    std::atomic<int>  midiScratchCCB      { -1 };
    std::atomic<bool> midiScratchLearningA { false };
    std::atomic<bool> midiScratchLearningB { false };

    // Per-slice attack/decay (milliseconds, 0..500)
    std::array<std::atomic<int>, 8> sliceAttackMsA, sliceDecayMsA;
    std::array<std::atomic<int>, 8> sliceAttackMsB, sliceDecayMsB;

    // Scope ring buffer (for oscilloscope in editor)
    static constexpr int SCOPE_SIZE = 4096;
    std::atomic<float> scopeBuffer[SCOPE_SIZE];
    std::atomic<int>   scopeWritePos { 0 };

    // BPM info (from host)
    std::atomic<double> hostBpm       { 120.0 };
    std::atomic<double> hostBarPhase  { 0.0 };   // 0..1 position in bar
    std::atomic<bool>   hostIsPlaying { false };

    // Tap tempo
    void tapTempo();
    std::atomic<double> tapBpm { 120.0 };

    // First-run flag (for onboarding overlay)
    std::atomic<bool> isFirstRun { true };

    // ── Vinyl ambient noise player ─────────────────────────────────────────────
    struct VinylNoisePlayer {
        juce::AudioBuffer<float> sampleData;
        int    totalSamples     = 0;
        int    loopStart        = 0;
        int    loopEnd          = 0;
        int    xfadeSamples     = 0;
        double readPos          = 0.0;
        double nativeSampleRate = 48000.0;
        std::atomic<float> volume { 0.0f };
        bool   loaded           = false;

        void loadAndPrepare(double playbackSampleRate);
        void process(float* outL, float* outR, int numSamples, double playbackSampleRate);
    };
    VinylNoisePlayer vinylNoise;

private:
    //==========================================================================
    int currentProgram = 0;
    double sampleRate  = 44100.0;

    // ── Master effects ─────────────────────────────────────────────────────────
    juce::Reverb reverb;

    static constexpr int MAX_DELAY_SAMPLES = 192001;
    std::vector<float> delayBufL, delayBufR;
    int delayWritePos = 0;

    double autopanPhase = 0.0;  // LFO phase for auto-pan (0..2π)

    // Raw parameter pointers (lock-free reads on audio thread)
    std::atomic<float>* rawCrossfader      = nullptr;
    std::atomic<float>* rawXfaderCurve     = nullptr;
std::atomic<float>* rawOutputGain      = nullptr;
    std::atomic<float>* rawCrackle         = nullptr;
    std::atomic<float>* rawWarp            = nullptr;
    std::atomic<float>* rawScratchSens     = nullptr;
    std::atomic<float>* rawScratchSmooth   = nullptr;
    std::atomic<float>* rawInertia         = nullptr;
    std::atomic<float>* rawFriction        = nullptr;
    std::atomic<float>* rawAVolume         = nullptr;
    std::atomic<float>* rawBVolume         = nullptr;
    std::atomic<float>* rawAPitch          = nullptr;
    std::atomic<float>* rawBPitch          = nullptr;
    std::atomic<float>* rawASpeed          = nullptr;
    std::atomic<float>* rawBSpeed          = nullptr;
    std::atomic<float>* rawATrimStart      = nullptr;
    std::atomic<float>* rawATrimEnd        = nullptr;
    std::atomic<float>* rawBTrimStart      = nullptr;
    std::atomic<float>* rawBTrimEnd        = nullptr;
    std::atomic<float>* rawReverbSize      = nullptr;
    std::atomic<float>* rawReverbDamp      = nullptr;
    std::atomic<float>* rawReverbWet       = nullptr;
    std::atomic<float>* rawDelayTime       = nullptr;
    std::atomic<float>* rawDelayFeedback   = nullptr;
    std::atomic<float>* rawDelayWet        = nullptr;
    std::atomic<float>* rawAutopanRate     = nullptr;
    std::atomic<float>* rawAutopanDepth    = nullptr;
    std::atomic<float>* rawVinylNoiseVol   = nullptr;

    // MIDI scratch: last CC value for delta (relative) mode — audio thread only
    int   lastScratchCCValueA = -1;
    int   lastScratchCCValueB = -1;
    float scrSpeedA = 0.f;   // persistent per-block scratch speed (decays each block)
    float scrSpeedB = 0.f;

    // Tap tempo
    juce::int64 lastTapTime = 0;
    std::vector<double> tapIntervals;

    // AsyncUpdater: load default samples on first run
    void handleAsyncUpdate() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScratcherAudioProcessor)
};
