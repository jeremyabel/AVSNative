#pragma once

#include "engine/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace avs
{
// Rotation about an axis (1=X, 2=Y, 3=Z) by `deg` degrees, written into m (16 floats).
inline void MatRot(float* m, int axis, float deg)
{
    const float r = deg * avs::Pi / 180.0f;
    std::fill(m, m + 16, 0.0f);
    m[(axis - 1) * 4 + (axis - 1)] = 1.0f;
    m[15] = 1.0f;
    const int m1 = axis % 3;
    const int m2 = (m1 + 1) % 3;
    const float c = std::cos(r), s = std::sin(r);
    m[m1 * 4 + m1] = c;
    m[m1 * 4 + m2] = s;
    m[m2 * 4 + m2] = c;
    m[m2 * 4 + m1] = -s;
}

// Translation by (x,y,z), written into m (16 floats).
inline void MatTrans(float* m, float x, float y, float z)
{
    std::fill(m, m + 16, 0.0f);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    m[3] = x; m[7] = y; m[11] = z;
}

// dest = src x dest_old (in place).
inline void MatMul(float* Dest, const float* Src)
{
    float Temp[16];
    std::memcpy(Temp, Dest, sizeof(Temp));
    for (int i = 0; i < 16; i += 4)
    {
        Dest[i + 0] = Src[i]*Temp[0] + Src[i+1]*Temp[4] + Src[i+2]*Temp[8]  + Src[i+3]*Temp[12];
        Dest[i + 1] = Src[i]*Temp[1] + Src[i+1]*Temp[5] + Src[i+2]*Temp[9]  + Src[i+3]*Temp[13];
        Dest[i + 2] = Src[i]*Temp[2] + Src[i+1]*Temp[6] + Src[i+2]*Temp[10] + Src[i+3]*Temp[14];
        Dest[i + 3] = Src[i]*Temp[3] + Src[i+1]*Temp[7] + Src[i+2]*Temp[11] + Src[i+3]*Temp[15];
    }
}
} // namespace avs
