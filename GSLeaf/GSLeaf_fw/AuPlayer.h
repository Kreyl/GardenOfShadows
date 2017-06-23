/*
 * AuPlayer.h
 *
 *  Created on: 4 θώνÿ 2017 γ.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ff.h"

#define FRAME_BUF_SZ        16384

class AuPlayer_t {
private:
    FIL IFile;
    uint32_t Buf[(FRAME_BUF_SZ/4)], *PCurBuf;
    uint8_t OpenWav(const char* AFileName);
    uint32_t BytesToTransmit;
public:
    void Init();
    uint8_t Play(const char* AFileName);
    void Stop();
    void Rewind();
    void ITask();
};
