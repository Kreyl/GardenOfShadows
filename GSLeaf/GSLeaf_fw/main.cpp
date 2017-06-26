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

// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
extern CmdUart_t Uart;
void OnCmd(Shell_t *PShell);
void ITask();

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Audio;
AuPlayer_t Player;

#define PAUSE_AFTER_S   4
TmrKL_t tmrPauseAfter {S2ST(PAUSE_AFTER_S), evtIdPauseEnds, tktOneShot};

enum State_t { stIdle, stPlaying, stWaiting };
State_t State = stIdle;

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

    // LED
    Led.Init();
    Led.StartOrRestart(lsqStart);

    PwrEn.Init();
    PwrEn.SetLo();
    chThdSleepMilliseconds(18);

    // Audio
    i2c1.Init();
    Audio.Init();
    Audio.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Audio.DisableHeadphones();
    Audio.EnableSpeakerMono();

//    i2c1.ScanBus();
    Acc.Init();

    SD.Init();
    Player.Init();
    Audio.SetSpeakerVolume(-10);
    Audio.Standby();
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

            case evtIdAcc:
                if(State == stIdle) {
                    Printf("AccWhenIdle\r");
                    Led.StartOrRestart(lsqAccIdle);
                    State = stPlaying;
                    Audio.Resume();
                    Player.PlayRandomFileFromDir("Sounds");
//                    Player.Play("Alive.wav");
                }
                else if(State == stWaiting) {
                    Printf("AccWhenW\r");
                    Led.StartOrRestart(lsqAccWaiting);
                    tmrPauseAfter.StartOrRestart();
                }
                break;

            case evtIdPlayEnd:
                Printf("PlayEnd\r");
                if(State == stPlaying) {
                    tmrPauseAfter.StartOrRestart();
                    State = stWaiting;
                    Led.StartOrRestart(lsqWaiting);
                }
                Audio.Standby();
                break;

            case evtIdPauseEnds:
                if(State == stWaiting) {
                    Led.StartOrRestart(lsqIdle);
                    Printf("PauseEnd\r");
                    State = stIdle;
                }
                break;

            default: break;
        } // switch
    } // while true
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
    else if(PCmd->NameIs("48")) Player.Play("Mocart48.wav");
    else if(PCmd->NameIs("96")) Player.Play("Mocart96.wav");


    else PShell->Ack(retvCmdUnknown);
}
#endif
