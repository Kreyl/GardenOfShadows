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
#include "SimpleSensors.h"
#include "CS42L52.h"
#include "kl_sd.h"
#include "AuPlayer.h"

// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
extern CmdUart_t Uart;
void OnCmd(Shell_t *PShell);
void ITask();

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Audio;
AuPlayer_t Player;

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
//    Audio.StartStream();

    SD.Init();
//    int32_t dw32;
//    if(SD.iniRead<int32_t>("settings.ini", "radio", "id", &dw32) == retvOk) {
//        Printf("%d\r", dw32);
//    }
    Player.Init();
    Player.Play("alive.wav");

    SimpleSensors::Init();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            default: break;
        } // switch
    } // while true
}

#if 1 // ========================== Audio processing ===========================
void AuOnNewSampleI(SampleStereo_t &Sample) {

}
#endif

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

    else if(PCmd->NameIs("A")) Player.Play("Alive.wav");
    else if(PCmd->NameIs("44")) Player.Play("Mocart44.wav");
    else if(PCmd->NameIs("48")) Player.Play("Mocart48.wav");
    else if(PCmd->NameIs("96")) Player.Play("Mocart96.wav");



    else PShell->Ack(retvCmdUnknown);
}
#endif
