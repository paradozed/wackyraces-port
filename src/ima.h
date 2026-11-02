#ifndef WPC_IMA_H
#define WPC_IMA_H

#pragma pack(push, 1)
struct WpcImaState {
    short predictor;
    signed char index;
};
#pragma pack(pop)
typedef char WpcImaStateMustBeThreeBytes[sizeof(WpcImaState) == 3 ? 1 : -1];
typedef char WpcImaIntMustBeFourBytes[sizeof(int) == 4 ? 1 : -1];

extern "C" {
extern const int wpc_ima_index_adjust[16];    // 0x00524ff0
extern const int wpc_ima_steps[89];          // 0x00525030

// 0x0040bdd0, cdecl, six stack arguments. Count is output frames: one sample
// per frame in mono, two in stereo. Nonpositive counts do not decode samples.
// Both initial indices must be in 0..88 (right is unused in mono). The caller
// must bound input/output storage; this recovered routine has no byte lengths.
void wpc_ima_decode(int stereo, const signed char *input, short *output, int frames,
                    WpcImaState *left, WpcImaState *right);
}

#endif
