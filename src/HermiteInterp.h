#pragma once

//==============================================================================
// 6-point, 5th-order Hermite interpolation.
// Coefficients from the specification table; evaluated via Horner's scheme
// to minimise multiply operations.
//
// Parameters:
//   ym2, ym1, y0, y1, y2, y3  — six consecutive samples (y[-2]..y[3])
//   f                          — fractional position in [0, 1)
//
// Reference: "Polynomial Interpolators for High-Quality Resampling of
// Oversampled Audio" (Olli Niemitalo, 2001), Table 4, row "6-point 5th-order".
//==============================================================================
namespace Hermite
{
    // Pre-computed fractional constants (resolved at compile time).
    static constexpr double C_1_24  = 1.0 /  24.0;
    static constexpr double C_5_24  = 5.0 /  24.0;
    static constexpr double C_7_24  = 7.0 /  24.0;
    static constexpr double C_1_12  = 1.0 /  12.0;
    static constexpr double C_5_12  = 5.0 /  12.0;
    static constexpr double C_7_12  = 7.0 /  12.0;
    static constexpr double C_13_12 = 13.0 / 12.0;
    static constexpr double C_25_12 = 25.0 / 12.0;
    static constexpr double C_11_24 = 11.0 / 24.0;
    static constexpr double C_1_8   = 1.0 /   8.0;
    static constexpr double C_2_3   = 2.0 /   3.0;

    inline float interpolate(float ym2, float ym1, float y0,
                             float y1,  float y2,  float y3,
                             float f) noexcept
    {
        // Coefficients per the specification:
        // c0 = y0
        // c1 = 1/12*(y[-2] - y[2]) + 2/3*(y[1] - y[-1])
        // c2 = 13/12*y[-1] - 25/12*y[0] + 2/3*y[1] - 11/24*y[2] + 1/24*y[3] - 1/8*y[-2]
        // c3 = 5/12*y[0] - 7/12*y[1] + 7/24*y[2] - 1/24*(y[-2]+y[-1]+y[3])
        // c4 = 1/8*y[-2] - 7/12*y[-1] + 13/12*y[0] - y[1] + 11/24*y[2] - 1/12*y[3]
        // c5 = 1/24*(y[3]-y[-2]) + 5/24*(y[-1]-y[2]) + 5/12*(y[1]-y[0])

        const double c0 = y0;
        const double c1 = C_1_12 * (ym2 - y2) + C_2_3  * (y1 - ym1);
        const double c2 = C_13_12 * ym1 - C_25_12 * y0  + C_2_3 * y1
                        - C_11_24 * y2  + C_1_24  * y3  - C_1_8  * ym2;
        const double c3 = C_5_12 * y0  - C_7_12 * y1  + C_7_24 * y2
                        - C_1_24 * (ym2 + ym1 + y3);
        const double c4 = C_1_8  * ym2 - C_7_12 * ym1 + C_13_12 * y0
                        - y1 + C_11_24 * y2 - C_1_12 * y3;
        const double c5 = C_1_24 * (y3 - ym2) + C_5_24 * (ym1 - y2)
                        + C_5_12 * (y1 - y0);

        // Horner's scheme: (((((c5*f+c4)*f+c3)*f+c2)*f+c1)*f+c0)
        const double fd = static_cast<double>(f);
        return static_cast<float>(
            (((((c5 * fd + c4) * fd + c3) * fd + c2) * fd + c1) * fd + c0)
        );
    }
} // namespace Hermite
