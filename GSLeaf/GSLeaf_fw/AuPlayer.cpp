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
    uint32_t NextDataChunkOffset, CurDataChunkFrames;
};
static WavFileInfo_t Info;


void AuPlayer_t::Init() {

}

uint8_t AuPlayer_t::Play(const char* AFileName) {
    return OpenWav(AFileName);
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
    Printf("Sz: %u\r", Info.Size);
    // Check riff type
    if(TryRead(&IFile, ChunkID, 4) != retvOk or (memcmp(ChunkID, "WAVE", 4) != 0)) goto end;
    // Check format
    if(TryRead(&IFile, ChunkID, 4) != retvOk or (memcmp(ChunkID, "fmt ", 4) != 0)) goto end;
    // Get offset of next chunk
    if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
    NextChunkOffset = IFile.fptr + ChunkSz;
    if((NextChunkOffset & 1) != 0) NextChunkOffset++;
    Printf("NextCh: %u\r", NextChunkOffset);

    // Read format
    if(TryRead<uint16_t>(&IFile, &uw16) != retvOk) goto end;
    Printf("Fmt: %X\r", uw16);
    if(uw16 != 1) goto end; // PCM only
    // Channel cnt
    if(TryRead<uint16_t>(&IFile, &Info.ChannelCnt) != retvOk) goto end;
    if(Info.ChannelCnt > 2) goto end;
    // Sample rate
    if(TryRead<uint32_t>(&IFile, &Info.SampleRate) != retvOk) goto end;
    Printf("SmplRt: %u\r", Info.SampleRate);
    // Bytes per second
    if(TryRead<uint32_t>(&IFile, &Info.BytesPerSecond) != retvOk) goto end;
    Printf("BytesPerSecond: %u\r", Info.BytesPerSecond);
    // Block alignment == frame sz
    if(TryRead<uint16_t>(&IFile, &uw16) != retvOk) goto end;
    Printf("BlkAlgn: %u\r", uw16);
    Info.FrameSz = uw16;
    // Bits per sample
    if(TryRead<uint16_t>(&IFile, &Info.BitsPerSample) != retvOk) goto end;
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
            if(memcmp(ChunkID, "wavl", 4) != 0) goto end;
            // Calc offsets
            Info.InitialDataChunkOffset = IFile.fptr;   // Here is "data", definitely
            if((Info.InitialDataChunkOffset & 1) != 0) Info.InitialDataChunkOffset++;
            Info.FinalDataChunkOffset = IFile.fptr + ChunkSz;
            if((Info.FinalDataChunkOffset & 1) != 0) Info.FinalDataChunkOffset++;
            Printf("DataStart: %u; DataEnd: %u\r", Info.InitialDataChunkOffset, Info.FinalDataChunkOffset);
            break;
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
    Info.CurDataChunkFrames = 0;
}
