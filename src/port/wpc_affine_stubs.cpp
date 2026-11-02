/*
 * wpc_affine_stubs.cpp - C++ stubs for the affine transform routines.
 *
 * The original affine implementation (src/affine.cpp, src/affine_rotations.cpp)
 * uses x87 FPU assembly that only compiles on 32-bit x86. On the native
 * x86-64 port these are replaced with minimal stubs. Real implementations
 * can be ported later as the game's rendering subsystem is rebuilt.
 */

#include <cstring>

#include "wpc_stub_types.h"

extern "C" {

void WpcAffine::identity()
{
    std::memset(m, 0, sizeof(m));
    m[0] = 1.0f;
    m[4] = 1.0f;
    m[8] = 1.0f;
    m[12] = 1.0f;
}

void WpcAffine::translate_local(const float *v)
{
    (void)v;
}

void WpcAffine::translate_world(const float *v)
{
    (void)v;
}

void WpcAffine::scale_basis(float x, float y, float z)
{
    (void)x;
    (void)y;
    (void)z;
}

void WpcAffine::scale_output(float x, float y, float z)
{
    (void)x;
    (void)y;
    (void)z;
}

void WpcAffine::point(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::point_duplicate(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::direction(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::direction_duplicate(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::transpose_direction(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::inverse_point(const float *v, float *out)
{
    (void)v;
    if (out) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

void WpcAffine::copy_from(const WpcAffine *other)
{
    (void)other;
    std::memset(m, 0, sizeof(m));
    m[0] = 1.0f;
    m[4] = 1.0f;
    m[8] = 1.0f;
    m[12] = 1.0f;
}

void WpcAffine::get_x(unsigned int *out)
{
    (void)out;
}

void WpcAffine::get_y(unsigned int *out)
{
    (void)out;
}

void WpcAffine::get_z(unsigned int *out)
{
    (void)out;
}

void WpcAffine::get_position(unsigned int *out)
{
    (void)out;
}

void WpcAffine::set_position(const unsigned int *v)
{
    (void)v;
}

void WpcAffine::compose(const WpcAffine *a, const WpcAffine *b)
{
    (void)a;
    (void)b;
    identity();
}

void WpcAffine::compose_linear(const WpcAffine *a, const WpcAffine *b)
{
    (void)a;
    (void)b;
    identity();
}

void WpcAffine::compose_rhs_transposed(const WpcAffine *a, const WpcAffine *b)
{
    (void)a;
    (void)b;
    identity();
}

void WpcAffine::inverse_scaled(const WpcAffine *other)
{
    (void)other;
    identity();
}

void WpcAffine::inverse_scaled_linear(const WpcAffine *other)
{
    (void)other;
    identity();
}

void WpcAffine::rotate_basis_x(float angle)
{
    (void)angle;
}

void WpcAffine::rotate_basis_y(float angle)
{
    (void)angle;
}

void WpcAffine::rotate_basis_z(float angle)
{
    (void)angle;
}

void WpcAffine::rotate_output_x(float angle)
{
    (void)angle;
}

void WpcAffine::rotate_output_y(float angle)
{
    (void)angle;
}

void WpcAffine::rotate_output_z(float angle)
{
    (void)angle;
}

void WpcAffine::rotation_x(float angle)
{
    (void)angle;
}

void WpcAffine::rotation_y(float angle)
{
    (void)angle;
}

void WpcAffine::rotation_z(float angle)
{
    (void)angle;
}

} /* extern "C" */
