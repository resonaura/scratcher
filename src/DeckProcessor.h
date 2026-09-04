#pragma once
#include <JuceHeader.h>
#include "CircularBuffer.h"
#include <atomic>

//==============================================================================
// DeckProcessor — one vinyl deck with physics simulation + DSP.
//
// Threading:
//   • processBlock() runs on the audio thread.
//   • applyMouseDelta() / touchRecord() are called from the GUI/message thread
//     but only write to atomic variables (no blocking).
//   • loadSample() must be called from a non-audio thread.
//==============================================================================
class DeckProcessor
{
public:
    // ── Playback states ──────────────────────────────────────────────────────
    enum class PlayState { Stopped, Playing, Paused, Cueing, Hold };

    DeckProcessor();
    ~DeckProcessor() = default;

    // Must be called from prepareToPlay() (non-real-time thread is fine).
    void prepare(double sampleRate, int blockSize);

    // Called from the audio thread once per block.
    // effectMode: if true, inputL/R are written to the circular buffer first,
    //             then read back with time displacement.
    // Returns stereo output pair in outL / outR (numSamples each).
    void processBlock(const float* inputL, const float* inputR,
                      float* outL, float* outR, int numSamples,
                      bool effectMode,
                      float timeEnvValue,  // 0=now, 1=8-beats-ago (Gross Beat)
                      float volEnvValue);  // 0..1 volume envelope

    // ── GUI → DSP communication (all atomic, no locks) ───────────────────────

    // Call when user presses / releases the mouse over the vinyl platter.
    void touchRecord(bool isDown);

    // Call on every mouse drag event (GUI thread).
    // deltaX: pixels moved since last call (positive=right, negative=left).
    // pixelsPerSecond: calibration constant (how many px = 1 s of normal audio).
    void applyMouseDelta(float deltaX, float pixelsPerSecond);

    // Playback control (may be called from any thread).
    void play();
    void pause();
    void stop();
    void cue();   // return to cue point and pause
    void hold();  // scratch-only: motor off unless hand on record
    void nudge(float semitones); // temporary pitch bend (0.0 = off)

    // ── Sample management (call from non-audio thread) ────────────────────────
    void loadFromBuffer(const juce::AudioBuffer<float>& buf,
                        double sourceSampleRate,
                        const juce::String& name);

    void setLoop(bool enabled, int startSample = 0, int endSample = -1);
    void setCuePoint(int samplePosition);

    // Trim controls — restrict playable region of the sample
    void setTrimStart(int pos) { atomTrimStart.store(std::max(0, pos), std::memory_order_relaxed); }
    void setTrimEnd(int pos)   { atomTrimEnd.store(std::min(pos, totalSamples), std::memory_order_relaxed); }
    int  getTrimStart() const  { return atomTrimStart.load(std::memory_order_relaxed); }
    int  getTrimEnd()   const  {
        int v = atomTrimEnd.load(std::memory_order_relaxed);
        return (v <= 0) ? totalSamples : v;
    }
    float getTrimStartNorm() const {
        return (totalSamples > 0) ? (float)atomTrimStart.load() / totalSamples : 0.f;
    }
    float getTrimEndNorm() const {
        int v = atomTrimEnd.load();
        return (totalSamples > 0) ? (float)(v <= 0 ? totalSamples : v) / totalSamples : 1.f;
    }

    // ── Parameters (written from GUI thread, read on audio thread via atomics) ─
    void setInertia(float v)         { atomInertia.store(v); }
    void setFriction(float v)        { atomFriction.store(v); }
    void setSensitivity(float v)     { atomSensitivity.store(v); }
    void setSmoothing(float v)       { atomSmoothing.store(v); }
    void setVolume(float v)          { atomVolume.store(v); }
    void setSpeed(float v)           { atomSpeed.store(std::max(0.01f, v)); }
    void setPitch(float semitones)   { atomPitch.store(semitones); }
    void setVinylCrackle(float v)    { atomCrackle.store(v); }
    void setVinylWarp(float v)       { atomWarp.store(v); }

