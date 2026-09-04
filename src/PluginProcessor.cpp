#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <BinaryData.h>

//==============================================================================
// Parameter layout
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ScratcherAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Crossfader
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"crossfader", 1}, "Crossfader", 0.f, 1.f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"xfaderCurve", 1}, "X-Fader Curve", 0, 6, 0));

    // Deck A
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"a_volume", 1}, "Deck A Volume", 0.f, 1.f, 1.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"a_pitch", 1}, "Deck A Pitch", -24.f, 24.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"a_speed", 1}, "Deck A Speed",
        juce::NormalisableRange<float>(0.25f, 4.f, 0.01f, 0.5f), 1.f));

    // Deck B
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"b_volume", 1}, "Deck B Volume", 0.f, 1.f, 1.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"b_pitch", 1}, "Deck B Pitch", -24.f, 24.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"b_speed", 1}, "Deck B Speed",
        juce::NormalisableRange<float>(0.25f, 4.f, 0.01f, 0.5f), 1.f));

    // Vinyl physics
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inertia", 1}, "Inertia",
        juce::NormalisableRange<float>(0.01f, 1.f, 0.001f, 0.4f), 0.12f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"slipmatFriction", 1}, "Slipmat Friction",
        juce::NormalisableRange<float>(0.01f, 1.f, 0.001f, 0.4f), 0.05f));

    // Scratch control
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"scratchSens", 1}, "Scratch Sensitivity",
        juce::NormalisableRange<float>(0.01f, 2.f, 0.005f, 0.35f), 0.12f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"scratchSmooth", 1}, "Scratch Smoothing", 0.f, 1.f, 0.82f));

    // Effects
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vinylCrackle", 1}, "Vinyl Crackle", 0.f, 1.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vinylWarp", 1}, "Vinyl Warp", 0.f, 1.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain", 0.f, 2.f, 1.f));

    // Gross Beat
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"grossBeatEnabled", 1}, "Gross Beat", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"timeEnvSlot", 1}, "Time Env Slot", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"volEnvSlot", 1}, "Vol Env Slot", 0, 7, 0));

    // Sample trim
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"a_trimStart", 1}, "Deck A Trim Start", 0.f, 1.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"a_trimEnd", 1}, "Deck A Trim End", 0.f, 1.f, 1.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"b_trimStart", 1}, "Deck B Trim Start", 0.f, 1.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"b_trimEnd", 1}, "Deck B Trim End", 0.f, 1.f, 1.f));

    // Master effects
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbSize", 1},  "Reverb Size",    0.f, 1.f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDamp", 1},  "Reverb Damping", 0.f, 1.f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbWet", 1},   "Reverb Wet",     0.f, 1.f, 0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayTime", 1},   "Delay Time",
        juce::NormalisableRange<float>(0.f, 2.f, 0.001f), 0.375f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayFeedback", 1},"Delay Feedback",0.f, 0.95f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayWet", 1},    "Delay Wet",      0.f, 1.f,  0.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"autopanRate", 1}, "AutoPan Rate",
        juce::NormalisableRange<float>(0.05f, 8.f, 0.01f, 0.4f), 1.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"autopanDepth", 1},"AutoPan Depth",  0.f, 1.f,  0.f));

    // Vinyl ambient noise
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vinylNoiseVol", 1}, "Vinyl Noise Volume", 0.f, 1.f, 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
ScratcherAudioProcessor::ScratcherAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ScratcherState", createParameterLayout())
{
    // Cache raw parameter pointers
    rawCrossfader    = apvts.getRawParameterValue("crossfader");
    rawXfaderCurve   = apvts.getRawParameterValue("xfaderCurve");
    rawOutputGain    = apvts.getRawParameterValue("outputGain");
    rawCrackle       = apvts.getRawParameterValue("vinylCrackle");
    rawWarp          = apvts.getRawParameterValue("vinylWarp");
    rawScratchSens   = apvts.getRawParameterValue("scratchSens");
    rawScratchSmooth = apvts.getRawParameterValue("scratchSmooth");
    rawInertia       = apvts.getRawParameterValue("inertia");
    rawFriction      = apvts.getRawParameterValue("slipmatFriction");
    rawAVolume       = apvts.getRawParameterValue("a_volume");
    rawBVolume       = apvts.getRawParameterValue("b_volume");
    rawAPitch        = apvts.getRawParameterValue("a_pitch");
    rawBPitch        = apvts.getRawParameterValue("b_pitch");
    rawASpeed        = apvts.getRawParameterValue("a_speed");
    rawBSpeed        = apvts.getRawParameterValue("b_speed");
    rawATrimStart    = apvts.getRawParameterValue("a_trimStart");
    rawATrimEnd      = apvts.getRawParameterValue("a_trimEnd");
    rawBTrimStart    = apvts.getRawParameterValue("b_trimStart");
    rawBTrimEnd      = apvts.getRawParameterValue("b_trimEnd");
    rawReverbSize    = apvts.getRawParameterValue("reverbSize");
    rawReverbDamp    = apvts.getRawParameterValue("reverbDamp");
    rawReverbWet     = apvts.getRawParameterValue("reverbWet");
    rawDelayTime     = apvts.getRawParameterValue("delayTime");
    rawDelayFeedback = apvts.getRawParameterValue("delayFeedback");
    rawDelayWet      = apvts.getRawParameterValue("delayWet");
    rawAutopanRate   = apvts.getRawParameterValue("autopanRate");
    rawAutopanDepth  = apvts.getRawParameterValue("autopanDepth");
    rawVinylNoiseVol = apvts.getRawParameterValue("vinylNoiseVol");

    apvts.addParameterListener("grossBeatEnabled", this);
    apvts.addParameterListener("timeEnvSlot", this);
    apvts.addParameterListener("volEnvSlot", this);

    // Default Gross Beat patterns
    timePatterns[0] = EnvelopePattern::makeNormal();
    timePatterns[1] = EnvelopePattern::makeTapeStop();
    timePatterns[2] = EnvelopePattern::makeReverse();
    timePatterns[3] = EnvelopePattern::makeStutter(8);
    timePatterns[4] = EnvelopePattern::makeStutter(16);
    timePatterns[5] = EnvelopePattern::makeScratch();
    timePatterns[6] = EnvelopePattern::makeForwardFast();
    timePatterns[7] = EnvelopePattern::makeBabyScratch();

    volPatterns[0]  = EnvelopePattern::makeVolFull();
    volPatterns[1]  = EnvelopePattern::makeVolGate(4);
    volPatterns[2]  = EnvelopePattern::makeVolGate(8);
    volPatterns[3]  = EnvelopePattern::makeVolTranceGate();
    volPatterns[4]  = EnvelopePattern::makeVolFlare();
    volPatterns[5]  = EnvelopePattern::makeVolCrab();
    volPatterns[6]  = EnvelopePattern::makeVolFadeIn();
    volPatterns[7]  = EnvelopePattern::makeVolFadeOut();

    for (auto& s : scopeBuffer) s.store(0.f);

    // Default slice MIDI notes: A = C2..G#2 (36-43), B = C3..G#3 (48-55)
    for (int i = 0; i < 8; ++i)
    {
        const auto si = (size_t)i;
        sliceStartsA[si].store(0);
        sliceStartsB[si].store(0);
        sliceNotesA[si].store(36 + i);
        sliceNotesB[si].store(48 + i);
        sliceAttackMsA[si].store(10);
        sliceDecayMsA[si].store(20);
        sliceAttackMsB[si].store(10);
        sliceDecayMsB[si].store(20);
    }

    // Sample manager callback
    sampleMgr.onLoadComplete = [this](SampleManager::DeckIndex d)
    {
        juce::ignoreUnused(d);
        if (auto* e = dynamic_cast<ScratcherAudioProcessorEditor*>(getActiveEditor()))
            e->onSampleLoaded(d);
        recomputeSlices(d == SampleManager::DeckA ? 0 : 1);
        if (onSlicesRecomputed) onSlicesRecomputed(d == SampleManager::DeckA ? 0 : 1);
    };

    triggerAsyncUpdate();
}

