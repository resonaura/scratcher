#include "DeckProcessor.h"
#include "HermiteInterp.h"
#include <cmath>
#include <algorithm>

//==============================================================================
DeckProcessor::DeckProcessor()
{
    for (auto& s : scopeBuffer) s.store(0.f);
}

void DeckProcessor::prepare(double sr, int /*blockSize*/)
{
    sampleRate = sr;

    // Size the circular buffer to exactly 8 bars at 120 BPM as a safe default.
    // The buffer will still hold any reasonable BPM because MAX_SIZE > 5 s.
    int defaultSize = static_cast<int>(sr * 8.0 * (60.0 / 120.0) * 4.0);
    circBuf.resize(defaultSize);

    angularVelocity = 0.0;
    readPosition    = 0.0;
    warpPhase       = 0.0;
    samplesWritten  = 0;
    vuPeakDecay     = 0.f;
    lastLpfSpeed    = -99.0;
    bqL1.reset(); bqL2.reset();
    bqR1.reset(); bqR2.reset();
    playState.store(PlayState::Stopped);
}

//==============================================================================
// Audio thread — main processing
//==============================================================================
void DeckProcessor::processBlock(const float* inputL, const float* inputR,
                                 float* outL, float* outR, int numSamples,
                                 bool effectMode,
                                 float timeEnvValue,
                                 float volEnvValue)
{
    // Local copies of atomic parameters (read once per block)
    const float  inertia     = atomInertia.load(std::memory_order_relaxed);
    const float  friction    = atomFriction.load(std::memory_order_relaxed);
    const float  sensitivity = atomSensitivity.load(std::memory_order_relaxed);
    const float  smoothCoeff = atomSmoothing.load(std::memory_order_relaxed);
    const float  volume      = atomVolume.load(std::memory_order_relaxed);
    const float  pitchSemi   = atomPitch.load(std::memory_order_relaxed);
    const float  nudgeSemi   = atomNudge.load(std::memory_order_relaxed);
    const float  crackleAmt  = atomCrackle.load(std::memory_order_relaxed);
    const float  warpAmt     = atomWarp.load(std::memory_order_relaxed);
    const bool   hand         = handOnRecord.load(std::memory_order_relaxed);
    const auto   state        = playState.load(std::memory_order_relaxed);
    const float  extScratch   = atomExternalScratch.load(std::memory_order_relaxed);
    const bool   hasExtScratch = (std::abs(extScratch) > 0.001f);
    const int    decaySamples  = atomSliceDecaySamples.load(std::memory_order_relaxed);

    // Absorb pending mouse delta (GUI thread wrote this)
    double delta    = 0.0;
    bool   hasDelta = false;
    if (newDeltaReady.exchange(false, std::memory_order_acq_rel))
    {
        delta    = pendingDelta.exchange(0.0, std::memory_order_relaxed);
        hasDelta = (std::abs(delta) > 1e-9);
    }

    // Pitch multiplier from semitones
    const double pitchMult = std::pow(2.0, (pitchSemi + nudgeSemi) / 12.0);

    float peakThisBlock = 0.f;

    // ── Scratch physics: apply ONCE per block, not per sample ─────────────────
    // Per-sample smoothing would give 0.82^512 ≈ 0 decay = silence on mouse pause.
    if (state == PlayState::Playing || state == PlayState::Cueing
        || state == PlayState::Hold)
    {
        // Track hand state transitions for coast
        if (prevHandState && !hand)
            handReleaseCoastBlocks = HAND_COAST_BLOCKS;
        prevHandState = hand;

        if (hand)
        {
            if (hasDelta)
            {
                double pixPerSec = delta * sampleRate / (double)numSamples;
                // Normalised velocity: 1.0 = 500 px/s = 1× playback speed
                double normV = pixPerSec / 500.0;
                // Non-linear dynamics: gentle touch → soft scratch, hard spin → strong.
                // Power 1.6 gives sub-linear at low speed, super-linear at high speed —
                // matches real vinyl inertia feel (kinetic energy ∝ v²).
                double absNV  = std::abs(normV);
                double dynV   = std::copysign(absNV * absNV / (absNV + 0.35) * 1.35, normV);
                double rawSpeed = dynV * (double)sensitivity * pitchMult;
                // When changing direction use low smooth so we don't dwell at zero.
                bool dirChange = (rawSpeed * angularVelocity < -0.01);
                double smooth  = dirChange ? 0.10 : (double)smoothCoeff;
                angularVelocity = angularVelocity * smooth + rawSpeed * (1.0 - smooth);
                angularVelocity = std::clamp(angularVelocity, -6.0, 6.0);
            }
            else
            {
                // Hand on record but not moving: clamp to 0 fast (~10 ms).
                // Simulates holding the vinyl still.
                angularVelocity *= 0.08;
                if (std::abs(angularVelocity) < 1e-4) angularVelocity = 0.0;
            }
            smoothedSpeed.store(angularVelocity, std::memory_order_relaxed);
        }
    }

    // Decrement coast counter once per block
    if (handReleaseCoastBlocks > 0)
        --handReleaseCoastBlocks;

    // ── Tape start request (GUI thread sets, audio thread picks up) ───────────
    if (atomTapeStartReq.exchange(false, std::memory_order_acq_rel))
    {
        tapeStartActive = true;
        tapeStartPhase  = 0.f;
    }

    // ── Tape stop request ─────────────────────────────────────────────────────
    if (atomTapeStopReq.exchange(false, std::memory_order_acq_rel))
    {
        tapeStopActive  = true;
        tapeStopPhase   = 0.f;
        tapeStopInitSpd = std::max(0.01f, (float)std::abs(angularVelocity));
    }

    // ── Tape stop: override motor with ramp-down ──────────────────────────────
    if (tapeStopActive)
    {
        float blocksFor = std::max(1.f, 0.55f * (float)sampleRate / (float)numSamples);
        tapeStopPhase  += 1.f / blocksFor;
        if (tapeStopPhase >= 1.f)
        {
            tapeStopActive  = false;
            angularVelocity = 0.0;
            smoothedSpeed.store(0.0, std::memory_order_relaxed);
            playState.store(PlayState::Stopped, std::memory_order_release);
            int lStart = atomLoopStart.load(std::memory_order_relaxed);
            readPosition = lStart;
            atomPlayhead.store(lStart);
        }
        else
        {
            float env = 1.f - tapeStopPhase * tapeStopPhase;  // quadratic ease-out
            angularVelocity = (double)tapeStopInitSpd * env;
            smoothedSpeed.store(angularVelocity, std::memory_order_relaxed);
        }
    }

    // ── External (MIDI) scratch override ─────────────────────────────────────
    if (hasExtScratch && (state == PlayState::Playing || state == PlayState::Cueing
                          || state == PlayState::Hold))
    {
        // Apply sensitivity to MIDI/external scratch the same way as mouse scratch
        double absExt  = std::abs((double)extScratch);
        double dynExt  = std::copysign(absExt * absExt / (absExt + 0.35) * 1.35, (double)extScratch);
        double scaledExt = dynExt * (double)sensitivity;
        // Smooth toward target speed to avoid clicks
        angularVelocity += (scaledExt - angularVelocity) * 0.35;
        smoothedSpeed.store(angularVelocity, std::memory_order_relaxed);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // ── 1. Write input to circular buffer (effect mode) ────────────────
        if (effectMode && inputL && inputR)
        {
            circBuf.write(inputL[i], inputR[i]);
            samplesWritten = std::min(samplesWritten + 1, circBuf.getActualSize());
        }

        float sampleL = 0.f, sampleR = 0.f;

        if (state == PlayState::Playing || state == PlayState::Cueing
            || state == PlayState::Hold)
        {
            // ── 2. Motor physics (per-sample for smooth spin-up/down) ──────
            if (!hand && !hasExtScratch)
            {
                if (handReleaseCoastBlocks > 0)
                {
                    // Brief coast: preserve velocity from scratch release
                    // (decremented once per block in the scratch-physics section)
                    angularVelocity *= 0.98;
                    smoothedSpeed.store(angularVelocity, std::memory_order_relaxed);
                }
                else if (!tapeStopActive)
                {
                    double speedMult = (double)atomSpeed.load(std::memory_order_relaxed);
                    double targetV   = (state == PlayState::Playing) ? pitchMult * speedMult : 0.0;

                    // Tape start: ramp target velocity from 0 over ~300 ms
                    if (tapeStartActive)
                    {
                        float blocksFor = std::max(1.f, 0.30f * (float)sampleRate / (float)numSamples);
                        tapeStartPhase  = std::min(1.f, tapeStartPhase + 1.f / blocksFor);
                        float ramp      = tapeStartPhase * tapeStartPhase;  // quadratic ease-in
                        targetV        *= ramp;
                        if (tapeStartPhase >= 1.f) tapeStartActive = false;
                    }

                    double torque   = (double)friction * (targetV - angularVelocity);
                    angularVelocity += torque / (double)inertia / 5000.0;
                    angularVelocity = std::clamp(angularVelocity, -4.0, 4.0);
                    smoothedSpeed.store(angularVelocity, std::memory_order_relaxed);
                }
            }

            // ── 3. Update anti-alias filter coefficients (once per speed change) ─
            updateAntiAliasFilter(angularVelocity);

            // ── 4. Read audio via Hermite interpolation ────────────────────
            if (effectMode)
            {
                double timeOffsetBeats = timeEnvValue * 8.0;
                int offset = static_cast<int>(timeOffsetBeats * sampleRate * 0.5);
                offset = std::min(offset, samplesWritten - 6);
                if (offset < 0) offset = 0;

                float frac = 0.f;
                sampleL = readCircHermiteL(offset, frac);
                sampleR = readCircHermiteR(offset, frac);

                // 4th-order Butterworth anti-alias (2× cascaded biquad)
                sampleL = bqL2.process(bqL1.process(sampleL));
                sampleR = bqR2.process(bqR1.process(sampleR));
            }
            else
            {
                // Instrument mode: read from loaded sample buffer
                const juce::ScopedTryLock stl(samplesLock);
                if (stl.isLocked() && totalSamples > 0)
                {
                    sampleL = readHermiteL(readPosition);
                    sampleR = readHermiteR(readPosition);

                    // 4th-order Butterworth anti-alias (2× cascaded biquad)
                    sampleL = bqL2.process(bqL1.process(sampleL));
                    sampleR = bqR2.process(bqR1.process(sampleR));

                    // Advance read position — apply vinyl warp as pitch modulation if enabled
                    double warpMod = 1.0;
                    if (warpAmt > 0.001f)
                        warpMod = 1.0 + (double)(generateWarp() * warpAmt * 0.03f);
                    readPosition += angularVelocity * warpMod;

                    // Loop / boundary handling — use atomics, apply trim constraints
                    int trimStartLocal = atomTrimStart.load(std::memory_order_relaxed);
                    int trimEndLocal   = atomTrimEnd.load(std::memory_order_relaxed);
                    if (trimEndLocal <= 0 || trimEndLocal > totalSamples)
                        trimEndLocal = totalSamples;

                    int end   = atomLoopEnd.load(std::memory_order_relaxed);
                    int start = atomLoopStart.load(std::memory_order_relaxed);
                    bool loopEnabledLocal = atomLoopEnabled.load(std::memory_order_relaxed);

                    // Apply trim constraints
                    end   = std::min(end,   trimEndLocal);
                    start = std::max(start, trimStartLocal);
                    end   = (end <= 0 || end > totalSamples) ? trimEndLocal : end;
                    start = std::max(0, std::min(start, end - 1));

                    if (readPosition >= end)
                    {
                        if (loopEnabledLocal) {
                            if (end > start)
                                readPosition = start + std::fmod(readPosition - end, (double)(end - start));
                            else
                                readPosition = start;
                        }
                        else { readPosition = start; playState.store(PlayState::Stopped); }
                    }
                    else if (readPosition < start)
                    {
                        if (loopEnabledLocal) {
                            if (end > start)
                                readPosition = end - std::fmod(start - readPosition, (double)(end - start));
                            else
                                readPosition = start;
                        }
                        else readPosition = start;
                    }

                    // ── Slice decay fade-out (near loop end) ──────────────
                    if (decaySamples > 0 && loopEnabledLocal)
                    {
                        int distToEnd = std::max(0, end - static_cast<int>(readPosition));
                        if (distToEnd < decaySamples)
                        {
                            float fadeGain = (float)distToEnd / (float)decaySamples;
                            sampleL *= fadeGain;
                            sampleR *= fadeGain;
                        }
                    }

                    atomPlayhead.store(static_cast<int>(readPosition));
                }
            }

            // ── 5. Vinyl Crackle (additive noise) ─────────────────────────
            if (crackleAmt > 0.001f)
            {
                float crackle = generateCrackle() * crackleAmt * 0.02f;
                sampleL += crackle;
                sampleR += crackle;
            }
        }

        // ── 6. Seek fade-in (prevents clicks after seekToSample) ──────────
        if (fadeInRemaining > 0)
        {
            float gain = (fadeInTotal > 0)
                ? 1.f - (float)fadeInRemaining / (float)fadeInTotal
                : 1.f;
            sampleL *= gain;
            sampleR *= gain;
            --fadeInRemaining;
        }

        // ── 7. Volume envelope (Gross Beat) + deck volume ─────────────────
        sampleL *= volEnvValue * volume;
        sampleR *= volEnvValue * volume;

        outL[i] = sampleL;
        outR[i] = sampleR;

        // VU
        float absL = std::abs(sampleL);
        float absR = std::abs(sampleR);
        if (absL > peakThisBlock) peakThisBlock = absL;
        if (absR > peakThisBlock) peakThisBlock = absR;

        // Scope
        int sp = scopeWritePos.load(std::memory_order_relaxed);
        scopeBuffer[sp & (SCOPE_SIZE - 1)].store((sampleL + sampleR) * 0.5f);
        scopeWritePos.store(sp + 1, std::memory_order_relaxed);
    }

    // Decay VU peak
    float vu = atomVU.load(std::memory_order_relaxed);
    vu = std::max(vu * 0.995f, peakThisBlock);
    atomVU.store(vu, std::memory_order_relaxed);
}