    void seekToSample(int pos);                  // jump to position, keep playing
    void setExternalScratchSpeed(float speed);   // 0 = release (motor takes over)

    // Tape-effect transport: gradual speed ramp on play/stop
    void playTape();   // play with speed ramp-up (~300 ms)
    void stopTape();   // stop with speed ramp-down (~500 ms)

    // Per-slice fade: call before seekToSample/setLoop for the upcoming slice.
    // Values in milliseconds — converted to samples using the stored sample rate.
    void setSliceAttackMs(int ms) {
        int s = std::max(0, (int)(ms * sampleRate / 1000.0));
        atomSliceAttackSamples.store(s, std::memory_order_relaxed);
    }
    void setSliceDecayMs(int ms) {
        int s = std::max(0, (int)(ms * sampleRate / 1000.0));
        atomSliceDecaySamples.store(s, std::memory_order_relaxed);
    }

    // ── State queries (thread-safe) ───────────────────────────────────────────
    PlayState    getPlayState()   const { return playState.load(); }
    float        getVuLevel()     const { return atomVU.load(); }
    float        getPlayheadNorm() const;  // 0..1 position in sample
    int          getPlayheadSample() const { return atomPlayhead.load(std::memory_order_relaxed); }
    int          getTotalSamples() const { return totalSamples; }
    double       getDetectedBpm()  const { return detectedBpm; }
    juce::String getSampleName()   const { return sampleName; }
    float        getCurrentSpeed() const { return static_cast<float>(smoothedSpeed.load()); }
    double       getSampleRate()   const { return sampleRate; }

    // Loop state queries
    int  getLoopStartSample() const { return atomLoopStart.load(std::memory_order_relaxed); }
    int  getLoopEndSample()   const { return atomLoopEnd.load(std::memory_order_relaxed); }
    bool isLoopEnabled()      const { return atomLoopEnabled.load(std::memory_order_relaxed); }

    // Returns true only when a real (non-default) sample is loaded
    bool hasSampleLoaded() const { return totalSamples > 0; }

    // Waveform peaks for the overview display (computed on load).
    const std::vector<float>& getWaveformPeaks() const { return waveformPeaks; }

    // Generate a built-in scratch-friendly demo sample and auto-load it.
    void generateDefaultSample();

    // Returns up to numSlices transient-aligned slice positions (sample indices).
    // Uses onset detection + beat-grid quantization + beat offset detection.
    std::vector<int> computeTransientSlices(int numSlices = 8) const;

    // ── BPM sync ──────────────────────────────────────────────────────────────
    void setLoopLengthByBars(double numBars, double hostBpm);

    // ── Scope / ring buffer for GUI oscilloscope ──────────────────────────────
    static constexpr int SCOPE_SIZE = 4096;
    std::atomic<float> scopeBuffer[SCOPE_SIZE];
    std::atomic<int>   scopeWritePos { 0 };

private:
    // ── Audio thread state (no atomics needed — only touched by audio thread) ──
    double sampleRate      = 44100.0;
    double angularVelocity = 0.0;   // current playback speed (1.0 = normal)
    double readPosition    = 0.0;   // fractional sample index
    double warpPhase       = 0.0;   // for vinyl warp LFO
    double crackleState    = 0.0;   // LFSR state for crackle noise

    // Fade-in after seekToSample() to avoid clicks
    int fadeInRemaining = 0;
    static constexpr int FADE_IN_SAMPLES = 256; // ~6 ms at 44.1 kHz

    // Safety: number of samples written to circular buffer since prepare()
    int samplesWritten = 0;

    // 4th-order Butterworth anti-alias filter (two biquads per channel)
    // Coefficients updated when speed changes; state reset on large speed jumps.
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;

        // fcNorm = fc / Nyquist  (range 0..1).  1.0 = bypass.
        void setLowpass(float fcNorm) noexcept;
        void reset() noexcept { z1 = z2 = 0.f; }
        float process(float x) noexcept
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad bqL1, bqL2, bqR1, bqR2;
    double lastLpfSpeed = -99.0;  // sentinel so coeffs are set on first block