ScratcherAudioProcessor::~ScratcherAudioProcessor()
{
    apvts.removeParameterListener("grossBeatEnabled", this);
    apvts.removeParameterListener("timeEnvSlot", this);
    apvts.removeParameterListener("volEnvSlot", this);
}

//==============================================================================
void ScratcherAudioProcessor::handleAsyncUpdate()
{
    // Nothing to load by default — editor handles first-run onboarding.
}

//==============================================================================
void ScratcherAudioProcessor::prepareToPlay(double sr, int blockSize)
{
    sampleRate = sr;
    deckA.prepare(sr, blockSize);
    deckB.prepare(sr, blockSize);

    // Delay buffer (2 s max per channel)
    int maxDelaySamples = (int)(sr * 2.0) + 1;
    delayBufL.assign((size_t)maxDelaySamples, 0.f);
    delayBufR.assign((size_t)maxDelaySamples, 0.f);
    delayWritePos = 0;
    autopanPhase  = 0.0;

    // Reverb
    juce::Reverb::Parameters rp;
    rp.roomSize   = rawReverbSize  ? rawReverbSize->load()  : 0.5f;
    rp.damping    = rawReverbDamp  ? rawReverbDamp->load()  : 0.5f;
    rp.wetLevel   = rawReverbWet   ? rawReverbWet->load()   : 0.f;
    rp.dryLevel   = 1.f - rp.wetLevel;
    rp.width      = 1.f;
    rp.freezeMode = 0.f;
    reverb.setParameters(rp);
    reverb.setSampleRate(sr);

    // Load vinyl noise sample (idempotent — skips if already loaded at same rate)
    if (!vinylNoise.loaded)
        vinylNoise.loadAndPrepare(sr);

    // Auto-load built-in demo sample in HOLD mode (no auto-play).
    // generateDefaultSample() already calls hold() internally.
    if (deckA.getTotalSamples() == 0)
        deckA.generateDefaultSample();
    if (deckB.getTotalSamples() == 0)
        deckB.generateDefaultSample();

    if (deckA.getTotalSamples() > 0) recomputeSlices(0);
    if (deckB.getTotalSamples() > 0) recomputeSlices(1);
}

