#pragma once
#include <cmath>
#include <algorithm>

//==============================================================================
// Crossfader gain computation with adjustable curve sharpness.
//
// x    — crossfader position in [0.0, 1.0]
//          0.0 = full Deck A,  0.5 = centre,  1.0 = full Deck B
//
// mode — CrossfaderMode enum (see below)
// n    — sharpness integer [0..5] used in scratch-cut mode
//
// Constant-Power (n=0):
//   gA = cos(π/2 * x),  gB = sin(π/2 * x)
//   → gA²+gB²=1 everywhere, 0 dB at centre
//
// Scratch-Cut (n>0):
//   gA = cos(π/4 * ((2*(1-x)-1)^(2n+1) + 1))
//   gB = cos(π/4 * ((2*x   -1)^(2n+1) + 1))
//   Larger n → sharper cut at edges (crab/flare technique).
//
// Linear:
//   gA = 1-x, gB = x  (reference only; causes -3 dB dip at centre)
//==============================================================================
namespace Crossfader
{
    enum class Mode { ConstantPower, ScratchCut, Linear };

    struct Gains { float gA = 1.f, gB = 0.f; };

    inline Gains compute(float x, Mode mode, int n = 0) noexcept
    {
        x = std::clamp(x, 0.0f, 1.0f);
        constexpr float PI2 = static_cast<float>(1.5707963267948966);  // π/2
        constexpr float PI4 = static_cast<float>(0.7853981633974483);  // π/4

        switch (mode)
        {
            case Mode::ConstantPower:
            {
                return { std::cos(PI2 * x), std::sin(PI2 * x) };
            }

            case Mode::ScratchCut:
            {
                // n is clamped to [0,5]; exponent must be odd: 2n+1
                int exp = std::max(0, 2 * n + 1);

                // Signed power: preserves sign of base
                auto signedPow = [](float base, int e) -> float {
                    float result = 1.0f;
                    float absB   = std::abs(base);
                    for (int i = 0; i < e; ++i) result *= absB;
                    return (base >= 0.f) ? result : -result;
                };

                float tA = 2.0f * (1.0f - x) - 1.0f;  // maps [0,1] → [1,-1]
                float tB = 2.0f * x - 1.0f;            // maps [0,1] → [-1,1]
                float gA = std::cos(PI4 * (signedPow(tA, exp) + 1.0f));
                float gB = std::cos(PI4 * (signedPow(tB, exp) + 1.0f));
                return { gA, gB };
            }

            case Mode::Linear:
            default:
                return { 1.0f - x, x };
        }
    }

    // Convenience: compute for a given x position in [0,1] and curve index
    // curveIndex: 0 = ConstantPower, 1-5 = ScratchCut with n=curveIndex, 6 = Linear
    inline Gains computeByIndex(float x, int curveIndex) noexcept
    {
        if (curveIndex == 0)  return compute(x, Mode::ConstantPower);
        if (curveIndex == 6)  return compute(x, Mode::Linear);
        return compute(x, Mode::ScratchCut, std::clamp(curveIndex, 1, 5));
    }
} // namespace Crossfader