//==============================================================================
// Lanczos-8 windowed sinc interpolation (16 taps) with anti-aliasing.
//
// When speed > 1×, the kernel is compressed by factor 1/speed so that the
// effective cutoff is Nyquist/speed — identical to what a perfect brick-wall
// LPF would do before downsampling.  A normalised sum preserves DC gain.
//
// Aliasing floor ≈ –80 dB (vs –43 dB for 6-point Hermite).
//==============================================================================
static constexpr int SINC_LOBES = 8;   // 8 lobes → 16 taps total

// sinc(x) = sin(πx)/(πx),  sinc(0) = 1
static inline float sincf_l(float x) noexcept
{
    if (x > -1e-6f && x < 1e-6f) return 1.f;
    const float px = 3.14159265f * x;
    return std::sin(px) / px;
}

// Shared wrapIdx logic (template on data pointer avoids lambda capture overhead)
static inline int wrapSample(int idx, bool loopEnabled, int loopStart,
                              int loopEnd, int totalSamples) noexcept
{
    if (loopEnabled)
    {
        const int end   = (loopEnd > 0 ? loopEnd : totalSamples);
        const int range = end - loopStart;
        if (range <= 0) return loopStart;
        idx = loopStart + ((idx - loopStart) % range + range) % range;
    }
    else idx = std::clamp(idx, 0, totalSamples - 1);
    return idx;
}

