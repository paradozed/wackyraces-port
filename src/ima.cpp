#include "ima.h"

#if defined(_MSC_VER)
#define WPC_INLINE __forceinline
#else
#define WPC_INLINE inline __attribute__((always_inline))
#endif

static WPC_INLINE int decode_nibble(int nibble, int &predictor, int &index, int &step)
{
    index += wpc_ima_index_adjust[nibble];
    if (index < 0)
        index = 0;
    else if (index > 88)
        index = 88;

    int difference = step >> 3;
    if (nibble & 4)
        difference += step;
    if (nibble & 2)
        difference += step >> 1;
    if (nibble & 1)
        difference += step >> 2;
    if (nibble & 8)
        predictor -= difference;
    else
        predictor += difference;

    if (predictor > 32767)
        predictor = 32767;
    else if (predictor < -32768)
        predictor = -32768;
    step = wpc_ima_steps[index];
    return predictor;
}

static WPC_INLINE int next_nibble(const signed char *&input, int &byte, int &low)
{
    int value;
    if (low)
        value = byte;
    else {
        byte = *input++;
        value = byte >> 4;
    }
    low = !low;
    return value & 15;
}

extern "C" void wpc_ima_decode(int stereo, const signed char *input, short *output, int frames,
                                WpcImaState *left, WpcImaState *right)
{
    int predictor_left = left->predictor;
    int index_left = left->index;
    int step_left = wpc_ima_steps[index_left];
    int predictor_right = 0;
    int index_right = 0;
    int step_right = 0;
    if (stereo) {
        predictor_right = right->predictor;
        index_right = right->index;
        step_right = wpc_ima_steps[index_right];
    }

    int byte = 0;
    int low = 0;
    if (frames > 0) {
        do {
            int nibble = next_nibble(input, byte, low);
            *output++ = (short)decode_nibble(nibble, predictor_left, index_left, step_left);
            if (stereo) {
                nibble = next_nibble(input, byte, low);
                *output++ = (short)decode_nibble(nibble, predictor_right, index_right, step_right);
            }
        } while (--frames);
    }

    left->predictor = (short)predictor_left;
    left->index = (signed char)index_left;
    if (stereo) {
        right->predictor = (short)predictor_right;
        right->index = (signed char)index_right;
    }
}