void ScratcherAudioProcessor::releaseResources() {}

bool ScratcherAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Must always have stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input can be stereo (effect mode) or disabled (instrument mode)
    auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::disabled();
}

//==============================================================================
// Audio thread
//==============================================================================
void ScratcherAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    // Effect mode: we have a real audio input connected (not just disabled bus)
    const bool effectMode = (getMainBusNumInputChannels() > 0);

    // ── Read MIDI ─────────────────────────────────────────────────────────────
    const bool  slicerOn   = slicerEnabled.load(std::memory_order_relaxed);
    const int   learnDeck  = sliceLearningDeck.load(std::memory_order_relaxed);
    const int   learnIdx   = sliceLearningIndex.load(std::memory_order_relaxed);
    const bool  scrLearnA  = midiScratchLearningA.load(std::memory_order_relaxed);
    const bool  scrLearnB  = midiScratchLearningB.load(std::memory_order_relaxed);
    const int   scrCCA     = midiScratchCCA.load(std::memory_order_relaxed);
    const int   scrCCB     = midiScratchCCB.load(std::memory_order_relaxed);

    // Decay persistent scratch speeds each block (smooth release when encoder stops)
    scrSpeedA *= 0.75f;
    scrSpeedB *= 0.75f;

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();

        // Per-deck scratch learn A
        if (scrLearnA && msg.isController())
        {
            midiScratchCCA.store(msg.getControllerNumber(), std::memory_order_relaxed);
            midiScratchLearningA.store(false, std::memory_order_relaxed);
            lastScratchCCValueA = -1;
            juce::MessageManager::callAsync([this] {
                if (auto* e = dynamic_cast<ScratcherAudioProcessorEditor*>(getActiveEditor()))
                    { e->updateMidiScratchButtons(); e->updateMidiScratchButton(); }
            });
        }
        // Per-deck scratch learn B
        if (scrLearnB && msg.isController())
        {
            midiScratchCCB.store(msg.getControllerNumber(), std::memory_order_relaxed);
            midiScratchLearningB.store(false, std::memory_order_relaxed);
            lastScratchCCValueB = -1;
            juce::MessageManager::callAsync([this] {
                if (auto* e = dynamic_cast<ScratcherAudioProcessorEditor*>(getActiveEditor()))
                    { e->updateMidiScratchButtons(); e->updateMidiScratchButton(); }
            });
        }

        // Per-deck CC scratch A
        if (scrCCA >= 0 && msg.isController() && msg.getControllerNumber() == scrCCA)
        {
            const int value = msg.getControllerValue();
            if (lastScratchCCValueA >= 0)
            {
                int delta = value - lastScratchCCValueA;
                if (delta >  63) delta -= 128;
                if (delta < -63) delta += 128;
                scrSpeedA += (float)delta * 0.3f;
            }
            lastScratchCCValueA = value;
        }
        // Per-deck CC scratch B
        if (scrCCB >= 0 && msg.isController() && msg.getControllerNumber() == scrCCB)
        {
            const int value = msg.getControllerValue();
            if (lastScratchCCValueB >= 0)
            {
                int delta = value - lastScratchCCValueB;
                if (delta >  63) delta -= 128;
                if (delta < -63) delta += 128;
                scrSpeedB += (float)delta * 0.3f;
            }
            lastScratchCCValueB = value;
        }

        // Slice note learn
        if (learnDeck >= 0 && learnIdx >= 0 && msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            if (learnDeck == 0) sliceNotesA[(size_t)learnIdx].store(note, std::memory_order_relaxed);
            else                sliceNotesB[(size_t)learnIdx].store(note, std::memory_order_relaxed);
            sliceLearningDeck.store(-1, std::memory_order_relaxed);
            sliceLearningIndex.store(-1, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this] {
                if (auto* e = dynamic_cast<ScratcherAudioProcessorEditor*>(getActiveEditor()))
                    e->updateSlicerPads();
            });
        }

        // Slicer trigger
        if (slicerOn && msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            for (int i = 0; i < 8; ++i)
            {
                const auto si = (size_t)i;
                if (note == sliceNotesA[si].load(std::memory_order_relaxed))
                {
                    int startPos = sliceStartsA[si].load(std::memory_order_relaxed);
                    int endPos   = (i < 7) ? sliceStartsA[si + 1].load(std::memory_order_relaxed)
                                           : deckA.getTotalSamples();
                    deckA.setSliceAttackMs(sliceAttackMsA[si].load(std::memory_order_relaxed));
                    deckA.setSliceDecayMs (sliceDecayMsA[si].load(std::memory_order_relaxed));
                    deckA.setLoop(true, startPos, endPos);
                    deckA.seekToSample(startPos);
                    slicerActiveNote.store(note, std::memory_order_relaxed);
                }
                if (note == sliceNotesB[si].load(std::memory_order_relaxed))
                {
                    int startPos = sliceStartsB[si].load(std::memory_order_relaxed);
                    int endPos   = (i < 7) ? sliceStartsB[si + 1].load(std::memory_order_relaxed)
                                           : deckB.getTotalSamples();
                    deckB.setSliceAttackMs(sliceAttackMsB[si].load(std::memory_order_relaxed));
                    deckB.setSliceDecayMs (sliceDecayMsB[si].load(std::memory_order_relaxed));
                    deckB.setLoop(true, startPos, endPos);
                    deckB.seekToSample(startPos);
                    slicerActiveNote.store(note, std::memory_order_relaxed);
                }
            }
        }
        if (slicerOn && msg.isNoteOff())
        {
            if (msg.getNoteNumber() == slicerActiveNote.load(std::memory_order_relaxed))
                slicerActiveNote.store(-1, std::memory_order_relaxed);
        }

        midiLearn.processMidi(msg, apvts);
    }
    midiMessages.clear();

    // Each deck only responds to its own per-deck CC (global button sets both simultaneously)
    deckA.setExternalScratchSpeed(scrSpeedA);
    deckB.setExternalScratchSpeed(scrSpeedB);

    // ── Host playhead ─────────────────────────────────────────────────────────
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())
                hostBpm.store(*pos->getBpm(), std::memory_order_relaxed);
            hostIsPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);

            if (pos->getBarCount().hasValue() && pos->getPpqPosition().hasValue())
            {
                double ppq    = *pos->getPpqPosition();
                [[maybe_unused]] double bpm = hostBpm.load(std::memory_order_relaxed);
                double barLen = 4.0; // 4/4 assumed
                double phase  = std::fmod(ppq, barLen) / barLen;
                hostBarPhase.store(phase, std::memory_order_relaxed);
            }
        }
    }

    // ── Load parameters ───────────────────────────────────────────────────────
    const float crossfader  = rawCrossfader ->load(std::memory_order_relaxed);
    const int   curveIdx    = static_cast<int>(rawXfaderCurve->load(std::memory_order_relaxed));
    const float outputGain  = rawOutputGain ->load(std::memory_order_relaxed);
    const float inertia     = rawInertia    ->load(std::memory_order_relaxed);
    const float friction    = rawFriction   ->load(std::memory_order_relaxed);
    const float crackle     = rawCrackle    ->load(std::memory_order_relaxed);
    const float warp        = rawWarp       ->load(std::memory_order_relaxed);
    const float sensitivity = rawScratchSens->load(std::memory_order_relaxed);
    const float smoothing   = rawScratchSmooth->load(std::memory_order_relaxed);

    // Push physics params to decks
    deckA.setInertia(inertia);      deckB.setInertia(inertia);
    deckA.setFriction(friction);    deckB.setFriction(friction);
    deckA.setSensitivity(sensitivity); deckB.setSensitivity(sensitivity);
    deckA.setSmoothing(smoothing);  deckB.setSmoothing(smoothing);
    deckA.setVolume(rawAVolume->load(std::memory_order_relaxed));
    deckB.setVolume(rawBVolume->load(std::memory_order_relaxed));
    deckA.setPitch(rawAPitch->load(std::memory_order_relaxed));
    deckB.setPitch(rawBPitch->load(std::memory_order_relaxed));
    deckA.setSpeed(rawASpeed->load(std::memory_order_relaxed));
    deckB.setSpeed(rawBSpeed->load(std::memory_order_relaxed));
    deckA.setVinylCrackle(crackle); deckB.setVinylCrackle(crackle);
    deckA.setVinylWarp(warp);       deckB.setVinylWarp(warp);

    // Apply sample trim
    if (rawATrimStart && rawATrimEnd)
    {
        int tsA = (int)(rawATrimStart->load(std::memory_order_relaxed) * deckA.getTotalSamples());
        int teA = (int)(rawATrimEnd->load(std::memory_order_relaxed)   * deckA.getTotalSamples());
        deckA.setTrimStart(tsA);
        deckA.setTrimEnd(teA);
    }
    if (rawBTrimStart && rawBTrimEnd)
    {
        int tsB = (int)(rawBTrimStart->load(std::memory_order_relaxed) * deckB.getTotalSamples());
        int teB = (int)(rawBTrimEnd->load(std::memory_order_relaxed)   * deckB.getTotalSamples());
        deckB.setTrimStart(tsB);
        deckB.setTrimEnd(teB);
    }

    // ── Gross Beat envelope values ────────────────────────────────────────────
    float timeEnvA = 0.f, volEnvA  = 1.f;
    float timeEnvB = 0.f, volEnvB  = 1.f;

    const bool gbEnabled = grossBeatEnabled.load(std::memory_order_relaxed);
    if (gbEnabled)
    {
        float phase = static_cast<float>(hostBarPhase.load(std::memory_order_relaxed));
        int tSlot   = activeTimeSlot.load(std::memory_order_relaxed);
        int vSlot   = activeVolSlot.load(std::memory_order_relaxed);

        timeEnvA = timeEnvB = timePatterns[(size_t)tSlot].evaluate(phase);
        volEnvA  = volEnvB  = volPatterns[(size_t)vSlot].evaluate(phase);
    }

    // ── Input buffers for effect mode ─────────────────────────────────────────
    const float* inL = effectMode ? buffer.getReadPointer(0) : nullptr;
    const float* inR = (effectMode && buffer.getNumChannels() > 1)
                       ? buffer.getReadPointer(1) : inL;

    // ── Temp buffers for deck output ──────────────────────────────────────────
    juce::AudioBuffer<float> bufA(2, numSamples), bufB(2, numSamples);
    bufA.clear(); bufB.clear();

    deckA.processBlock(inL, inR,
                       bufA.getWritePointer(0), bufA.getWritePointer(1),
                       numSamples, effectMode, timeEnvA, volEnvA);
    deckB.processBlock(inL, inR,
                       bufB.getWritePointer(0), bufB.getWritePointer(1),
                       numSamples, effectMode, timeEnvB, volEnvB);

    // ── Crossfade mix ─────────────────────────────────────────────────────────
    auto gains = Crossfader::computeByIndex(crossfader, curveIdx);

    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    const float* aL = bufA.getReadPointer(0);
    const float* aR = bufA.getReadPointer(1);
    const float* bL = bufB.getReadPointer(0);
    const float* bR = bufB.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = (aL[i] * gains.gA + bL[i] * gains.gB) * outputGain;
        outR[i] = (aR[i] * gains.gA + bR[i] * gains.gB) * outputGain;
    }

    // ── Master effects (reverb → delay → autopan) ──────────────────────────────
    {
        const float revWet  = rawReverbWet     ? rawReverbWet->load()     : 0.f;
        const float delWet  = rawDelayWet      ? rawDelayWet->load()      : 0.f;
        const float panDep  = rawAutopanDepth  ? rawAutopanDepth->load()  : 0.f;
        const float panRate = rawAutopanRate   ? rawAutopanRate->load()   : 1.f;

        // Update reverb parameters (cheap — only changes coeffs when values change)
        if (revWet > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize   = rawReverbSize ? rawReverbSize->load() : 0.5f;
            rp.damping    = rawReverbDamp ? rawReverbDamp->load() : 0.5f;
            rp.wetLevel   = revWet;
            rp.dryLevel   = 1.f - revWet;
            rp.width      = 1.f;
            rp.freezeMode = 0.f;
            reverb.setParameters(rp);
            reverb.processStereo(outL, outR, numSamples);
        }

        // Delay
        if (delWet > 0.001f && !delayBufL.empty())
        {
            const float delayTimeSec = rawDelayTime     ? rawDelayTime->load()     : 0.375f;
            const float feedback     = rawDelayFeedback ? rawDelayFeedback->load() : 0.4f;
            const int   delaySamp    = std::min((int)(delayTimeSec * sampleRate),
                                                (int)delayBufL.size() - 1);
            for (int i = 0; i < numSamples; ++i)
            {
                int readPos = ((delayWritePos - delaySamp) + (int)delayBufL.size())
                              % (int)delayBufL.size();
                float dL = delayBufL[(size_t)readPos];
                float dR = delayBufR[(size_t)readPos];
                delayBufL[(size_t)delayWritePos] = outL[i] + dL * feedback;
                delayBufR[(size_t)delayWritePos] = outR[i] + dR * feedback;
                outL[i] = outL[i] * (1.f - delWet) + dL * delWet;
                outR[i] = outR[i] * (1.f - delWet) + dR * delWet;
                delayWritePos = (delayWritePos + 1) % (int)delayBufL.size();
            }
        }

        // Auto-pan
        if (panDep > 0.001f)
        {
            const double phaseInc = 2.0 * 3.14159265358979 * panRate / sampleRate;
            for (int i = 0; i < numSamples; ++i)
            {
                float lfo  = (float)std::sin(autopanPhase) * panDep * 0.5f;
                outL[i]   *= 1.f + lfo;
                outR[i]   *= 1.f - lfo;
                autopanPhase += phaseInc;
                if (autopanPhase >= 2.0 * 3.14159265358979) autopanPhase -= 2.0 * 3.14159265358979;
            }
        }
        else
        {
            // Keep phase accumulating to avoid click when depth is re-enabled
            autopanPhase += 2.0 * 3.14159265358979 * panRate / sampleRate * numSamples;
            if (autopanPhase >= 2.0 * 3.14159265358979) autopanPhase -= 2.0 * 3.14159265358979;
        }
    }

    // ── Scope ring buffer ─────────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        int sp = scopeWritePos.load(std::memory_order_relaxed);
        scopeBuffer[sp & (SCOPE_SIZE - 1)].store((outL[i] + outR[i]) * 0.5f);
        scopeWritePos.store(sp + 1, std::memory_order_relaxed);
    }

    // ── Vinyl ambient noise ───────────────────────────────────────────────────
    if (rawVinylNoiseVol)
        vinylNoise.volume.store(rawVinylNoiseVol->load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
    vinylNoise.process(outL, outR, numSamples, sampleRate);
}

