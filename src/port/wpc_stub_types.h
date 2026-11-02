/*
 * wpc_stub_types.h - Minimal type declarations for the i386-only stubs.
 *
 * The original types (WpcVector, WpcAffine) were defined in headers that
 * only compile on 32-bit x86. On the native x86-64 port the matching
 * routines are replaced with no-op stubs, so only the type layouts needed
 * by those stubs are declared here.
 */

#ifndef WPC_STUB_TYPES_H
#define WPC_STUB_TYPES_H

class WpcVector {
public:
    float v[3];

    void normalize(float *length);
    void normalized_copy(const WpcVector *source);
    void cross(const WpcVector *a, const WpcVector *b);
    void angles(float *pitch, float *yaw);
    void from_angles(float pitch, float yaw);
    void rotate_x(float angle);
    void rotate_y(float angle);
    void rotate_z(float angle);
};

extern "C" int wpc_vectors_within(const WpcVector *a, const WpcVector *b, float radius);

class WpcAffine {
public:
    float m[16];

    void identity();
    void translate_local(const float *v);
    void translate_world(const float *v);
    void scale_basis(float x, float y, float z);
    void scale_output(float x, float y, float z);
    void point(const float *v, float *out);
    void point_duplicate(const float *v, float *out);
    void direction(const float *v, float *out);
    void direction_duplicate(const float *v, float *out);
    void transpose_direction(const float *v, float *out);
    void inverse_point(const float *v, float *out);
    void copy_from(const WpcAffine *other);
    void get_x(unsigned int *out);
    void get_y(unsigned int *out);
    void get_z(unsigned int *out);
    void get_position(unsigned int *out);
    void set_position(const unsigned int *v);
    void compose(const WpcAffine *a, const WpcAffine *b);
    void compose_linear(const WpcAffine *a, const WpcAffine *b);
    void compose_rhs_transposed(const WpcAffine *a, const WpcAffine *b);
    void inverse_scaled(const WpcAffine *other);
    void inverse_scaled_linear(const WpcAffine *other);
    void rotate_basis_x(float angle);
    void rotate_basis_y(float angle);
    void rotate_basis_z(float angle);
    void rotate_output_x(float angle);
    void rotate_output_y(float angle);
    void rotate_output_z(float angle);
    void rotation_x(float angle);
    void rotation_y(float angle);
    void rotation_z(float angle);
};

#endif /* WPC_STUB_TYPES_H */
