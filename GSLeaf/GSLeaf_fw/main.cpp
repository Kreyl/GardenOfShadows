/*
 * main.cpp
 *
 *  Created on: 20 февр. 2014 г.
 *      Author: g.kruglov
 */

#include "hal.h"
#include "MsgQ.h"
#include "kl_i2c.h"
#include "Sequences.h"
#include "shell.h"
#include "led.h"
#include "CS42L52.h"
#include "kl_sd.h"
#include "AuPlayer.h"
#include "acc_mma8452.h"
#include "kl_fs_utils.h"
#include "radio_lvl1.h"
#include "SimpleSensors.h"

// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
extern CmdUart_t Uart;
void OnCmd(Shell_t *PShell);
void ITask();

#define PAUSE_BEFORE_REPEAT_S       7

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Audio;
AuPlayer_t Player;

#if 1 // ============================= Rx Table ================================
class RxTable_t {
private:
    systime_t ITable[RCHNL_CNT];
public:
    void Put(int32_t Chnl) {
        int32_t Indx = Chnl - RCHNL_MIN;
        ITable[Indx] = chVTGetSystemTimeX();
    }

    bool EnoughTimePassed(int32_t Chnl) {
        int32_t Indx = Chnl - RCHNL_MIN;
        Printf("%u\r", chVTTimeElapsedSinceX(ITable[Indx]));
        return (chVTTimeElapsedSinceX(ITable[Indx]) >= S2ST(PAUSE_BEFORE_REPEAT_S));
    }

    RxTable_t() {
        for(int i=0; i<RCHNL_CNT; i++) ITable[i] = 0;
    }
} RxTable;
#endif

int32_t IdNowPlaying = -1, IdPrevious = -1;
void PlayID(int32_t AID) {
    char DirName[9];
    itoa(IdNowPlaying, DirName, 10);
    Printf("Play %S\r", DirName);
    Audio.Resume();
    Player.PlayRandomFileFromDir(DirName);
}

TmrKL_t tmrPauseAfter {evtIdPauseEnds, tktOneShot};

int main(void) {
    // ==== Setup clock frequency ====
    Clk.SetHiPerfMode();
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();
    // ==== Init hardware ====
    EvtQMain.Init();
    Uart.Init(115200);
    Printf("\r%S %S\r\n", APP_NAME, BUILD_TIME);
    Clk.PrintFreqs();

    Clk.Select48MhzSrc(src48PllQ);

    Led.Init();

    PwrEn.Init();
    PwrEn.SetLo();
    chThdSleepMilliseconds(18);

    // Audio
    i2c1.Init();
    Audio.Init();
    Audio.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Audio.DisableSpeakers();
    Audio.EnableHeadphones();

//    i2c1.ScanBus();
    Acc.Init();

    SD.Init();
    Player.Init();

    Audio.SetSpeakerVolume(0);
    Audio.Standby();

    if(Radio.Init() == retvOk) Led.StartOrRestart(lsqStart);
    else Led.StartOrRestart(lsqFailure);

    SimpleSensors::Init();

//    Player.Play("alive.wav");

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdOnRx: {
                int32_t rxID = Msg.Value;
                if(IdNowPlaying < 0) {  // Not playing now
                    if(RxTable.EnoughTimePassed(rxID)) {
                        IdNowPlaying = rxID;
                        PlayID(IdNowPlaying);
                    }
                }
                else { // Playing now
                    if(rxID != IdNowPlaying) {  // Some new ID received
                        // Switch to new ID if current one is offline for enough time
                        if(RxTable.EnoughTimePassed(IdNowPlaying)) {
                            IdPrevious = IdNowPlaying;
                            IdNowPlaying = rxID;
                            PlayID(IdNowPlaying);
                        }
                    }
                }
                // Put timestamp to table
                RxTable.Put(Msg.Value);
            } break;

//            case evtIdAcc:
//                if(State == stIdle) {
//                    Printf("AccWhenIdle\r");
//                    Led.StartOrRestart(lsqAccIdle);
//                    State = stPlaying;
//                    Audio.Resume();
//                    Player.PlayRandomFileFromDir("Sounds");
////                    Player.Play("Alive.wav");
//                }
//                else if(State == stWaiting) {
//                    Printf("AccWhenW\r");
//                    Led.StartOrRestart(lsqAccWaiting);
//                    tmrPauseAfter.StartOrRestart();
//                }
//                break;

            case evtIdPlayEnd:
                Printf("PlayEnd\r");
                IdPrevious = IdNowPlaying;
                IdNowPlaying = -1;
                Audio.Standby();
                break;

            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                if(Msg.BtnEvtInfo.BtnID == 0) Audio.VolumeUp();
                else Audio.VolumeDown();
                break;

            default: break;
        } // switch
    } // while true
}

void OnRadioRx(uint8_t AID, int8_t Rssi) {
    Printf("Rx %u %d\r", AID, Rssi);
    if(AID < RCHNL_MIN or AID > RCHNL_MAX) return;
    // Inform main thread
    EvtMsg_t Msg(evtIdOnRx, (int32_t)AID);
    EvtQMain.SendNowOrExit(Msg);
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
//    __unused int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("\r%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Printf("%S %S\r", APP_NAME, BUILD_TIME);

    else if(PCmd->NameIs("V")) {
        int8_t v;
        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
        Audio.SetMasterVolume(v);
    }
    else if(PCmd->NameIs("SV")) {
        int8_t v;
        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
        Audio.SetSpeakerVolume(v);
    }

    else if(PCmd->NameIs("A")) Player.Play("Alive.wav");
    else if(PCmd->NameIs("44")) Player.Play("Mocart44.wav");
    else if(PCmd->NameIs("48")) {
        Audio.Resume();
        Player.Play("Mocart48.wav");
    }
    else if(PCmd->NameIs("96")) Player.Play("Mocart96.wav");


    else PShell->Ack(retvCmdUnknown);
}
#endif
