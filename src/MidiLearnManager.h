#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// MidiLearnManager — maps incoming MIDI CC / Note messages to APVTS parameters.
// All public methods are safe to call from the message thread.
// processMidi() is called from the audio thread but only reads bindings (no lock).
//==============================================================================
class MidiLearnManager
{
public:
    struct Binding
    {
        juce::String paramID;
        int  midiChannel  = -1;   // 1-16, -1 = any channel
        int  ccNumber     = -1;   // ≥0 = CC, -1 = not CC
        int  noteNumber   = -1;   // ≥0 = Note, -1 = not Note
        bool usePitchBend = false;
        float rangeMin    = 0.f;
        float rangeMax    = 1.f;
    };

    // Fired when a learn operation completes (message thread callback).
    std::function<void(const Binding&)> onLearnComplete;

    MidiLearnManager() = default;

    // ── Learn mode ────────────────────────────────────────────────────────────
    void startLearn(const juce::String& paramID);
    void stopLearn();
    void cancelLearn() { learningActive = false; }
    bool isLearning() const noexcept { return learningActive; }
    juce::String getLearningParam() const { return learningParam; }

    // ── Binding management ────────────────────────────────────────────────────
    void addBinding(const Binding& b);
    void removeBinding(const juce::String& paramID);
    void clearAll();
    bool hasBinding(const juce::String& paramID) const;
    juce::String getBindingLabel(const juce::String& paramID) const;

    // ── Audio thread: process MIDI and update APVTS ───────────────────────────
    // Call from processBlock() for each MIDI message.
    void processMidi(const juce::MidiMessage& msg,
                     juce::AudioProcessorValueTreeState& apvts);

    // ── Serialisation ─────────────────────────────────────────────────────────
    void saveToXml(juce::XmlElement& parent) const;
    void loadFromXml(const juce::XmlElement& parent);

    const std::vector<Binding>& getBindings() const { return bindings; }

    // Default recommended bindings (shown in help panel)
    static std::vector<Binding> getDefaultBindings();

private:
    std::vector<Binding> bindings;
    bool         learningActive = false;
    juce::String learningParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};