//==============================================================================
void ScratcherAudioProcessor::recomputeSlices(int deck)
{
    DeckProcessor& dp = (deck == 0) ? deckA : deckB;
    auto& starts = (deck == 0) ? sliceStartsA : sliceStartsB;

    if (dp.getTotalSamples() <= 0) return;

    auto pts = dp.computeTransientSlices(8);
    for (int i = 0; i < 8 && i < (int)pts.size(); ++i)
        starts[(size_t)i].store(pts[(size_t)i], std::memory_order_relaxed);
}

//==============================================================================
void ScratcherAudioProcessor::parameterChanged(const juce::String& paramID,
                                               float newValue)
{
    if (paramID == "grossBeatEnabled")
        grossBeatEnabled.store(newValue > 0.5f);
    else if (paramID == "timeEnvSlot")
        activeTimeSlot.store(static_cast<int>(newValue));
    else if (paramID == "volEnvSlot")
        activeVolSlot.store(static_cast<int>(newValue));
}

//==============================================================================
// Programs
//==============================================================================
const juce::String ScratcherAudioProcessor::getProgramName(int index)
{
    static const char* names[] = {
        "DJ Mode", "Scratch Battle", "Studio FX", "Smooth Mix",
        "Vinyl Emulation", "Stutter Gate", "Reverse Loop", "Default"
    };
    return (index >= 0 && index < 8) ? names[index] : "Unknown";
}

