#include <inttypes.h>

struct RawAudio_t {
    uint32_t SampleRate;
    uint16_t ChnlCnt;
    uint32_t FrameCnt;
    const int16_t *PData;
};

extern const struct RawAudio_t alive;

