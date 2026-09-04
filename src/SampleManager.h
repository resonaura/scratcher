#pragma once
#include <JuceHeader.h>
#include "DeckProcessor.h"
#include <functional>

//==============================================================================
// SampleManager — asynchronous file loading for both decks.
// Loading runs on a background thread so the audio thread is never blocked.
//==============================================================================
class SampleManager : private juce::Thread
{
public:
    enum DeckIndex { DeckA = 0, DeckB = 1 };

    // Callback fired on the message thread after loading completes.
    std::function<void(DeckIndex)> onLoadComplete;

    SampleManager();
    ~SampleManager() override;

    // Trigger async load of a file into a deck.
    // The DeckProcessor will be updated on load completion.
    void loadFile(const juce::File& file, DeckIndex deck, DeckProcessor& deckProc,
                  double targetSampleRate);

    // Auto-preset: adjust loop points to align the loaded sample to a BPM grid.
    struct AutoPreset
    {
        juce::String name;
        double barCount;     // number of bars (0 = auto-detect)
        bool   beatMatch;    // rescale speed to match host BPM
    };

    static std::vector<AutoPreset> getAutoPresets();

    // Apply a preset after loading. Must call after load completes.
    void applyAutoPreset(DeckIndex deck, const AutoPreset& p,
                         double hostBpm, DeckProcessor& deckProc);

    bool isLoading(DeckIndex deck) const;
    juce::String getFileName(DeckIndex deck) const;
    juce::File   getLastLoadedFile(DeckIndex deck) const;
    double getDurationSeconds(DeckIndex deck) const;

private:
    struct LoadRequest
    {
        juce::File file;
        DeckIndex  deck;
        DeckProcessor* deckProc;
        double     targetSampleRate;
    };

    juce::AbstractFifo fifo { 4 };
    LoadRequest        requestBuffer[4];

    struct DeckInfo
    {
        std::atomic<bool> loading { false };
        juce::String      fileName;
        juce::File        lastLoadedFile;
        double            durationSeconds = 0.0;
    } deckInfo[2];

    void run() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleManager)
};