void ScratcherAudioProcessor::setCurrentProgram(int index)
{
    currentProgram = index;
    // Apply preset parameters
    switch (index)
    {
    case 0: // DJ Mode
        apvts.getParameter("crossfader")->setValueNotifyingHost(0.5f);
        apvts.getParameter("xfaderCurve")->setValueNotifyingHost(
            apvts.getParameter("xfaderCurve")->convertTo0to1(1.f));
        apvts.getParameter("inertia")->setValueNotifyingHost(
            apvts.getParameter("inertia")->convertTo0to1(0.1f));
        break;
    case 1: // Scratch Battle
        apvts.getParameter("xfaderCurve")->setValueNotifyingHost(
            apvts.getParameter("xfaderCurve")->convertTo0to1(3.f));
        apvts.getParameter("inertia")->setValueNotifyingHost(
            apvts.getParameter("inertia")->convertTo0to1(0.05f));
        break;
    case 4: // Vinyl Emulation
        apvts.getParameter("vinylCrackle")->setValueNotifyingHost(0.3f);
        apvts.getParameter("vinylWarp")->setValueNotifyingHost(0.2f);
        apvts.getParameter("inertia")->setValueNotifyingHost(
            apvts.getParameter("inertia")->convertTo0to1(0.3f));
        break;
    case 5: // Stutter Gate
        apvts.getParameter("grossBeatEnabled")->setValueNotifyingHost(1.f);
        apvts.getParameter("volEnvSlot")->setValueNotifyingHost(
            apvts.getParameter("volEnvSlot")->convertTo0to1(2.f));
        break;
    case 6: // Reverse Loop
        apvts.getParameter("grossBeatEnabled")->setValueNotifyingHost(1.f);
        apvts.getParameter("timeEnvSlot")->setValueNotifyingHost(
            apvts.getParameter("timeEnvSlot")->convertTo0to1(2.f));
        break;
    case 7: // Default
        apvts.getParameter("crossfader")->setValueNotifyingHost(0.5f);
        apvts.getParameter("xfaderCurve")->setValueNotifyingHost(0.f);
        apvts.getParameter("grossBeatEnabled")->setValueNotifyingHost(0.f);
        apvts.getParameter("vinylCrackle")->setValueNotifyingHost(0.f);
        apvts.getParameter("vinylWarp")->setValueNotifyingHost(0.f);
        break;
    default: break;
    }
}

