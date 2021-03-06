#include "compressQuat.h"
#include "math.h"


#define cq_Pi_2 1.5707963267948966192313216916398f
#define cq_Sqrt2 1.4142135623730950488016887242097f
#define cq_NormalizationToleranceSq 1e-6f

static inline int32_t clamp(int32_t _a, int32_t _x, int32_t _b) {
    int32_t min = _x < _b ? _x : _b;
    return (min < _a ? _a : min);
}

void cq_identity(struct cqQuaternion *_q) {
    _q->x = 0;
    _q->y = 0;
    _q->z = 0;
    _q->w = 1.0f;
}


void cq_mult(const struct cqQuaternion *_a, const struct cqQuaternion *_b, struct cqQuaternion *out) {
    out->x = _a->w * _b->x + _a->x * _b->w + _a->y * _b->z - _a->z * _b->y;
    out->y = _a->w * _b->y + _a->y * _b->w + _a->z * _b->x - _a->x * _b->z;
    out->z = _a->w * _b->z + _a->z * _b->w + _a->x * _b->y - _a->y * _b->x;
    out->w = _a->w * _b->w - _a->x * _b->x - _a->y * _b->y - _a->z * _b->z;
}

void cq_conjugate(const struct cqQuaternion *_a, struct cqQuaternion *out) {
    out->x = -_a->x;
    out->y = -_a->y;
    out->z = -_a->z;
    out->w =  _a->w;
}

void cq_normalize(struct cqQuaternion *_q) {
    const float sq_len = _q->x * _q->x + _q->y * _q->y + _q->z * _q->z + _q->w * _q->w;
    const float inv_len = 1.f / sqrtf(sq_len);
    _q->x = _q->x * inv_len;
    _q->y = _q->y * inv_len;
    _q->z = _q->z * inv_len;
    _q->w = _q->w * inv_len;
}

cqBOOL cq_isNormalize(const struct cqQuaternion *_q) {
    const float sq_len = _q->x * _q->x + _q->y * _q->y + _q->z * _q->z + _q->w * _q->w;
    return fabsf(sq_len - 1.f) < cq_NormalizationToleranceSq ? cqTRUE : cqFALSE;
}

cqBOOL cq_compare(const struct cqQuaternion *_a, const struct cqQuaternion *_b, float _tolerance) {
    // Computes w component of a-1 * b.
    const float diff_w = _a->x * _b->x + _a->y * _b->y + _a->z * _b->z + _a->w * _b->w;
    // Converts w back to an angle.
    const float angle = 2.f * acosf(fminf(fabsf(diff_w), 1.f));
    return (fabsf(angle) <= _tolerance) ? cqTRUE : cqFALSE;
}


void cq_fromEuler(const struct cqFloat3 *_euler, struct cqQuaternion *out) {
    const struct cqFloat3 half_euler = {_euler->x * .5f, _euler->y * .5f, _euler->z * .5f};
    const float c1 = cosf(half_euler.x);
    const float s1 = sinf(half_euler.x);
    const float c2 = cosf(half_euler.y);
    const float s2 = sinf(half_euler.y);
    const float c3 = cosf(half_euler.z);
    const float s3 = sinf(half_euler.z);
    const float c1c2 = c1 * c2;
    const float s1s2 = s1 * s2;

    out->x = c1c2 * s3 + s1s2 * c3;
    out->y = s1 * c2 * c3 + c1 * s2 * s3;
    out->z = c1 * s2 * c3 - s1 * c2 * s3;
    out->w = c1c2 * c3 - s1s2 * s3;
}

