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
    chThdResumeI(&ThdRef, (msg_t)flags);
    chSysUnlockFromISR();
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
        uint32_t flags = (uint32_t)chThdSuspendS(&ThdRef); // Wait IRQ
        chSysUnlock();
        Printf("\rSFlags: %X\r", flags);
        Printf("chsz: %u\r", Info.ChunkSz);
        Printf("Bttx: %u\r", BytesToTransmit);

        // If nothing to transmit, stop
        if(BytesToTransmit == 0) {
            f_close(&IFile);
            Audio.Stop();
            Printf("Stop\r");
        }
        else {
            BytesToTransmit = MIN(Info.ChunkSz, FRAME_BUF_SZ/2);
            Printf("Bttx: %u\r", BytesToTransmit);
            if(BytesToTransmit != 0) {
                uint32_t *PBufToFill;
                // If this is half tranfer, fill first half of buffer
                if(flags & DMA_ISR_HTIF1) PBufToFill = Buf;
                // Otherwise, fill last half of buffer
                else PBufToFill = &Buf[(FRAME_BUF_SZ/2)];

                if(TryRead(&IFile, PBufToFill, BytesToTransmit) != retvOk) Info.ChunkSz = 0;
                else {
                    Printf("a\r");
                    Info.ChunkSz -= BytesToTransmit;
                    // Fill what left with zeroes
                    if(BytesToTransmit < (FRAME_BUF_SZ/2)) {
                        memset(&PBufToFill[BytesToTransmit], 0, ((FRAME_BUF_SZ/2) - BytesToTransmit));
                        if(flags & DMA_ISR_HTIF1) memset(&PBufToFill[(FRAME_BUF_SZ/2)], 0, (FRAME_BUF_SZ/2));
                    }
                }
            }
        }
    } // while true
}

void AuPlayer_t::Init() {
    chThdCreateStatic(waAudioThread, sizeof(waAudioThread), NORMALPRIO, (tfunc_t)AudioThread, NULL);
}

uint8_t AuPlayer_t::Play(const char* AFileName) {
    // Try to open file (read params and get pointer to data)
    if(OpenWav(AFileName) != retvOk) return retvFail;
    // Setup audio
    Audio.SetupParams((Info.ChannelCnt == 1)? Mono : Stereo, Info.SampleRate);

    // Fill both buffers
    char ChunkID[4] = {0, 0, 0, 0};

    if(f_lseek(&IFile, Info.NextDataChunkOffset) != FR_OK) goto end;
    if(TryRead(&IFile, ChunkID, 4) != retvOk) goto end;
    if(TryRead<uint32_t>(&IFile, &Info.ChunkSz) != retvOk) goto end;
    if(memcmp(ChunkID, "data", 4) == 0) {  // "data" found
        // Read to buf
        BytesToTransmit = MIN(Info.ChunkSz, FRAME_BUF_SZ);
        if(TryRead(&IFile, Buf, BytesToTransmit) != retvOk) goto end;
        Info.ChunkSz -= BytesToTransmit;
        // Start transmission
        Audio.TransmitBuf(Buf, BytesToTransmit);
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
