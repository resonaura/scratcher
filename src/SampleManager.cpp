#include "SampleManager.h"

SampleManager::SampleManager() : juce::Thread("ScratcherSampleLoader")
{
    startThread(juce::Thread::Priority::low);
}

SampleManager::~SampleManager()
{
    stopThread(2000);
}

void SampleManager::loadFile(const juce::File& file, DeckIndex deck,
                              DeckProcessor& deckProc, double targetSampleRate)
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        requestBuffer[start1] = { file, deck, &deckProc, targetSampleRate };
        fifo.finishedWrite(1);
        deckInfo[deck].loading.store(true);
        notify();
    }
}

void SampleManager::run()
{
    while (!threadShouldExit())
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(1, start1, size1, start2, size2);

        if (size1 == 0)
        {
            wait(100);
            continue;
        }

        LoadRequest req = requestBuffer[start1];
        fifo.finishedRead(1);

        // ── Load and decode audio file ────────────────────────────────────
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(
            fmtMgr.createReaderFor(req.file));

        if (reader)
        {
            juce::AudioBuffer<float> buf(
                static_cast<int>(reader->numChannels),
                static_cast<int>(reader->lengthInSamples));

            reader->read(&buf, 0, static_cast<int>(reader->lengthInSamples),
                         0, true, true);

            // Resample if needed
            if (static_cast<int>(reader->sampleRate) != static_cast<int>(req.targetSampleRate) &&
                reader->lengthInSamples > 0)
            {
                double ratio = req.targetSampleRate / reader->sampleRate;
                int newLen   = static_cast<int>(reader->lengthInSamples * ratio);

                juce::AudioBuffer<float> resampled(buf.getNumChannels(), newLen);

                juce::WindowedSincInterpolator resampler;
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    resampler.reset();
                    resampler.process(1.0 / ratio,
                                      buf.getReadPointer(ch),
                                      resampled.getWritePointer(ch),
                                      newLen);
                }
                req.deckProc->loadFromBuffer(resampled, req.targetSampleRate,
                                             req.file.getFileNameWithoutExtension());
            }
            else
            {
                req.deckProc->loadFromBuffer(buf, reader->sampleRate,
                                             req.file.getFileNameWithoutExtension());
            }

            deckInfo[req.deck].fileName        = req.file.getFileName();
            deckInfo[req.deck].lastLoadedFile  = req.file;
            deckInfo[req.deck].durationSeconds = reader->lengthInSamples
                                               / reader->sampleRate;
        }

        deckInfo[req.deck].loading.store(false);

        if (onLoadComplete)
        {
            const auto d = req.deck;
            juce::MessageManager::callAsync([this, d]{ onLoadComplete(d); });
        }
    }
}

std::vector<SampleManager::AutoPreset> SampleManager::getAutoPresets()
{
    return {
        { "Auto Beat",  0.0,  true  },
        { "1 Bar",      1.0,  true  },
        { "2 Bars",     2.0,  true  },
        { "4 Bars",     4.0,  true  },
        { "Half Bar",   0.5,  true  },
        { "1/4 Note",   0.25, true  },
        { "Loop 1/2",  -1.0,  false },  // -1 = first half of file
        { "Manual",    -2.0,  false },  // -2 = user-defined
    };
}

void SampleManager::applyAutoPreset([[maybe_unused]] DeckIndex deck, const AutoPreset& p,
                                    double hostBpm, DeckProcessor& deckProc)
{
    if (deckProc.getTotalSamples() <= 0) return;

    if (p.barCount < 0.0)
    {
        // Special cases
        if (p.barCount < -0.5) // Loop 1/2
            deckProc.setLoop(true, 0, deckProc.getTotalSamples() / 2);
        // Manual: do nothing
        return;
    }

    double bpm = hostBpm > 10.0 ? hostBpm :
                 (deckProc.getDetectedBpm() > 0.0 ? deckProc.getDetectedBpm() : 120.0);

    if (p.barCount == 0.0)
    {
        // Auto: use detected BPM to set loop to the nearest bar
        double detBpm = deckProc.getDetectedBpm();
        if (detBpm > 10.0)
            deckProc.setLoopLengthByBars(1.0, detBpm);
        else
            deckProc.setLoop(true, 0, deckProc.getTotalSamples());
    }
    else
    {
        deckProc.setLoopLengthByBars(p.barCount, bpm);
    }
}

bool SampleManager::isLoading(DeckIndex deck) const
{
    return deckInfo[deck].loading.load();
}

juce::String SampleManager::getFileName(DeckIndex deck) const
{
    return deckInfo[deck].fileName;
}

juce::File SampleManager::getLastLoadedFile(DeckIndex deck) const
{
    return deckInfo[deck].lastLoadedFile;
}

double SampleManager::getDurationSeconds(DeckIndex deck) const
{
    return deckInfo[deck].durationSeconds;
}
