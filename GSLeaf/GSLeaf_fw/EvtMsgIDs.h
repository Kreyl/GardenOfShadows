#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmdRcvd,
    evtIdEverySecond,

    evtIdAudioBufEnd,
    evtIdAudioPlayEnd,
};
