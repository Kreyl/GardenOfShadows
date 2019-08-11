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
#include "SimpleSensors.h"
#include "main.h"
#include "ini_kl.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();


enum State_t {stIdle, stPlaying, stPauseAfter};
static State_t State = stIdle;
static int32_t DelayBeforeNextPlay_s = 7;

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };

PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Codec;
static int32_t Volume = 9;

TmrKL_t tmrPauseAfter {TIME_S2I(7), evtIdPauseEnd, tktOneShot};
DirList_t DirList;
static char FName[MAX_NAME_LEN];
#endif

int main(void) {
    // ==== Setup clock frequency ====
    Clk.SetCoreClk(cclk24MHz);
    Clk.UpdateFreqValues();
    // 48 MHz Clock
    Clk.EnablePLLQOut();
    Clk.Select48MHzClk(src48PllQ);
    // SAI clock
    Clk.EnablePLLPOut();
    Clk.SelectSAI1Clk(srcSaiPllP);

    // Init OS
    halInit();
    chSysInit();
    // ==== Init hardware ====
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    Led.Init();
    Led.StartOrRestart(lsqStart);

    PwrEn.Init();
    PwrEn.SetLo();
    chThdSleepMilliseconds(18);

    // Audio
    i2c1.Init();
    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Stereo); // Always
    Codec.Standby();
    // Decoder
    Player.Init();

//    i2c1.ScanBus();
    Acc.Init();

    SD.Init();
#if 1 // Read config
    int32_t tmp;
    if(ini::ReadInt32("Settings.ini", "Common", "Volume", &tmp) == retvOk) {
        if(tmp >= 0 and tmp <= 100) {
            // 0...100 => -38...12
            Volume = (tmp / 2) - 38;
            Printf("Volume: %d -> %d\r", tmp, Volume);
        }
    }
    if(ini::ReadInt32("Settings.ini", "Common", "Threshold", &tmp) == retvOk) {
        if(tmp > 0) {
            Acc.ThresholdStable = tmp;
            Printf("ThresholdStable: %d\r", tmp);
        }
    }
    if(ini::ReadInt32("Settings.ini", "Common", "Delay", &tmp) == retvOk) {
        if(tmp > 0) {
            DelayBeforeNextPlay_s = tmp;
            Printf("DelayBeforeNextPlay_s: %d\r", DelayBeforeNextPlay_s);
        }
    }
#endif

    Codec.SetSpeakerVolume(0);
    Codec.SetMasterVolume(Volume);
    Codec.Resume();
    Player.Play("alive.wav", spmSingle);
    chThdSleepMilliseconds(1530);

//    SimpleSensors::Init();

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

            case evtIdMotion:
                switch(State) {
                    case stIdle:
                        Printf("AccWhenIdle\r");
                        if(DirList.GetRandomFnameFromDir(DIRNAME_SND, FName) == retvOk) {
                            Led.StartOrRestart(lsqAccWhenIdleAndPlay);
                            Codec.Resume();
                            Codec.SetMasterVolume(Volume);
                            Player.Play(FName, spmSingle);
                            State = stPlaying;
                        }
                        else Led.StartOrRestart(lsqAccWhenIdleAndNothing);
                        break;

                    case stPlaying:
                        Printf("AccWhenPlaying\r");
                        Led.StartOrRestart(lsqAccWhenPlaying);
                        break;

                    case stPauseAfter:
                        Printf("AccWhenPause\r");
                        Led.StartOrRestart(lsqAccWhenPause);
                        // Renew timer
//                        tmrPauseAfter.SetNewPeriod_s(DelayBeforeNextPlay_s);
//                        tmrPauseAfter.StartOrRestart();
                        break;
                } // switch state
                break;

            case evtIdPauseEnd:
                Printf("PauseEnd\r");
                State = stIdle;
                Led.StartOrRestart(lsqIdle);
                break;

            case evtIdSoundPlayStop:
                Printf("PlayEnd\r");
                Led.StartOrRestart(lsqPause);
                Codec.Standby();
                tmrPauseAfter.SetNewPeriod_s(DelayBeforeNextPlay_s);
                tmrPauseAfter.StartOrRestart();
                State = stPauseAfter;
                break;

            default: break;
        } // switch
    } // while true
}

void ProcessSns(PinSnsState_t *PState, uint32_t Len) {
//    if(*PState == pssRising) {
//        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdSns));
//    }
//    Printf("st: %u\r", *PState);
}

#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
//    __unused int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("\r%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

//    else if(PCmd->NameIs("s")) {
//        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdSns));
//    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
