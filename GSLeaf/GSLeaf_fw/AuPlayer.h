/*
 * AuPlayer.h
 *
 *  Created on: 4 θώνÿ 2017 γ.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ff.h"

struct WavFileInfo_t {
    uint32_t SampleRate;
    uint32_t BytesPerSecond;
    uint16_t FrameSz;
    uint32_t InitialDataChunkOffset, FinalDataChunkOffset;
    uint16_t BitsPerSample;
    uint16_t ChannelCnt;
    uint32_t FrameCnt;
    uint32_t Size;

};

class AuPlayer_t {
private:
    FIL IFile;
    uint32_t NextDataChunkOffset, CurDataChunkFrames;
    uint8_t Open(const char* AFileName);
public:
    void Init();
    WavFileInfo_t Info;
    uint8_t Play(const char* AFileName);

    void Rewind() {
        NextDataChunkOffset = Info.InitialDataChunkOffset;
        CurDataChunkFrames = 0;
    }

    enum class Format : unsigned int {
        Pcm = 1
    };
};
