/*
 * main.cpp
 *
 *  Created on: 20 февр. 2014 г.
 *      Author: g.kruglov
 */

#include "board.h"
#include "led.h"
#include "Sequences.h"
#include "radio_lvl1.h"
#include "kl_i2c.h"
#include "kl_lib.h"
#include "MsgQ.h"
#include "SimpleSensors.h"
#include "acc_mma8452.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
extern CmdUart_t Uart;
static void ITask();
static void OnCmd(Shell_t *PShell);

#define ID_MIN                  1
#define ID_MAX                  36
#define ID_DEFAULT              ID_MIN
// EEAddresses
#define EE_ADDR_DEVICE_ID       0

int32_t ID;
static uint8_t ISetID(int32_t NewID);
void ReadIDfromEE();

extern Acc_t Acc;
static bool AccExists, IsOn = true;

// No more than 6500 mS
TmrKL_t TmrOnDelay (S2ST(4), evtIdOnDelayEnd, tktOneShot);

LedSmooth_t Led { LED_PIN };
#endif

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V2);
    Clk.UpdateFreqValues();

    // === Init OS ===
    halInit();
    chSysInit();
    EvtQMain.Init();

    // ==== Init hardware ====
    Uart.Init(115200);
    ReadIDfromEE();
    Printf("\r%S %S; ID=%u\r", APP_NAME, BUILD_TIME, ID);
//    Uart.Printf("ID: %X %X %X\r", GetUniqID1(), GetUniqID2(), GetUniqID3());
    if(Sleep::WasInStandby()) {
        Printf("WasStandby\r");
        Sleep::ClearStandbyFlag();
    }
    Clk.PrintFreqs();

    Led.Init();
    i2c1.Init();
//    i2c1.ScanBus();
    AccExists = (Acc.Init() == retvOk);
    if(!AccExists) {
        i2c1.Standby();
        SimpleSensors::Init();
    }

    // ==== Radio ====
    if(Radio.Init() == retvOk) {
        Led.StartOrRestart(lsqStart);
        RMsg_t msg = {R_MSG_SET_CHNL, (uint8_t)ID};
        Radio.RMsgQ.SendNowOrExit(msg);
    }
    else {
        Led.StartOrRestart(lsqFailure);
        while(true) {
            chThdSleepMilliseconds(3600);
        }
    }

    TmrOnDelay.StartOrRestart();    // Will switch device off after a while

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdOnDelayEnd: {
                Printf("Off\r");
                IsOn = false;
                Led.SetBrightness(0);
                RMsg_t msg = {R_MSG_STANDBY};
                Radio.RMsgQ.SendWaitingAbility(msg, MS2ST(4500));
                chThdSleepMilliseconds(90);
                if(!AccExists) {
                    chSysLock();
                    Sleep::EnableWakeup1Pin();
                    Sleep::EnterStandby();
                    chSysUnlock();
                }
            } break;

            case evtIdAcc:
                Printf("Acc\r");
                TmrOnDelay.StartOrRestart();
                if(!IsOn) {
                    IsOn = true;
                    Led.StartOrRestart(lsqStart);
                    RMsg_t msg = {R_MSG_WAKEUP};
                    Radio.RMsgQ.SendWaitingAbility(msg, MS2ST(4500));
                }
                break;

#if UART_RX_ENABLED
            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
#endif
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

void ProcessTouch(PinSnsState_t *PState, uint32_t Len) {
    if(*PState == pssHi) {
        TmrOnDelay.StartOrRestart();
//        Printf("Touch\r");
    }
    else if(*PState == pssFalling) Printf("Detouch\r");
}

#if UART_RX_ENABLED // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Version")) PShell->Printf("%S %S\r", APP_NAME, BUILD_TIME);

    else if(PCmd->NameIs("GetID")) PShell->Reply("ID", ID);

    else if(PCmd->NameIs("SetID")) {
        if(PCmd->GetNext<int32_t>(&ID) != retvOk) { PShell->Ack(retvCmdError); return; }
        uint8_t r = ISetID(ID);
        RMsg_t msg = {R_MSG_SET_CHNL, (uint8_t)ID};
        Radio.RMsgQ.SendNowOrExit(msg);
        PShell->Ack(r);
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif

#if 1 // =========================== ID management =============================
void ReadIDfromEE() {
    ID = EE::Read32(EE_ADDR_DEVICE_ID);  // Read device ID
    if(ID < ID_MIN or ID > ID_MAX) {
        Printf("\rUsing default ID\r");
        ID = ID_DEFAULT;
    }
}

uint8_t ISetID(int32_t NewID) {
    if(NewID < ID_MIN or NewID > ID_MAX) return retvFail;
    uint8_t rslt = EE::Write32(EE_ADDR_DEVICE_ID, NewID);
    if(rslt == retvOk) {
        ID = NewID;
        Printf("New ID: %u\r", ID);
        return retvOk;
    }
    else {
        Printf("EE error: %u\r", rslt);
        return retvFail;
    }
}
#endif
