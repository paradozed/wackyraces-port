/*
 * wpc_vector_stubs.cpp - C++ stubs for the vector math routines.
 *
 * The original vector implementation (src/vector.cpp) uses x87 FPU assembly
 * that only compiles on 32-bit x86. On the native x86-64 port these are
 * replaced with minimal stubs. Real implementations can be ported later.
 */

#include <cmath>

#include "wpc_stub_types.h"

extern "C" {

int wpc_vectors_within(const WpcVector *a, const WpcVector *b, float radius)
{
    (void)a;
    (void)b;
    (void)radius;
    return 0;
}

} /* extern "C" */

void WpcVector::normalize(float *length)
{
    (void)length;
}

void WpcVector::normalized_copy(const WpcVector *source)
{
    (void)source;
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

void WpcVector::cross(const WpcVector *a, const WpcVector *b)
{
    (void)a;
    (void)b;
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

void WpcVector::angles(float *pitch, float *yaw)
{
    (void)pitch;
    (void)yaw;
}

void WpcVector::from_angles(float pitch, float yaw)
{
    (void)pitch;
    (void)yaw;
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

void WpcVector::rotate_x(float angle)
{
    (void)angle;
}

void WpcVector::rotate_y(float angle)
{
    (void)angle;
}

void WpcVector::rotate_z(float angle)
{
    (void)angle;
}
