#pragma once
#include <atomic>
#include <cstring>

//==============================================================================
// Lock-free stereo circular buffer for vinyl time-manipulation.
// Size is always a power of two → index wrapping via bitwise AND (no %).
// All allocation happens in resize() which must only be called from
// prepareToPlay(), never from processBlock().
//==============================================================================
class CircularBuffer
{
public:
    // Maximum pre-allocated size: 2^18 = 262144 samples ≈ 5.9 s @ 44.1 kHz
    static constexpr int MAX_SIZE = 1 << 18;

    CircularBuffer()
    {
        std::memset(bufL, 0, sizeof(bufL));
        std::memset(bufR, 0, sizeof(bufR));
    }

    // Call from prepareToPlay() only. newSize will be rounded up to next
    // power-of-two and clamped to MAX_SIZE.
    void resize(int newSize)
    {
        // Round up to next power of two
        int sz = 1;
        while (sz < newSize && sz < MAX_SIZE) sz <<= 1;
        actualSize = sz;
        mask       = sz - 1;
        writePos.store(0);
        std::memset(bufL, 0, sizeof(float) * (size_t)sz);
        std::memset(bufR, 0, sizeof(float) * (size_t)sz);
    }

    void reset()
    {
        writePos.store(0);
        std::memset(bufL, 0, sizeof(float) * (size_t)actualSize);
        std::memset(bufR, 0, sizeof(float) * (size_t)actualSize);
    }

    // Write one stereo sample and advance write pointer.
    inline void write(float l, float r) noexcept
    {
        int w = writePos.load(std::memory_order_relaxed);
        bufL[w & mask] = l;
        bufR[w & mask] = r;
        writePos.store(w + 1, std::memory_order_release);
    }

    // Read a sample at (writePos - offsetFromWrite) without moving any pointer.
    // offsetFromWrite: 0 = most recent, actualSize-1 = oldest.
    inline void readAt(int offsetFromWrite, float& outL, float& outR) const noexcept
    {
        int w   = writePos.load(std::memory_order_acquire);
        int idx = (w - offsetFromWrite) & mask;
        outL = bufL[idx];
        outR = bufR[idx];
    }

    // Read a single channel sample at a fractional offset (for Hermite use).
    inline float readL(int offsetFromWrite) const noexcept
    {
        return bufL[(writePos.load(std::memory_order_acquire) - offsetFromWrite) & mask];
    }
    inline float readR(int offsetFromWrite) const noexcept
    {
        return bufR[(writePos.load(std::memory_order_acquire) - offsetFromWrite) & mask];
    }

    int getActualSize() const noexcept { return actualSize; }
    int getWritePos()   const noexcept { return writePos.load(std::memory_order_acquire); }
    int getMask()       const noexcept { return mask; }

    // Total samples written since last reset (capped at actualSize for safety check).
    int getTotalWritten() const noexcept
    {
        return std::min(writePos.load(std::memory_order_acquire), actualSize);
    }

private:
    alignas(64) float bufL[MAX_SIZE] = {};
    alignas(64) float bufR[MAX_SIZE] = {};
    std::atomic<int>  writePos { 0 };
    int  actualSize = MAX_SIZE;
    int  mask       = MAX_SIZE - 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularBuffer)
};