void DeckProcessor::seekToSample(int pos)
{
    pos = std::clamp(pos, 0, std::max(0, totalSamples - 1));
    readPosition    = static_cast<double>(pos);
    fadeInTotal     = atomSliceAttackSamples.load(std::memory_order_relaxed);
    fadeInRemaining = fadeInTotal;
    atomPlayhead.store(pos);
}

void DeckProcessor::setExternalScratchSpeed(float speed)
{
    atomExternalScratch.store(speed, std::memory_order_relaxed);
}

static float readSincChannel(const float* data, double pos, double speed,
                              bool loopEnabled, int loopStart,
                              int loopEnd, int totalSamples) noexcept
{
    const int i0   = static_cast<int>(pos);
    const float fr = static_cast<float>(pos - i0);

    // At speed > 1× compress the kernel to act as a LPF at Nyquist/speed.
    // At speed ≤ 1× (or reverse) no anti-aliasing stretch is needed.
    const float scale = static_cast<float>(
        std::min(1.0, 1.0 / std::max(0.01, std::abs(speed))));

    float result = 0.f, weightSum = 0.f;

    for (int k = 1 - SINC_LOBES; k <= SINC_LOBES; ++k)
    {
        const float x = scale * ((float)k - fr);

        // Lanczos window is zero outside [−N, N]
        if (x <= -(float)SINC_LOBES || x >= (float)SINC_LOBES)
            continue;

        // w = sinc(x) * sinc(x / N)   — product of main sinc and Lanczos window
        const float w = sincf_l(x) * sincf_l(x / (float)SINC_LOBES);

        const int idx = wrapSample(i0 + k, loopEnabled, loopStart, loopEnd, totalSamples);
        result    += data[idx] * w;
        weightSum += w;
    }

    // Normalise so DC gain = 1 regardless of scale or fractional position.
    return (weightSum > 1e-10f) ? result / weightSum : 0.f;
}