//==============================================================================
// State save / load
//==============================================================================
void ScratcherAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto xml   = state.createXml();

    // Save Gross Beat patterns
    auto* gbNode = xml->createNewChildElement("GrossBeat");
    for (int i = 0; i < 8; ++i)
    {
        timePatterns[(size_t)i].saveToXml(*gbNode, "TimePattern" + juce::String(i));
        volPatterns[(size_t)i].saveToXml(*gbNode, "VolPattern"  + juce::String(i));
    }

    // Save MIDI bindings
    midiLearn.saveToXml(*xml);

    // Save first-run flag
    xml->setAttribute("firstRun", isFirstRun.load() ? 1 : 0);

    // Save loaded file paths
    xml->setAttribute("filePathA", sampleMgr.getLastLoadedFile(SampleManager::DeckA).getFullPathName());
    xml->setAttribute("filePathB", sampleMgr.getLastLoadedFile(SampleManager::DeckB).getFullPathName());

    // Save slice positions and per-slice envelopes
    auto* slicesNode = xml->createNewChildElement("Slices");
    for (int i = 0; i < 8; ++i)
    {
        const auto si = (size_t)i;
        slicesNode->setAttribute("startA" + juce::String(i), sliceStartsA[si].load());
        slicesNode->setAttribute("startB" + juce::String(i), sliceStartsB[si].load());
        slicesNode->setAttribute("noteA"  + juce::String(i), sliceNotesA[si].load());
        slicesNode->setAttribute("noteB"  + juce::String(i), sliceNotesB[si].load());
        slicesNode->setAttribute("atkA"   + juce::String(i), sliceAttackMsA[si].load());
        slicesNode->setAttribute("dcyA"   + juce::String(i), sliceDecayMsA[si].load());
        slicesNode->setAttribute("atkB"   + juce::String(i), sliceAttackMsB[si].load());
        slicesNode->setAttribute("dcyB"   + juce::String(i), sliceDecayMsB[si].load());
    }

    copyXmlToBinary(*xml, dest);
}

void ScratcherAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml) return;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));

    // Load Gross Beat patterns
    if (auto* gbNode = xml->getChildByName("GrossBeat"))
    {
        for (int i = 0; i < 8; ++i)
        {
            timePatterns[(size_t)i].loadFromXml(*gbNode, "TimePattern" + juce::String(i));
            volPatterns[(size_t)i].loadFromXml(*gbNode, "VolPattern"   + juce::String(i));
        }
    }

    midiLearn.loadFromXml(*xml);
    isFirstRun.store(xml->getIntAttribute("firstRun", 1) != 0);

    // Restore slice positions and per-slice envelopes
    if (auto* slicesNode = xml->getChildByName("Slices"))
    {
        for (int i = 0; i < 8; ++i)
        {
            const auto si = (size_t)i;
            sliceStartsA[si].store(slicesNode->getIntAttribute("startA" + juce::String(i), 0));
            sliceStartsB[si].store(slicesNode->getIntAttribute("startB" + juce::String(i), 0));
            sliceNotesA[si].store(slicesNode->getIntAttribute("noteA"   + juce::String(i), 36 + i));
            sliceNotesB[si].store(slicesNode->getIntAttribute("noteB"   + juce::String(i), 48 + i));
            sliceAttackMsA[si].store(slicesNode->getIntAttribute("atkA" + juce::String(i), 10));
            sliceDecayMsA[si].store(slicesNode->getIntAttribute("dcyA"  + juce::String(i), 20));
            sliceAttackMsB[si].store(slicesNode->getIntAttribute("atkB" + juce::String(i), 10));
            sliceDecayMsB[si].store(slicesNode->getIntAttribute("dcyB"  + juce::String(i), 20));
        }
    }

    // Restore loaded files — reload asynchronously after state is applied
    juce::String pathA = xml->getStringAttribute("filePathA", "");
    juce::String pathB = xml->getStringAttribute("filePathB", "");

    juce::MessageManager::callAsync([this, pathA, pathB]
    {
        if (pathA.isNotEmpty())
        {
            juce::File f(pathA);
            if (f.existsAsFile())
                sampleMgr.loadFile(f, SampleManager::DeckA, deckA, sampleRate);
        }
        if (pathB.isNotEmpty())
        {
            juce::File f(pathB);
            if (f.existsAsFile())
                sampleMgr.loadFile(f, SampleManager::DeckB, deckB, sampleRate);
        }
    });
}

//==============================================================================
void ScratcherAudioProcessor::tapTempo()
{
    auto now = juce::Time::currentTimeMillis();
    if (lastTapTime > 0)
    {
        double interval = (double)(now - lastTapTime) / 1000.0;
        if (interval > 0.2 && interval < 3.0)
        {
            tapIntervals.push_back(60.0 / interval);
            if (tapIntervals.size() > 8) tapIntervals.erase(tapIntervals.begin());

            double avg = 0.0;
            for (auto v : tapIntervals) avg += v;
            avg /= tapIntervals.size();
            tapBpm.store(avg, std::memory_order_relaxed);
        }
        else if (interval >= 3.0)
        {
            tapIntervals.clear();
        }
    }
    lastTapTime = now;
}

