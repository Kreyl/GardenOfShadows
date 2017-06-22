/*
 * kl_fs_common.h
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#pragma once

#include "ff.h"
#include "kl_lib.h"

uint8_t TryOpenFileRead(const char *Filename, FIL *PFile);
uint8_t CheckFileNotEmpty(FIL *PFile);
uint8_t TryRead(FIL *PFile, void *Ptr, uint32_t Sz);

//template <typename T>
//uint8_t TryRead(FIL *PFile, T *Ptr);

template <typename T>
uint8_t TryRead(FIL *PFile, T *Ptr) {
    uint32_t ReadSz=0;
    uint8_t r = f_read(PFile, Ptr, sizeof(T), &ReadSz);
    return (r == FR_OK and ReadSz == sizeof(T))? retvOk : retvFail;
}

//bool CurrentDirIsRoot();
//void ResetCurrDir

uint8_t ReadLine(FIL *PFile, char* S, uint32_t MaxLen);