float DeckProcessor::readHermiteL(double pos) const noexcept
{
    if (totalSamples < SINC_LOBES * 2) return 0.f;
    return readSincChannel(sampleData.getReadPointer(0), pos, angularVelocity,
                           atomLoopEnabled.load(std::memory_order_relaxed),
                           atomLoopStart.load(std::memory_order_relaxed),
                           atomLoopEnd.load(std::memory_order_relaxed),
                           totalSamples);
}

float DeckProcessor::readHermiteR(double pos) const noexcept
{
    if (totalSamples < SINC_LOBES * 2) return 0.f;
    const int ch = sampleData.getNumChannels() > 1 ? 1 : 0;
    return readSincChannel(sampleData.getReadPointer(ch), pos, angularVelocity,
                           atomLoopEnabled.load(std::memory_order_relaxed),
                           atomLoopStart.load(std::memory_order_relaxed),
                           atomLoopEnd.load(std::memory_order_relaxed),
                           totalSamples);
}

float DeckProcessor::readCircHermiteL(int baseOffset, float /*frac*/) const noexcept
{
    // Six taps from circular buffer
    float y[6];
    for (int k = 0; k < 6; ++k)
        y[k] = circBuf.readL(baseOffset + 3 - k);  // y[-2..3]
    return Hermite::interpolate(y[0], y[1], y[2], y[3], y[4], y[5], 0.f);
}

