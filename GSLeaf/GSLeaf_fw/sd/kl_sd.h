/*
 * sd.h
 *
 *  Created on: 13.02.2013
 *      Author: kreyl
 */

#pragma once

#include "ff.h"
#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "uart.h"

#define MAX_NAME_LEN    128

// See SDIO clock divider in halconf.h

// =========================== ini file operations =============================
/*
 * ini file has the following structure:
 *
 * # This is Comment: comment uses either '#' or ';' symbol
 * ; This is Comment too
 *
 * [Section]    ; This is name of section
 * Count=6      ; This is key with value of int32
 * Volume=-1    ; int32
 * SoundFileName=phrase01.wav   ; string
 *
 * [Section2]
 * Key1=1
 * ...
 */

#define SD_STRING_SZ    256 // for operations with strings

class sd_t {
private:
    FATFS SDC_FS;
    char LongFileName[MAX_NAME_LEN];
    FILINFO FileInfo;
    FIL IFile;  // Open and close inside one function, do not leave it opened
    char IStr[SD_STRING_SZ];
public:
    DIR Directory;
    bool IsReady;
    void Init();
    // ini file operations
    uint8_t iniReadString(const char *AFileName, const char *ASection, const char *AKey, char **PPOutput);
    template <typename T>
    uint8_t iniRead(const char *AFileName, const char *ASection, const char *AKey, T *POutput) {
        char *S = nullptr;
        if(iniReadString(AFileName, ASection, AKey, &S) == retvOk) {
            int32_t tmp = strtol(S, NULL, 10);
            *POutput = (T)tmp;
            return retvOk;
        }
        else return retvFail;
    }
};

extern sd_t SD;
