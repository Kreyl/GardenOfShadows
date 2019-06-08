#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmd,
    evtIdEverySecond,

    evtIdButtons,
    evtIdAcc,
    evtIdPlayEnd,
    evtIdPauseEnds,
    evtIdOnRx,

    evtIdSoundPlayStop,
};