//==============================================================================
juce::AudioProcessorEditor* ScratcherAudioProcessor::createEditor()
{
    return new ScratcherAudioProcessorEditor(*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScratcherAudioProcessor();
}

//==============================================================================
// VinylNoisePlayer
//==============================================================================
void ScratcherAudioProcessor::VinylNoisePlayer::loadAndPrepare(double /*playbackSampleRate*/)
{
    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();

    auto* stream = new juce::MemoryInputStream(
        BinaryData::vinyl_mp3, BinaryData::vinyl_mp3Size, false);

    std::unique_ptr<juce::AudioFormatReader> reader(
        fmtMgr.createReaderFor(std::unique_ptr<juce::InputStream>(stream)));

    if (!reader) { loaded = false; return; }

    nativeSampleRate = reader->sampleRate;

    // Trim to 16 seconds at native sample rate
    int maxNativeSamples = (int)std::min(
        (juce::int64)(nativeSampleRate * 16.0),
        reader->lengthInSamples);

    sampleData.setSize((int)reader->numChannels, maxNativeSamples, false, true, false);
    reader->read(&sampleData, 0, maxNativeSamples, 0, true, true);
    totalSamples = maxNativeSamples;

    if (totalSamples <= 0) { loaded = false; return; }

    // Cross-fade length: 500ms in native samples
    xfadeSamples = std::min((int)(nativeSampleRate * 0.5), totalSamples / 4);
    loopEnd = totalSamples;

    // ── Find optimal loop start ───────────────────────────────────────────────
    // Search in [0.5s .. 2.0s] for the 100ms window whose RMS best matches
    // the RMS of the last 100ms before the loop end (minimises amplitude click).
    const int winLen     = std::max(1, (int)(nativeSampleRate * 0.1));  // 100 ms
    const int searchFrom = (int)(nativeSampleRate * 0.5);               // 0.5 s
    const int searchTo   = std::min((int)(nativeSampleRate * 2.0),
                                    totalSamples - winLen);

    // RMS of the end region
    double endRms = 0.0;
    int endWinStart = std::max(0, loopEnd - winLen);
    for (int i = endWinStart; i < loopEnd; ++i)
    {
        double s = sampleData.getSample(0, i);
        endRms += s * s;
    }
    endRms = std::sqrt(endRms / std::max(1, loopEnd - endWinStart));

    // Find best match
    double bestDiff = 1.0e10;
    loopStart = searchFrom;
    for (int pos = searchFrom; pos < searchTo; pos += winLen / 4)
    {
        double rms = 0.0;
        for (int j = pos; j < pos + winLen && j < totalSamples; ++j)
        {
            double s = sampleData.getSample(0, j);
            rms += s * s;
        }
        rms = std::sqrt(rms / winLen);
        double diff = std::abs(rms - endRms);
        if (diff < bestDiff) { bestDiff = diff; loopStart = pos; }
    }

    // Align loopStart to the nearest zero-crossing (avoids initial click)
    int scanEnd = std::min(loopStart + winLen / 2, totalSamples - 1);
    for (int i = loopStart; i < scanEnd; ++i)
    {
        if (sampleData.getSample(0, i) * sampleData.getSample(0, i + 1) <= 0.f)
        {
            loopStart = i;
            break;
        }
    }

    readPos = (double)loopStart;
    loaded  = true;
}

void ScratcherAudioProcessor::VinylNoisePlayer::process(
    float* outL, float* outR, int numSamples, double playbackSampleRate)
{
    float vol = volume.load(std::memory_order_relaxed);
    if (vol < 1e-5f || !loaded || totalSamples <= 0) return;

    const double ratio = nativeSampleRate / playbackSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        int   iPos = (int)readPos;
        float frac = (float)(readPos - iPos);

        iPos = std::clamp(iPos, 0, totalSamples - 1);
        int iPos1 = std::min(iPos + 1, totalSamples - 1);

        // Linear interpolation on main stream
        float sL = sampleData.getSample(0, iPos)  * (1.f - frac)
                 + sampleData.getSample(0, iPos1) * frac;
        float sR = (sampleData.getNumChannels() > 1)
                 ? (sampleData.getSample(1, iPos)  * (1.f - frac)
                  + sampleData.getSample(1, iPos1) * frac)
                 : sL;

        // Crossfade zone: blend with loopStart
        int distFromEnd = loopEnd - iPos;
        if (xfadeSamples > 0 && distFromEnd < xfadeSamples)
        {
            float alpha = (float)distFromEnd / (float)xfadeSamples;  // 1→0

            int xPos  = loopStart + (xfadeSamples - distFromEnd);
            xPos      = std::clamp(xPos, 0, totalSamples - 1);
            int xPos1 = std::min(xPos + 1, totalSamples - 1);

            float sL2 = sampleData.getSample(0, xPos)  * (1.f - frac)
                      + sampleData.getSample(0, xPos1) * frac;
            float sR2 = (sampleData.getNumChannels() > 1)
                      ? (sampleData.getSample(1, xPos)  * (1.f - frac)
                       + sampleData.getSample(1, xPos1) * frac)
                      : sL2;

            sL = sL * alpha + sL2 * (1.f - alpha);
            sR = sR * alpha + sR2 * (1.f - alpha);
        }

        outL[i] += sL * vol;
        outR[i] += sR * vol;

        readPos += ratio;
        if ((int)readPos >= loopEnd)
            readPos = (double)(loopStart + xfadeSamples);
    }
}
