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

void AuPlayer_t::Init() {

}

uint8_t AuPlayer_t::Open(const char* AFileName) {
//    FRESULT rslt;
    // Open file
    if(TryOpenFileRead(AFileName, &IFile) != retvOk) return retvFail;
    uint8_t Retval = retvFail;
    uint32_t next_chunk_offset, ChunkSz;
    uint16_t uw16;

    // Check if starts with RIFF
    char chunk_id[4] = {0, 0, 0, 0};
    if(TryRead(&IFile, chunk_id, 4) != retvOk or (memcmp(chunk_id, "RIFF", 4) != 0)) goto end;
    // Get file size
    if(TryRead<uint32_t>(&IFile, &Info.Size) != retvOk) goto end;
    Printf("Sz: %u\r", Info.Size);
    // Check riff type
    if(TryRead(&IFile, chunk_id, 4) != retvOk or (memcmp(chunk_id, "WAVE", 4) != 0)) goto end;
    // Check format
    if(TryRead(&IFile, chunk_id, 4) != retvOk or (memcmp(chunk_id, "fmt ", 4) != 0)) goto end;
    // Get offset of next chunk
    if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
    next_chunk_offset = IFile.fptr + ChunkSz;
    if((next_chunk_offset & 1) != 0) next_chunk_offset++;
    Printf("NextCh: %u\r", next_chunk_offset);

    // Check format
    if(TryRead<uint16_t>(&IFile, &uw16) != retvOk) goto end;
    Printf("Fmt: %X\r", uw16);
    if(uw16 != (uint16_t)Format::Pcm) goto end;

    if(TryRead<uint16_t>(&IFile, &Info.ChannelCnt) != retvOk) goto end;
    if(Info.ChannelCnt > 2) goto end;

    if(TryRead<uint32_t>(&IFile, &Info.SampleRate) != retvOk) goto end;
    Printf("SmplRt: %u\r", Info.SampleRate);

    if(TryRead<uint32_t>(&IFile, &Info.BytesPerSecond) != retvOk) goto end;
    Printf("BytesPerSecond: %u\r", Info.BytesPerSecond);

    // Block alignment == frame sz
    if(TryRead<uint16_t>(&IFile, &uw16) != retvOk) goto end;
    Printf("BlkAlgn: %u\r", uw16);
    Info.FrameSz = uw16;

    if(TryRead<uint16_t>(&IFile, &Info.BitsPerSample) != retvOk) goto end;
    Printf("BitsPerSample: %u\r", Info.BitsPerSample);

    // Find data chunk
    while(true) {
        // Move to data chunk
        if(f_lseek(&IFile, next_chunk_offset) != FR_OK) goto end;
        // Check chunk ID
        if(TryRead(&IFile, chunk_id, 4) != retvOk) goto end;
        if(memcmp(chunk_id, "data", 4) == 0) {  // "data" found
            // Read chunk sz
            if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
            // Calc offsets
            Info.InitialDataChunkOffset = next_chunk_offset;
            if((Info.InitialDataChunkOffset & 1) != 0) Info.InitialDataChunkOffset++;
            Info.FinalDataChunkOffset = IFile.fptr + ChunkSz;
            if((Info.FinalDataChunkOffset & 1) != 0) Info.FinalDataChunkOffset++;
            Printf("DataStart: %u; DataEnd: %u\r", Info.InitialDataChunkOffset, Info.FinalDataChunkOffset);
            break;  // Data found
        }

        if(memcmp(chunk_id, "LIST", 4) == 0) {  // "LIST" found
            // Read chunk sz
            if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
            char list_type[4];
            if(TryRead(&IFile, list_type, 4) != retvOk) goto end;
            if (memcmp(list_type, "wavl", sizeof(list_type)) != 0) goto end;
            // Calc offsets
            Info.InitialDataChunkOffset = IFile.fptr;   // Here is "data", definitely
            if((Info.InitialDataChunkOffset & 1) != 0) Info.InitialDataChunkOffset++;
            Info.FinalDataChunkOffset = IFile.fptr + ChunkSz;
            if((Info.FinalDataChunkOffset & 1) != 0) Info.FinalDataChunkOffset++;
            Printf("DataStart: %u; DataEnd: %u\r", Info.InitialDataChunkOffset, Info.FinalDataChunkOffset);
            break;
        }

        // Read current chunk size and proceed with data search
        if(TryRead<uint32_t>(&IFile, &ChunkSz) != retvOk) goto end;
        next_chunk_offset = IFile.fptr + ChunkSz;
        if(next_chunk_offset & 1) next_chunk_offset++;
    } // while true
    // Offsets ready
    Rewind();
    return retvOk;

    end:
    f_close(&IFile);
    return Retval;
}

