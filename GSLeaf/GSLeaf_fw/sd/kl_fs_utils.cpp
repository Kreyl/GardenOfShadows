/*
 * kl_fs_common.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "kl_fs_utils.h"
#include "kl_lib.h"
#include "shell.h"

uint8_t TryOpenFileRead(const char *Filename, FIL *PFile) {
    FRESULT rslt = f_open(PFile, Filename, FA_READ);
    if(rslt == FR_OK) {
        // Check if zero file
        if(PFile->fsize == 0) {
            f_close(PFile);
            Printf("Empty file %S\r", Filename);
            return retvEmpty;
        }
        return retvOk;
    }
    else {
        if (rslt == FR_NO_FILE) Printf("%S: not found\r", Filename);
        else Printf("OpenFile error: %u\r", rslt);
        return retvFail;
    }
}

uint8_t CheckFileNotEmpty(FIL *PFile) {
    if(PFile->fsize == 0) {
        Printf("Empty file\r");
        return retvEmpty;
    }
    else return retvOk;
}

uint8_t TryRead(FIL *PFile, void *Ptr, uint32_t Sz) {
    uint32_t ReadSz=0;
    uint8_t r = f_read(PFile, Ptr, Sz, &ReadSz);
    return (r == FR_OK and ReadSz == Sz)? retvOk : retvFail;
}

//template <typename T>
//uint8_t TryRead(FIL *PFile, T *Ptr) {
//    uint32_t ReadSz=0;
//    uint8_t r = f_read(PFile, Ptr, sizeof(T), &ReadSz);
//    return (r == FR_OK and ReadSz == sizeof(T))? retvOk : retvFail;
//}

uint8_t ReadLine(FIL *PFile, char* S, uint32_t MaxLen) {
    uint32_t Len = 0, Rcv;
    char c, str[2];
    while(Len < MaxLen-1) {
        if(f_read(PFile, str, 1, &Rcv) != FR_OK) return retvFail;
        if(Rcv != 1) return retvEndOfFile;
        c = str[0];
        if(c == '\r' or c == '\n') {    // End of line
            *S = '\0';
            return retvOk;
        }
        else {
            *S++ = c;
            Len++;
        }
    } // while
    *S = '\0';
    return retvOk;
}
