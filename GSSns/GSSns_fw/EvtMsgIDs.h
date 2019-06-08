/*
 * EvtMsgIDs.h
 *
 *  Created on: 21 ���. 2017 �.
 *      Author: Kreyl
 */

#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdShellCmd = 1,
    evtIdEverySecond = 2,
    evtIdAdcRslt = 3,

    // Not eternal
    evtIdButtons = 15,
    evtIdAcc = 16,
    evtIdRadioCmd = 18,
    evtIdOnDelayEnd = 19,
};