/*
 * AuPlayer.h
 *
 *  Created on: 4 θώνÿ 2017 γ.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ff.h"

class AuPlayer_t {
private:
    FIL IFile;
    uint8_t OpenWav(const char* AFileName);
public:
    void Init();

    uint8_t Play(const char* AFileName);

    void Rewind();
};
