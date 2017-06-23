/*
 * AuPlayer.cpp
 *
 *  Created on: 4 θώνÿ 2017 γ.
 *      Author: Kreyl
 */

#include "kl_fs_utils.h"
#include "AuPlayer.h"
#include "kl_lib.h"
#include "shell.h"
#include "CS42L52.h"

//#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOB
#define DBG_PIN1    13
#define DBG1_SET()  PinSetHi(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinSetLo(DBG_GPIO1, DBG_PIN1)
#else
#define DBG1_SET()
#define DBG1_CLR()
#endif

extern CS42L52_t Audio;
extern AuPlayer_t Player;

struct WavFileInfo_t {
    uint32_t SampleRate;
    uint32_t BytesPerSecond;
    uint16_t FrameSz;
    uint32_t InitialDataChunkOffset, FinalDataChunkOffset;
    uint16_t BitsPerSample;
    uint16_t ChannelCnt;
    uint32_t FrameCnt;
    uint32_t Size;
    // Current state
    uint32_t NextDataChunkOffset, CurDataChunkFrameCnt;
    uint32_t ChunkSz;
};
static WavFileInfo_t Info;

// DMA Tx Completed IRQ
static thread_reference_t ThdRef = nullptr;
extern "C"
void DmaSAITxIrq(void *p, uint32_t flags) {
    chSysLockFromISR();
    Player.IHandleIrq();
    chSysUnlockFromISR();
}

void AuPlayer_t::IHandleIrq() {
    if(BufSz != 0) {
        PCurBuf = (PCurBuf == Buf1)? Buf2 : Buf1;
        Audio.TransmitBuf(PCurBuf, BufSz);
    }
    chThdResumeI(&ThdRef, MSG_OK);
}

// Thread
static THD_WORKING_AREA(waAudioThread, 2048);
__noreturn
static void AudioThread(void *arg) {
    chRegSetThreadName("Audio");
    Player.ITask();
}

__noreturn
void AuPlayer_t::ITask() {
    while(true) {
        chSysLock();
        chThdSuspendS(&ThdRef); // Wait IRQ
        chSysUnlock();
        if(BufSz != 0) {
            uint32_t *PBufToFill = (PCurBuf == Buf1)? Buf2 : Buf1;
            // Fill buff
            if(Info.ChunkSz != 0) {
                BufSz = MIN(Info.ChunkSz, FRAME_BUF_SZ);
                if(TryRead(&IFile, PBufToFill, BufSz) != retvOk) {
                    f_close(&IFile);
                    BufSz = 0;
                }
                Info.ChunkSz -= BufSz;
            }
            else BufSz = 0;
        } // if(BufSz != 0)
        else {  // End of file
            f_close(&IFile);
        }
    } // while true
}

void AuPlayer_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
#endif    // Init radioIC
    chThdCreateStatic(waAudioThread, sizeof(waAudioThread), NORMALPRIO, (tfunc_t)AudioThread, NULL);
}

uint8_t AuPlayer_t::Play(const char* AFileName) {
    // Try to open file
    if(OpenWav(AFileName) != retvOk) return retvFail;
    // Setup audio
    Audio.SetupParams((Info.ChannelCnt == 1)? Mono : Stereo, Info.SampleRate);

    // Fill both buffers
    char ChunkID[4] = {0, 0, 0, 0};

    if(f_lseek(&IFile, Info.NextDataChunkOffset) != FR_OK) goto end;
    if(TryRead(&IFile, ChunkID, 4) != retvOk) goto end;
    if(TryRead<uint32_t>(&IFile, &Info.ChunkSz) != retvOk) goto end;
    if(memcmp(ChunkID, "data", 4) == 0) {  // "data" found
        // Read first buf
        PCurBuf = Buf1;
        BufSz = MIN(Info.ChunkSz, FRAME_BUF_SZ);
        if(TryRead(&IFile, Buf1, BufSz) != retvOk) goto end;
        // Start transmission
        Audio.TransmitBuf(PCurBuf, BufSz);
        // Read second buf
        Info.ChunkSz -= BufSz;
        if(Info.ChunkSz != 0) {
            BufSz = MIN(Info.ChunkSz, FRAME_BUF_SZ);
            if(TryRead(&IFile, Buf2, BufSz) != retvOk) goto end;
            Info.ChunkSz -= BufSz;
        }
    }
    return retvOk;
    end:
    f_close(&IFile);
    return retvFail;
}