float DeckProcessor::readCircHermiteR(int baseOffset, float /*frac*/) const noexcept
{
    float y[6];
    for (int k = 0; k < 6; ++k)
        y[k] = circBuf.readR(baseOffset + 3 - k);
    return Hermite::interpolate(y[0], y[1], y[2], y[3], y[4], y[5], 0.f);
}

//==============================================================================
// Physics helpers
//==============================================================================
void DeckProcessor::updatePhysics(double /*dt*/) noexcept
{
    // Physics is integrated per-sample inside processBlock.
}

//==============================================================================
// 4th-order Butterworth anti-alias filter
//==============================================================================
void DeckProcessor::Biquad::setLowpass(float fcNorm) noexcept
{
    // fcNorm = fc / Nyquist → fc/sr = fcNorm/2 → ω₀ = π·fcNorm
    // Audio EQ Cookbook (RBJ) Butterworth LPF with Q = 1/√2
    if (fcNorm >= 1.f)
    {
        b0 = 1.f; b1 = 0.f; b2 = 0.f; a1 = 0.f; a2 = 0.f;
        return;
    }
    fcNorm = std::max(fcNorm, 0.002f);
    const float w0    = 3.14159265f * fcNorm;   // = 2π·fc/sr
    const float cosw  = std::cos(w0);
    const float sinw  = std::sin(w0);
    const float alpha = sinw * 0.70710678f;      // sinw / (2·Q), Q=1/√2 (Butterworth)
    const float a0inv = 1.f / (1.f + alpha);
    b0 = (1.f - cosw) * 0.5f * a0inv;
    b1 = (1.f - cosw)         * a0inv;
    b2 = b0;
    a1 = -2.f * cosw           * a0inv;
    a2 = (1.f - alpha)         * a0inv;
}

void DeckProcessor::updateAntiAliasFilter(double speed) noexcept
{
    // Only recompute when speed changes significantly (trig is expensive).
    double diff = speed - lastLpfSpeed;
    if (std::abs(diff) < 0.02) return;

    // Reset filter state on large jumps to avoid transients.
    if (std::abs(diff) > 1.0)
    {
        bqL1.reset(); bqL2.reset();
        bqR1.reset(); bqR2.reset();
    }

    lastLpfSpeed = speed;

    double absSpd = std::abs(speed);
    // At speed ≤ 1× no aliasing → bypass (fcNorm = 1).
    // At speed > 1× cut at Nyquist / speed.  Floor 0.10 prevents over-darkening.
    float fcNorm = (absSpd <= 1.0)
                   ? 1.f
                   : static_cast<float>(std::max(0.10, 1.0 / absSpd));

    bqL1.setLowpass(fcNorm); bqL2.setLowpass(fcNorm);
    bqR1.setLowpass(fcNorm); bqR2.setLowpass(fcNorm);
}

float DeckProcessor::generateCrackle() noexcept
{
    // Simple Galois LFSR
    uint32_t lfsr = *reinterpret_cast<uint32_t*>(&crackleState);
    if (lfsr == 0) lfsr = 0xACE1u;
    bool bit = lfsr & 1u;
    lfsr >>= 1;
    if (bit) lfsr ^= 0xB400u;
    crackleState = *reinterpret_cast<float*>(&lfsr);
    float noise = (static_cast<float>(lfsr & 0xFFFF) / 32768.f) - 1.f;
    // Sparse crackle: only occasional pops
    return (lfsr % 4096 == 0) ? noise * 0.5f : noise * 0.001f;
}

float DeckProcessor::generateWarp() noexcept
{
    warpPhase += 0.2 / sampleRate; // ~0.2 Hz LFO
    if (warpPhase > 1.0) warpPhase -= 1.0;
    return static_cast<float>(std::sin(warpPhase * 2.0 * 3.14159265));
}

//==============================================================================
// GUI → DSP interface
//==============================================================================
void DeckProcessor::touchRecord(bool isDown)
{
    handOnRecord.store(isDown, std::memory_order_release);
    if (!isDown)
    {
        // Release: let motor physics take over (smoothedSpeed stays as-is,
        // and the motor torque will gradually bring it back to target).
    }
}

