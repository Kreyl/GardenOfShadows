/*
 * AuPlayer.h
 *
 *  Created on: 4 ���� 2017 �.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ff.h"

#define FRAME_BUF_SZ        8192

class AuPlayer_t {
private:
    FIL IFile;
    uint32_t Buf1[(FRAME_BUF_SZ/4)], Buf2[(FRAME_BUF_SZ/4)], *PCurBuf, BufSz;
    uint8_t OpenWav(const char* AFileName);
//    uint8_t ReadNextWavFrames

public:
    void Init();

    uint8_t Play(const char* AFileName);

    void Rewind();
    void ITask();
};
