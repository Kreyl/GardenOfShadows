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
#define EE_ADDR_HEALTH_STATE    8

int32_t ID;
static uint8_t ISetID(int32_t NewID);
void ReadIDfromEE();

// ==== Periphery ====
LedSmooth_t Led { LED_PIN };

// ==== Timers ====
//static TmrKL_t TmrRxTableCheck {MS2ST(2007), EVT_RXCHECK, tktPeriodic};
#endif

int main(void) {
    // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V2);
//    Clk.SetMSI4MHz();
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
//    if(Sleep::WasInStandby()) {
//        Uart.Printf("WasStandby\r");
//        Sleep::ClearStandbyFlag();
//    }
    Clk.PrintFreqs();
//    RandomSeed(GetUniqID3());   // Init random algorythm with uniq ID

    Led.Init();
//    Led.SetupSeqEndEvt(chThdGetSelfX(), EVT_LED_SEQ_END);
#if BTN_ENABLED
//    PinSensors.Init();
#endif

    // ==== Time and timers ====
//    TmrEverySecond.StartOrRestart();
//    TmrRxTableCheck.InitAndStart();

    // ==== Radio ====
    if(Radio.Init() == retvOk) {
        Led.StartOrRestart(lsqStart);
        RMsg_t msg = {R_MSG_SET_CHNL, (uint8_t)ID};
        Radio.RMsgQ.SendNowOrExit(msg);
    }
    else Led.StartOrRestart(lsqFailure);
    chThdSleepMilliseconds(1008);

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {

#if BTN_ENABLED
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                break;
        if(Evt & EVT_BUTTONS) {
            Uart.Printf("Btn\r");
            BtnEvtInfo_t EInfo;
            while(BtnGetEvt(&EInfo) == OK) {
                if(EInfo.Type == bePress) {
                    Sheltering.TimeOfBtnPress = TimeS;
                    Sheltering.ProcessCChangeOrBtn();
                } // if Press
            } // while
        }
#endif

//        if(Evt & EVT_RX) {
//            int32_t TimeRx = Radio.PktRx.Time;
//            Uart.Printf("RX %u\r", TimeRx);
//            Cataclysm.ProcessSignal(TimeRx);
//        }

//        if(Evt & EVT_RXCHECK) {
//            if(ShowAliens == true) {
//                if(Radio.RxTable.GetCount() != 0) {
//                    Led.StartOrContinue(lsqTheyAreNear);
//                    Radio.RxTable.Clear();
//                }
//                else Led.StartOrContinue(lsqTheyDissapeared);
//            }
//        }

#if 0 // ==== Led sequence end ====
        if(Evt & EVT_LED_SEQ_END) {
        }
#endif
        //        if(Evt & EVT_OFF) {
        ////            Uart.Printf("Off\r");
        //            chSysLock();
        //            Sleep::EnableWakeup1Pin();
        //            Sleep::EnterStandby();
        //            chSysUnlock();
        //        }

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