void DeckProcessor::applyMouseDelta(float deltaX, float /*pixelsPerSecond*/)
{
    // Accumulate raw pixel delta. processBlock() converts to speed.
    // 500 px total ≈ 1× normal playback speed (calibrated in processBlock).
    double prev = pendingDelta.load(std::memory_order_relaxed);
    pendingDelta.store(prev + static_cast<double>(deltaX), std::memory_order_relaxed);
    newDeltaReady.store(true, std::memory_order_release);
}

//==============================================================================
// Playback control
//==============================================================================
void DeckProcessor::play()
{
    int lStart = atomLoopStart.load(std::memory_order_relaxed);
    int lEnd   = atomLoopEnd.load(std::memory_order_relaxed);
    readPosition = std::clamp(readPosition,
                              (double)lStart,
                              (double)(lEnd > 0 ? lEnd : totalSamples));
    // Jump to normal motor speed immediately — no "spin-up" chirp on play.
    angularVelocity = 1.0;
    smoothedSpeed.store(1.0, std::memory_order_relaxed);
    playState.store(PlayState::Playing, std::memory_order_release);
}

void DeckProcessor::hold()
{
    // Deck is "active" but motor target = 0: only moves when hand is on record.
    // Position is preserved so scratch resumes at the same point.
    playState.store(PlayState::Hold, std::memory_order_release);
}

void DeckProcessor::pause()
{
    tapeStopActive  = false;
    tapeStartActive = false;
    playState.store(PlayState::Paused, std::memory_order_release);
    angularVelocity = 0.0;
}

void DeckProcessor::playTape()
{
    int lStart = atomLoopStart.load(std::memory_order_relaxed);
    int lEnd   = atomLoopEnd.load(std::memory_order_relaxed);
    readPosition = std::clamp(readPosition,
                              (double)lStart,
                              (double)(lEnd > 0 ? lEnd : totalSamples));
    tapeStopActive  = false;
    tapeStartActive = false;
    angularVelocity = 0.001;  // start near zero — audio thread ramps up
    playState.store(PlayState::Playing, std::memory_order_release);
    atomTapeStartReq.store(true, std::memory_order_release);
}

void DeckProcessor::stopTape()
{
    tapeStartActive = false;
    atomTapeStopReq.store(true, std::memory_order_release);
}

void DeckProcessor::stop()
{
    int lStart = atomLoopStart.load(std::memory_order_relaxed);
    playState.store(PlayState::Stopped, std::memory_order_release);
    readPosition    = lStart;
    angularVelocity = 0.0;
    atomPlayhead.store(lStart);
}

void DeckProcessor::cue()
{
    int cp = atomCuePoint.load(std::memory_order_relaxed);
    readPosition = cp;
    atomPlayhead.store(cp);
    playState.store(PlayState::Paused, std::memory_order_release);
}

void DeckProcessor::nudge(float semitones)
{
    atomNudge.store(semitones, std::memory_order_relaxed);
}

//==============================================================================
// Sample loading (non-audio thread)
//==============================================================================
void DeckProcessor::loadFromBuffer(const juce::AudioBuffer<float>& buf,
                                   double sourceSampleRate,
                                   const juce::String& name)
{
    const juce::ScopedLock sl(samplesLock);
    sampleData.makeCopyOf(buf);
    sampleSampleRate = sourceSampleRate;
    totalSamples     = buf.getNumSamples();
    sampleName       = name;
    atomLoopStart.store(0, std::memory_order_relaxed);
    atomLoopEnd.store(totalSamples, std::memory_order_relaxed);
    atomCuePoint.store(0, std::memory_order_relaxed);
    readPosition     = 0.0;
    atomPlayhead.store(0);
    atomDefaultSample.store(false, std::memory_order_release);

    detectedBpm = detectBpm();
    computeWaveformPeaks(512);
}

void DeckProcessor::setLoop(bool enabled, int start, int end)
{
    atomLoopEnabled.store(enabled, std::memory_order_release);
    atomLoopStart.store(std::max(0, start), std::memory_order_release);
    atomLoopEnd.store((end < 0) ? totalSamples : std::min(end, totalSamples),
                      std::memory_order_release);
}

void DeckProcessor::setCuePoint(int pos)
{
    atomCuePoint.store(std::clamp(pos, 0, std::max(0, totalSamples - 1)),
                       std::memory_order_relaxed);
}