uint8_t AuPlayer_t::OpenWav(const char* AFileName) {
    // Open file
    if(TryOpenFileRead(AFileName, &IFile) != retvOk) return retvFail;
    uint32_t NextChunkOffset, ChunkSz;
    uint16_t uw16;

    // Check if starts with RIFF
    char ChunkID[4] = {0, 0, 0, 0};
    if(TryRead(&IFile, ChunkID, 4) != retvOk or (memcmp(ChunkID, "RIFF", 4) != 0)) goto end;
    // Get file size
    if(TryRead<uint32_t>(&IFile, &Info.Size) != retvOk) goto end;
    // Check riff type
    if(TryRead(&IFile, ChunkID, 4) != retvOk or (memcmp(ChunkID, "WAVE", 4) != 0)) goto end;
    // Check format
    if(TryRead(&IFile, ChunkID, 4) != retvOk or (memcmp(ChunkID, "fmt ", 4) != 0)) goto end;
    // Get offset of next chunk
    if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
    NextChunkOffset = IFile.fptr + ChunkSz;
    if((NextChunkOffset & 1) != 0) NextChunkOffset++;
    // Read format
    if(TryRead<uint16_t>(&IFile, &uw16) != retvOk) goto end;
    Printf("Fmt: %X\r", uw16);
    if(uw16 != 1) goto end; // PCM only
    // Channel cnt
    if(TryRead<uint16_t>(&IFile, &Info.ChannelCnt) != retvOk) goto end;
    if(Info.ChannelCnt > 2) goto end;
    // Sample rate
    if(TryRead<uint32_t>(&IFile, &Info.SampleRate) != retvOk) goto end;
    // Bytes per second
    if(TryRead<uint32_t>(&IFile, &Info.BytesPerSecond) != retvOk) goto end;
    // Block alignment == frame sz
    if(TryRead<uint16_t>(&IFile, &Info.FrameSz) != retvOk) goto end;
    // Bits per sample
    if(TryRead<uint16_t>(&IFile, &Info.BitsPerSample) != retvOk) goto end;

    Printf("Sz: %u\r", Info.Size);
    Printf("NextCh: %u\r", NextChunkOffset);
    Printf("ChnlCnt: %u\r", Info.ChannelCnt);
    Printf("SmplRt: %u\r", Info.SampleRate);
    Printf("BytesPerSecond: %u\r", Info.BytesPerSecond);
    Printf("BlkAlgn: %u\r", Info.FrameSz);
    Printf("BitsPerSample: %u\r", Info.BitsPerSample);

    // Find data chunk
    while(true) {
        // Move to data chunk
        if(f_lseek(&IFile, NextChunkOffset) != FR_OK) goto end;
        // Read chunk ID & sz
        if(TryRead(&IFile, ChunkID, 4) != retvOk) goto end;
        if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;

        if(memcmp(ChunkID, "data", 4) == 0) {  // "data" found
            // Calc offsets
            Info.InitialDataChunkOffset = NextChunkOffset;
            if((Info.InitialDataChunkOffset & 1) != 0) Info.InitialDataChunkOffset++;
            Info.FinalDataChunkOffset = IFile.fptr + ChunkSz;
            if((Info.FinalDataChunkOffset & 1) != 0) Info.FinalDataChunkOffset++;
            Printf("DataStart: %u; DataEnd: %u\r", Info.InitialDataChunkOffset, Info.FinalDataChunkOffset);
            break;  // Data found
        }

        if(memcmp(ChunkID, "LIST", 4) == 0) {  // "LIST" found
            if(TryRead(&IFile, ChunkID, 4) != retvOk) goto end;
            if(memcmp(ChunkID, "wavl", 4) == 0) {
                // Calc offsets
                Info.InitialDataChunkOffset = IFile.fptr;   // Here is "data", definitely
                if((Info.InitialDataChunkOffset & 1) != 0) Info.InitialDataChunkOffset++;
                Info.FinalDataChunkOffset = IFile.fptr + ChunkSz;
                if((Info.FinalDataChunkOffset & 1) != 0) Info.FinalDataChunkOffset++;
                Printf("DataStart: %u; DataEnd: %u\r", Info.InitialDataChunkOffset, Info.FinalDataChunkOffset);
                break;
            }
            else {  // Not wavl, so skip LIST alltogether
                ChunkSz -= 4;   // take in account 4 bytes of chunk type that just was read
            }
        }

        // Proceed with data search
        NextChunkOffset = IFile.fptr + ChunkSz;
        if(NextChunkOffset & 1) NextChunkOffset++;
    } // while true
    // Offsets ready
    Rewind();
    return retvOk;

    end:
    f_close(&IFile);
    return retvFail;
}

void AuPlayer_t::Rewind() {
    Info.NextDataChunkOffset = Info.InitialDataChunkOffset;
    Info.CurDataChunkFrameCnt = 0;
}
