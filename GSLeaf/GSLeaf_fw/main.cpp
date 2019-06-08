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
//#include "radio_lvl1.h"
#include "SimpleSensors.h"
#include "kl_json.h"
#include "main.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();

#define PAUSE_BEFORE_REPEAT_S       7

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };
PinOutput_t PwrEn(PWR_EN_PIN);
CS42L52_t Codec;
Settings_t Settings;

//TmrKL_t tmrPauseAfter {evtIdPauseEnds, tktOneShot};
TmrKL_t tmrIdleSnd {evtIdIdleSnd, tktOneShot};
void StartIdleTmr();
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

//    Led.Init();

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
    // Decoder
    Player.Init();

//    i2c1.ScanBus();
//    Acc.Init();

    SD.Init();
    Settings.Load();

    Codec.SetSpeakerVolume(0);
//    Codec.Standby();

//    if(Radio.Init() == retvOk) Led.StartOrRestart(lsqStart);
//    else Led.StartOrRestart(lsqFailure);

//    SimpleSensors::Init();

    // Start playing surround music
//    Player.PlayRandomFileFromDir(DIRNAME_SURROUND);

//    Player.Play("alive.wav", spmSingle);

    StartIdleTmr();

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
//                int32_t rxID = Msg.Value;
//                if(IdPlayingNow == ID_SURROUND) {
//                    if(RxTable.EnoughTimePassed(rxID)) {
//                        // Fadeout surround and play rcvd id
//                        IdPlayNext = rxID;
//                        Player.FadeOut();
//                    }
//                }
//                else { // Playing some ID
//                    if(rxID != IdPlayingNow) {  // Some new ID received
//                        // Switch to new ID if current one is offline for enough time
//                        if(RxTable.EnoughTimePassed(IdPlayingNow)) {
//                            // Fadeout current and play rcvd id
//                            IdPlayNext = rxID;
//                            Player.FadeOut();
//                        }
//                    }
//                }
//                // Put timestamp to table
//                RxTable.Put(rxID);
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

            case evtIdIdleSnd:

                break;

            case evtIdSoundPlayStop: {
                Printf("PlayEnd\r");
//                IdPlayingNow = IdPlayNext;
//                IdPlayNext = ID_SURROUND;
//                // Decide what to play: surround or some id
//                if(IdPlayingNow == ID_SURROUND) strcpy(DirName, DIRNAME_SURROUND);
//                else itoa(IdPlayingNow, DirName, 10);
////                Printf("Play %S\r", DirName);
//                Player.PlayRandomFileFromDir(DirName);
            } break;

            case evtIdButtons:
//                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
//                if(Msg.BtnEvtInfo.BtnID == 0) Audio.VolumeUp();
//                else Audio.VolumeDown();
                break;

            default: break;
        } // switch
    } // while true
}

void StartIdleTmr() {
    // Calculate random time
    sysinterval_t Delay = Random::Generate(Settings.Idle.SndPeriod.Min_s, Settings.Idle.SndPeriod.Max_s);
    tmrIdleSnd.StartOrRestart(Delay);
}


//void OnRadioRx(uint8_t AID, int8_t Rssi) {
//    Printf("Rx %u %d\r", AID, Rssi);
//    if(AID < RCHNL_MIN or AID > RCHNL_MAX) return;
//    // Inform main thread
//    EvtMsg_t Msg(evtIdOnRx, (int32_t)AID);
//    EvtQMain.SendNowOrExit(Msg);
//}

void ProcessChargePin(PinSnsState_t *PState, uint32_t Len) {
    if(*PState == pssFalling) { // Charge started
        Led.StartOrContinue(lsqCharging);
        Printf("Charge started\r");
    }
    if(*PState == pssRising) { // Charge ended
        Led.StartOrContinue(lsqOperational);
        Printf("Charge ended\r");
    }
}

void Settings_t::Load() {
    JsonParser_t *PJParser = new JsonParser_t;
    bool IsOk = true;
    if(PJParser->StartReadFromFile("Settings.ini") == retvOk) {
        JsonObj_t &Root = PJParser->Root;
        if(Root["Idle"]["Color"].ToColor(&Idle.Clr) != retvOk) IsOk = false;
        if(Root["Idle"]["SoundPeriod"]["Min"].ToUint(&Idle.SndPeriod.Min_s) != retvOk) IsOk = false;
        if(Root["Idle"]["SoundPeriod"]["Max"].ToUint(&Idle.SndPeriod.Max_s) != retvOk) IsOk = false;
        if(Root["Signal"]["Color"].ToColor(&Signal.Flash.Clr) != retvOk) IsOk = false;
        if(Root["Signal"]["Duration"].ToUint(&Signal.Flash.Duration_s) != retvOk) IsOk = false;
    }
    else IsOk = false;
    if(IsOk) Printf("Settings loaded\r");
    else Printf("Settings load error\r");
    delete PJParser;
}


#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
//    __unused int32_t dw32 = 0;  // May be unused in some configurations
//    Uart.Printf("\r%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

//    else if(PCmd->NameIs("V")) {
//        int8_t v;
//        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
//        Audio.SetMasterVolume(v);
//    }
//    else if(PCmd->NameIs("SV")) {
//        int8_t v;
//        if(PCmd->GetNext<int8_t>(&v) != retvOk) { PShell->Ack(retvCmdError); return; }
//        Audio.SetSpeakerVolume(v);
//    }

//    else if(PCmd->NameIs("A")) Player.Play("Alive.wav");
//    else if(PCmd->NameIs("48")) {
//        Audio.Resume();
//        Player.Play("Mocart48.wav");
//    }
//    else if(PCmd->NameIs("FO")) Player.FadeOut();


    else PShell->Ack(retvCmdUnknown);
}
#endif