void DeckProcessor::setLoopLengthByBars(double numBars, double hostBpm)
{
    if (totalSamples <= 0) return;

    // Prefer deck-detected BPM, then host BPM, then divide sample into 4 bars.
    double useBpm = (detectedBpm > 20.0) ? detectedBpm
                  : (hostBpm     > 20.0) ? hostBpm
                  : 0.0;

    int lStart = atomLoopStart.load(std::memory_order_relaxed);
    int newEnd;
    if (useBpm > 20.0)
    {
        double barSamples = (60.0 / useBpm) * 4.0 * sampleRate;
        newEnd = lStart + static_cast<int>(barSamples * numBars);
    }
    else
    {
        // No BPM info: treat whole sample as 4 bars
        double barSamples = totalSamples / 4.0;
        newEnd = lStart + static_cast<int>(barSamples * numBars);
    }

    newEnd = std::clamp(newEnd, lStart + 1, totalSamples);
    setLoop(true, lStart, newEnd);
}

float DeckProcessor::getPlayheadNorm() const
{
    if (totalSamples <= 0) return 0.f;
    return static_cast<float>(atomPlayhead.load()) / static_cast<float>(totalSamples);
}

//==============================================================================
// Waveform peaks (non-real-time, called after load)
//==============================================================================
void DeckProcessor::computeWaveformPeaks(int numPoints)
{
    waveformPeaks.resize((size_t)numPoints, 0.f);
    if (totalSamples <= 0) return;

    const float* dataL = sampleData.getReadPointer(0);
    int chCount = sampleData.getNumChannels();
    const float* dataR = chCount > 1 ? sampleData.getReadPointer(1) : dataL;

    for (int p = 0; p < numPoints; ++p)
    {
        int start = static_cast<int>((double)p / numPoints * totalSamples);
        int end   = static_cast<int>((double)(p + 1) / numPoints * totalSamples);
        end = std::min(end, totalSamples);

        float peak = 0.f;
        for (int s = start; s < end; ++s)
        {
            float v = std::max(std::abs(dataL[s]), std::abs(dataR[s]));
            if (v > peak) peak = v;
        }
        waveformPeaks[(size_t)p] = peak;
    }
}

//==============================================================================
// Generate a built-in scratch-friendly demo sample.
// Creates a 2-second sustained organ stab (rich harmonics + tremolo).
//==============================================================================
void DeckProcessor::generateDefaultSample()
{
    const int numSamp = static_cast<int>(sampleRate * 2.0);
    juce::AudioBuffer<float> buf(2, numSamp);
    float* L = buf.getWritePointer(0);
    float* R = buf.getWritePointer(1);

    const double twoPi = 2.0 * 3.14159265358979;
    const double fund   = 220.0; // A3

    for (int i = 0; i < numSamp; ++i)
    {
        double t = (double)i / sampleRate;

        // Rich harmonic stack (organ-like, great for scratch)
        double sig = 0.35 * std::sin(twoPi * fund       * t)
                   + 0.22 * std::sin(twoPi * fund * 2.0 * t)
                   + 0.15 * std::sin(twoPi * fund * 3.0 * t)
                   + 0.10 * std::sin(twoPi * fund * 4.0 * t)
                   + 0.07 * std::sin(twoPi * fund * 5.0 * t)
                   + 0.04 * std::sin(twoPi * fund * 6.0 * t);

        // Fast attack (5 ms), slow decay, 6 Hz tremolo
        double attack  = std::min(1.0, t * 200.0);
        double decay   = std::max(0.0, 1.0 - t * 0.15);
        double tremolo = 1.0 + 0.04 * std::sin(twoPi * 6.0 * t);
        double env     = attack * decay * tremolo;

        float s = static_cast<float>(sig * env * 0.85);
        L[i] = s;
        R[i] = s;
    }

    loadFromBuffer(buf, sampleRate, "Demo (Scratch Me)");
    // Override the flag set by loadFromBuffer — this IS the default sample
    atomDefaultSample.store(true, std::memory_order_release);
    play();  // Start spinning so vinyl animates on load
}