    // VU peak hold
    float vuPeakDecay = 0.f;

    // Cue + loop points — atomic so GUI and audio thread can both access safely
    std::atomic<int>  atomCuePoint   { 0 };
    std::atomic<int>  atomLoopStart  { 0 };
    std::atomic<int>  atomLoopEnd    { 0 };
    std::atomic<bool> atomLoopEnabled { true };

    // Trim region (0 = no trim for atomTrimEnd, meaning use totalSamples)
    std::atomic<int> atomTrimStart { 0 };
    std::atomic<int> atomTrimEnd   { 0 };

    // True while the built-in demo sample is loaded (no real file loaded yet)
    std::atomic<bool> atomDefaultSample { true };

    // Coast on hand release for more vinyl-like feel
    int handReleaseCoastBlocks = 0;
    static constexpr int HAND_COAST_BLOCKS = 3;  // ~15 ms coast at 512 samples/block
    bool prevHandState = false;

    // Per-slice attack/decay fade (set before seekToSample + setLoop)
    std::atomic<int> atomSliceAttackSamples { 256 };
    std::atomic<int> atomSliceDecaySamples  { 0 };
    int fadeInTotal = FADE_IN_SAMPLES;  // denominator for fade-in ramp

    // Tape transport effects (GUI→audio via atomics, state on audio thread)
    std::atomic<bool> atomTapeStopReq  { false };
    std::atomic<bool> atomTapeStartReq { false };
    bool  tapeStopActive  = false;
    float tapeStopPhase   = 0.f;
    float tapeStopInitSpd = 0.f;
    bool  tapeStartActive = false;
    float tapeStartPhase  = 0.f;

    // Loaded sample data (protected by samplesLock)
    juce::AudioBuffer<float> sampleData;
    double sampleSampleRate = 44100.0;
    int    totalSamples     = 0;
    juce::String sampleName;
    double detectedBpm = 0.0;
    std::vector<float> waveformPeaks;

    juce::CriticalSection samplesLock;

    // Circular buffer for effect mode
    CircularBuffer circBuf;

    // ── Atomic parameters (GUI → audio thread) ───────────────────────────────
    std::atomic<float>  atomInertia    { 0.12f };
    std::atomic<float>  atomFriction   { 0.05f };
    std::atomic<float>  atomSensitivity{ 1.0f };
    std::atomic<float>  atomSmoothing  { 0.92f };
    std::atomic<float>  atomVolume     { 1.0f };
    std::atomic<float>  atomSpeed      { 1.0f };   // playback speed multiplier (0.25..4)
    std::atomic<float>  atomPitch      { 0.0f };   // semitones
    std::atomic<float>  atomCrackle    { 0.0f };
    std::atomic<float>  atomWarp       { 0.0f };
    std::atomic<float>  atomNudge      { 0.0f };   // extra semitone offset
    std::atomic<float>  atomExternalScratch { 0.f }; // MIDI scratch override speed

    std::atomic<double> smoothedSpeed  { 1.0 };    // smoothed playback speed
    std::atomic<double> pendingDelta   { 0.0 };    // mouse delta accumulator
    std::atomic<bool>   handOnRecord   { false };
    std::atomic<bool>   newDeltaReady  { false };

    std::atomic<PlayState> playState   { PlayState::Stopped };
    std::atomic<int>       atomPlayhead { 0 };     // integer sample index
    std::atomic<float>     atomVU      { 0.f };

    // ── DSP helpers (audio thread only) ──────────────────────────────────────
    float readHermiteL(double pos) const noexcept;
    float readHermiteR(double pos) const noexcept;
    float readCircHermiteL(int baseOffset, float frac) const noexcept;
    float readCircHermiteR(int baseOffset, float frac) const noexcept;

    void updatePhysics(double dt) noexcept;
    void updateAntiAliasFilter(double speed) noexcept;

    float generateCrackle() noexcept;
    float generateWarp()    noexcept;

    void computeWaveformPeaks(int numPoints = 512);
    double detectBpm() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckProcessor)
};
