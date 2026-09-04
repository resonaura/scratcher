#include "MidiLearnManager.h"

void MidiLearnManager::startLearn(const juce::String& paramID)
{
    learningParam  = paramID;
    learningActive = true;
}

void MidiLearnManager::stopLearn()
{
    learningActive = false;
    learningParam  = {};
}

void MidiLearnManager::addBinding(const Binding& b)
{
    // Remove any existing binding for this param
    removeBinding(b.paramID);
    bindings.push_back(b);
}

void MidiLearnManager::removeBinding(const juce::String& paramID)
{
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(),
                       [&](const Binding& b){ return b.paramID == paramID; }),
        bindings.end());
}

void MidiLearnManager::clearAll()
{
    bindings.clear();
}

bool MidiLearnManager::hasBinding(const juce::String& paramID) const
{
    for (auto& b : bindings)
        if (b.paramID == paramID) return true;
    return false;
}

juce::String MidiLearnManager::getBindingLabel(const juce::String& paramID) const
{
    for (auto& b : bindings)
    {
        if (b.paramID != paramID) continue;
        if (b.usePitchBend)     return "PBend";
        if (b.ccNumber >= 0)    return "CC" + juce::String(b.ccNumber);
        if (b.noteNumber >= 0)  return juce::MidiMessage::getMidiNoteName(b.noteNumber, true, true, 4);
    }
    return {};
}

void MidiLearnManager::processMidi(const juce::MidiMessage& msg,
                                   juce::AudioProcessorValueTreeState& apvts)
{
    // ── Learn mode: capture next CC/Note/PitchBend ────────────────────────────
    if (learningActive)
    {
        Binding b;
        b.paramID    = learningParam;
        b.midiChannel = msg.getChannel();

        if (msg.isController())
        {
            b.ccNumber = msg.getControllerNumber();
        }
        else if (msg.isNoteOn())
        {
            b.noteNumber = msg.getNoteNumber();
        }
        else if (msg.isPitchWheel())
        {
            b.usePitchBend = true;
        }
        else
        {
            return; // ignore other message types during learn
        }

        addBinding(b);
        learningActive = false;

        if (onLearnComplete)
        {
            const auto captured = b;
            juce::MessageManager::callAsync([this, captured]{ onLearnComplete(captured); });
        }
        return;
    }

    // ── Normal mode: apply bindings ───────────────────────────────────────────
    for (auto& b : bindings)
    {
        if (b.midiChannel > 0 && msg.getChannel() != b.midiChannel) continue;

        float value = 0.f;
        bool  matched = false;

        if (b.usePitchBend && msg.isPitchWheel())
        {
            // Pitch wheel: 0..16383 → range
            value   = (msg.getPitchWheelValue() / 16383.f);
            matched = true;
        }
        else if (b.ccNumber >= 0 && msg.isController()
                 && msg.getControllerNumber() == b.ccNumber)
        {
            value   = msg.getControllerValue() / 127.f;
            matched = true;
        }
        else if (b.noteNumber >= 0 && msg.isNoteOn()
                 && msg.getNoteNumber() == b.noteNumber)
        {
            value   = 1.f;
            matched = true;
        }
        else if (b.noteNumber >= 0 && msg.isNoteOff()
                 && msg.getNoteNumber() == b.noteNumber)
        {
            value   = 0.f;
            matched = true;
        }

        if (matched)
        {
            float mapped = b.rangeMin + value * (b.rangeMax - b.rangeMin);
            if (auto* param = apvts.getParameter(b.paramID))
            {
                float norm = param->convertTo0to1(mapped);
                param->setValueNotifyingHost(norm);
            }
        }
    }
}

void MidiLearnManager::saveToXml(juce::XmlElement& parent) const
{
    auto* node = parent.createNewChildElement("MidiBindings");
    for (auto& b : bindings)
    {
        auto* e = node->createNewChildElement("Binding");
        e->setAttribute("param",      b.paramID);
        e->setAttribute("channel",    b.midiChannel);
        e->setAttribute("cc",         b.ccNumber);
        e->setAttribute("note",       b.noteNumber);
        e->setAttribute("pitchBend",  b.usePitchBend ? 1 : 0);
        e->setAttribute("rangeMin",   b.rangeMin);
        e->setAttribute("rangeMax",   b.rangeMax);
    }
}

void MidiLearnManager::loadFromXml(const juce::XmlElement& parent)
{
    bindings.clear();
    auto* node = parent.getChildByName("MidiBindings");
    if (!node) return;

    for (auto* e : node->getChildIterator())
    {
        Binding b;
        b.paramID      = e->getStringAttribute("param");
        b.midiChannel  = e->getIntAttribute("channel", -1);
        b.ccNumber     = e->getIntAttribute("cc", -1);
        b.noteNumber   = e->getIntAttribute("note", -1);
        b.usePitchBend = e->getIntAttribute("pitchBend", 0) != 0;
        b.rangeMin     = static_cast<float>(e->getDoubleAttribute("rangeMin", 0.0));
        b.rangeMax     = static_cast<float>(e->getDoubleAttribute("rangeMax", 1.0));
        if (b.paramID.isNotEmpty()) bindings.push_back(b);
    }
}

std::vector<MidiLearnManager::Binding> MidiLearnManager::getDefaultBindings()
{
    return {
        { "crossfader",   1,  7, -1, false, 0.f, 1.f },   // CC 7 → crossfader
        { "a_volume",     1,  1, -1, false, 0.f, 1.f },   // CC 1 → Deck A vol
        { "b_volume",     1, 11, -1, false, 0.f, 1.f },   // CC 11 → Deck B vol
        { "a_scratchPos", 1, -1, -1, true,  0.f, 1.f },   // PB ch1 → Deck A scratch
        { "b_scratchPos", 2, -1, -1, true,  0.f, 1.f },   // PB ch2 → Deck B scratch
    };
}