//==============================================================================
// Transient-aligned slice detection
//==============================================================================
std::vector<int> DeckProcessor::computeTransientSlices(int numSlices) const
{
    std::vector<int> result;
    if (totalSamples < 1024 || numSlices <= 0)
        return result;

    const float* data = sampleData.getReadPointer(0);
    const int hopSize = 256;
    const int numHops = totalSamples / hopSize;

    // ── 1. Onset strength: half-wave-rectified energy flux ────────────────
    std::vector<float> onset((size_t)numHops, 0.f);
    float prevEnergy = 0.f;
    for (int h = 0; h < numHops; ++h)
    {
        float energy = 0.f;
        int hEnd = std::min((h + 1) * hopSize, totalSamples);
        for (int j = h * hopSize; j < hEnd; ++j)
            energy += data[j] * data[j];
        energy /= (hEnd - h * hopSize);
        onset[(size_t)h] = std::max(0.f, energy - prevEnergy);
        prevEnergy = energy;
    }

    // ── 2. Find beat offset: first hop above 30% of max onset ─────────────
    float maxOnset = *std::max_element(onset.begin(), onset.end());
    float threshold = maxOnset * 0.30f;
    int beatOffsetHop = 0;
    for (int h = 0; h < numHops; ++h)
    {
        if (onset[(size_t)h] > threshold) { beatOffsetHop = h; break; }
    }
    const int beatOffsetSamples = beatOffsetHop * hopSize;

    // ── 3. Beat grid parameters ───────────────────────────────────────────
    const double bpm        = (detectedBpm > 20.0) ? detectedBpm : 120.0;
    const double beatSamples = (60.0 / bpm) * sampleRate;
    // Snap candidates to nearest 1/8th note
    const double eighthSamples = beatSamples / 2.0;

    // ── 4. For each slice, find ideal grid position then snap to onset ────
    const double span = static_cast<double>(totalSamples - beatOffsetSamples);
    result.resize((size_t)numSlices);
    int prevSlice = beatOffsetSamples;

    for (int s = 0; s < numSlices; ++s)
    {
        // Ideal position: equal division over [beatOffset, totalSamples)
        double idealPos = beatOffsetSamples + (span * s) / numSlices;

        // Quantize to nearest 1/8th note grid from beatOffset
        double eighthsFromOffset = (idealPos - beatOffsetSamples) / eighthSamples;
        double quantized = beatOffsetSamples + std::round(eighthsFromOffset) * eighthSamples;

        // Search for a strong onset within ±1/16th note of quantized position
        int window = static_cast<int>(eighthSamples * 0.5);
        int lo = std::max(0, (static_cast<int>(quantized) - window) / hopSize);
        int hi = std::min(numHops - 1, (static_cast<int>(quantized) + window) / hopSize);

        float bestStr = -1.f;
        int   bestHop = static_cast<int>(quantized) / hopSize;
        for (int h = lo; h <= hi; ++h)
        {
            if (onset[(size_t)h] > bestStr) { bestStr = onset[(size_t)h]; bestHop = h; }
        }

        int candidate = (bestStr > threshold * 0.4f)
                        ? bestHop * hopSize
                        : static_cast<int>(quantized);

        // Ensure strictly increasing order, minimum 512 samples apart
        candidate = std::max(candidate, prevSlice + 512);
        candidate = std::clamp(candidate, 0, totalSamples - 1);
        result[(size_t)s] = candidate;
        prevSlice  = candidate;
    }
    return result;
}

//==============================================================================
// Simple BPM detection (onset-based, non-real-time)
//==============================================================================
double DeckProcessor::detectBpm() const
{
    if (totalSamples < static_cast<int>(sampleRate)) return 0.0;

    const float* data = sampleData.getReadPointer(0);
    const int hopSize = 512;
    std::vector<float> onsetStrength;

    float prevEnergy = 0.f;
    for (int i = 0; i + hopSize < totalSamples; i += hopSize)
    {
        float energy = 0.f;
        for (int j = i; j < i + hopSize; ++j)
            energy += data[j] * data[j];
        energy /= hopSize;
        float diff = energy - prevEnergy;
        onsetStrength.push_back(std::max(0.f, diff));
        prevEnergy = energy;
    }

    // Find average IOI (inter-onset interval)
    float threshold = 0.f;
    for (float v : onsetStrength) threshold += v;
    threshold = (threshold / onsetStrength.size()) * 1.5f;

    std::vector<int> onsets;
    int lastOnset = -20;
    for (int i = 0; i < (int)onsetStrength.size(); ++i)
    {
        if (onsetStrength[(size_t)i] > threshold && i - lastOnset > 4)
        {
            onsets.push_back(i);
            lastOnset = i;
        }
    }

    if (onsets.size() < 2) return 0.0;

    double avgIOI = 0.0;
    for (int i = 1; i < (int)onsets.size(); ++i)
        avgIOI += onsets[(size_t)i] - onsets[(size_t)(i - 1)];
    avgIOI /= (onsets.size() - 1);

    double secPerBeat = (avgIOI * hopSize) / sampleRate;
    double bpm = 60.0 / secPerBeat;

    // Clamp to sane range
    while (bpm < 70.0)  bpm *= 2.0;
    while (bpm > 200.0) bpm *= 0.5;

    return bpm;
}