void cq_toEuler(const struct cqQuaternion *_q, struct cqFloat3 *_euler) {
    const float sqw = _q->w * _q->w;
    const float sqx = _q->x * _q->x;
    const float sqy = _q->y * _q->y;
    const float sqz = _q->z * _q->z;
    // If normalized is one, otherwise is correction factor.
    const float unit = sqx + sqy + sqz + sqw;
    const float test = _q->x * _q->y + _q->z * _q->w;

    if (test > .499f * unit) {  // Singularity at north pole
        _euler->x = 2.f * atan2f(_q->x, _q->w);
        _euler->y = cq_Pi_2;
        _euler->z = 0;
    } else if (test < -.499f * unit) {  // Singularity at south pole
        _euler->x = -2 * atan2f(_q->x, _q->w);
        _euler->y = -cq_Pi_2;
        _euler->z = 0;
    } else {
        _euler->x = atan2f(2.f * _q->y * _q->w - 2.f * _q->x * _q->z,
                           sqx - sqy - sqz + sqw);
        _euler->y = asinf(2.f * test / unit);
        _euler->z = atan2f(2.f * _q->x * _q->w - 2.f * _q->y * _q->z,
                           -sqx + sqy - sqz + sqw);
    }
}


void compress_pack(const struct cqQuaternion *_src, struct CompressQuat *out) {
    const float quat[4] = {_src->x, _src->y, _src->z, _src->w};
    size_t largest = 0;
    float max = fabsf(quat[largest]);

    for (size_t i = 0; i < 4; ++i)
    {
        if (fabsf(quat[i]) > max)
        {
            max = fabsf(quat[i]);
            largest = i;
        }
    }

    out->largest = (uint16_t) (largest & 0x3);
    // Stores the sign of the largest component.
    out->sign = (uint16_t)(quat[largest] < 0.f ? 1 : 0);

    // 2^14 = 16384
    const uint16_t maxAbsRage = 16384;
    const float Float2Int = 16384.f * cq_Sqrt2;
    const int Mapping[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
    const int *map = Mapping[largest];

    const int32_t a = (int32_t)(floorf(quat[map[0]] * Float2Int + .5f));
    const int32_t b = (int32_t)(floorf(quat[map[1]] * Float2Int + .5f));
    const int32_t c = (int32_t)(floorf(quat[map[2]] * Float2Int + .5f));

    const int16_t l_a = (const int16_t) (clamp(-maxAbsRage, a, maxAbsRage));
    const int16_t l_b = (const int16_t) (clamp(-maxAbsRage, b, maxAbsRage));
    const int16_t l_c = (const int16_t) (clamp(-maxAbsRage, c, maxAbsRage));

    out->a = (uint16_t) (l_a & 0x3FFF);
    out->b = (uint16_t) (l_b & 0x3FFF);
    out->c = (uint16_t) (l_c & 0x3FFF);

    out->sign_a = (uint16_t)(l_a < 0 ? 1 : 0);
    out->sign_b = (uint16_t)(l_b < 0 ? 1 : 0);
    out->sign_c = (uint16_t)(l_c < 0 ? 1 : 0);
}

void uncompress_pack(const struct CompressQuat *_src, struct cqQuaternion *out) {
    const float Int2Float = 1.f / (16384.f * cq_Sqrt2);
    const int Mapping[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};

    size_t largest = _src->largest;
    float quat[4] = {0, 0, 0, 0};
    const int *map = Mapping[largest];

    quat[largest] = _src->sign > 0 ? -1.0f : 1.0f;

    quat[map[0]] =  _src->sign_a > 0 ? -(_src->a * Int2Float) : (_src->a * Int2Float);
    quat[map[1]] =  _src->sign_b > 0 ? -(_src->b * Int2Float) : (_src->b * Int2Float);
    quat[map[2]] =  _src->sign_c > 0 ? -(_src->c * Int2Float) : (_src->c * Int2Float);

    out->x =  quat[0];
    out->y =  quat[1];
    out->z =  quat[2];
    out->w =  quat[3];
}

void uncompress_packN(const struct CompressQuat *_src, struct cqQuaternion *out) {
    uncompress_pack(_src, out);
    cq_normalize(out);
}



