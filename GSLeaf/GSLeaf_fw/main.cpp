#include "hal.h"
#include "MsgQ.h"
#include "kl_i2c.h"
#include "Sequences.h"
#include "shell.h"
#include "led.h"
#include "CS42L52.h"
#include "SimpleSensors.h"
#include "uart2.h"
#include "kl_json.h"
#include "math.h"

#if 1 // ======================== Variables and defines ========================
// Forever
bool OsIsInitialized = false;
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
void OnCmd(ShellJson_t *PShell);
void ITask();

LedRGB_t Led { LED_RED_CH, LED_GREEN_CH, LED_BLUE_CH };

static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
JsonUart_t Uart{CmdUartParams};

PinOutput_t PwrEn(PWR_EN_PIN);

#define SAMPLING_FREQ_HZ    96000UL
#define DAC_BUF_SZ          20000UL
CS42L52_t Codec;
#endif

int main(void) {
#if 1 // ==== Setup clock frequency ====
    Clk.EnablePrefetch();
    Clk.SwitchToMSI();
    Clk.SetVoltageRange(mvrHiPerf);
//    Clk.SetupFlashLatency(80, mvrHiPerf);
    Clk.SetupFlashLatency(24, mvrHiPerf);
    // Try quartz
    if(Clk.EnableHSE() == retvOk) {
        Clk.SetupPllSrc(pllsrcHse);
        Clk.SetupM(3); // 12MHz / 3 = 4
    }
    else { // Quartz failed
        Clk.SetupPllSrc(pllsrcMsi);
        Clk.SetupM(1); // 4MHz / 1 = 4
    }
//    Clk.SetupPll(40, 2, 2);         // Sys clk: 4 * 40 / 2 => 80
    Clk.SetupPll(24, 4, 2);         // Sys clk: 4 * 24 / 4 => 80
    Clk.SetupPllSai1(24, 2, 2, 7);  // 48Mhz clk: 4 * 24 / 2 => 48
    // Sys clk
    if(Clk.EnablePLL() == retvOk) {
        Clk.EnablePllROut();
        Clk.SwitchToPLL();
    }
    // 48 MHz
    if(Clk.EnablePllSai1() == retvOk) {
        Clk.EnablePllSai1QOut();
        Clk.SetupSai1Qas48MhzSrc();
    }
    // SAI clock
    Clk.EnablePllPOut();
    Clk.SelectSAI1Clk(srcSaiPllP);
    Clk.UpdateFreqValues();
#endif

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

//    PwrEn.Init();
//    PwrEn.SetLo();
//    chThdSleepMilliseconds(18);

    // Audio
    i2c1.Init();
    Codec.Init();
    Codec.SetSpeakerVolume(-96);    // To remove speaker pop at power on
    Codec.DisableHeadphones();
    Codec.EnableSpeakerMono();
    Codec.SetupMonoStereo(Mono);
//    Codec.EnableHeadphones();
    Codec.SetHeadphoneVolume(0);
    Codec.SetMasterVolume(0);
    Codec.SetSpeakerVolume(0);
    Codec.SetupSampleRate(SAMPLING_FREQ_HZ);

    // Main cycle
    ITask();
}



template <typename T, uint32_t MaxSz>
struct BufTypeSz_t {
    uint32_t Length;
    T Buf[MaxSz];
};

typedef BufTypeSz_t<int16_t, DAC_BUF_SZ> DacBuf_t;
static DacBuf_t IBuf1, IBuf2;
static volatile DacBuf_t *BufToWriteTo = &IBuf1;
static int32_t CurrSweepFreq, SweepAmpl, FreqStart, FreqEnd, FreqIncrement = 2;
static volatile bool NewBufIsReady = false;
static volatile bool SignalBufEnd = false;

extern "C"
void DmaSAITxIrq(void *p, uint32_t flags) {
//    PrintfI("  %X %u\r", BufToWriteTo, BufToWriteTo->Length);
    if(NewBufIsReady) {
        NewBufIsReady = false;
        if(BufToWriteTo->Length == 0) {
            Codec.Stop();
        }
        else {
            Codec.TransmitBuf((void*)BufToWriteTo->Buf, BufToWriteTo->Length);
            BufToWriteTo = (BufToWriteTo == &IBuf1)? &IBuf2 : &IBuf1;
            BufToWriteTo->Length = 0;
        }
    }
    if(SignalBufEnd) {
        chSysLockFromISR();
        EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdAudioBufEnd));
        chSysUnlockFromISR();
    }
}

void FillBuf(volatile DacBuf_t *pBuf) {
    volatile int16_t *p = pBuf->Buf;
    pBuf->Length = 0;
    while(true) {
        // Check if top freq reached
        if(CurrSweepFreq > FreqEnd) break;
        // Number of samples per period
        int32_t NSamples = SAMPLING_FREQ_HZ / CurrSweepFreq;
        if((pBuf->Length + NSamples) >  DAC_BUF_SZ) break; // Do not add data if it will overflow buffer
        // Fill with curr freq
        float Multi = 2.0 * M_PI * (float)CurrSweepFreq / (float)SAMPLING_FREQ_HZ; // normalize freq
        for(int32_t i=0; i<NSamples; i++) {
            *p++ = (int16_t)(SweepAmpl * sinf(Multi * (float)i));
        }
        pBuf->Length += NSamples;
        CurrSweepFreq += FreqIncrement; // Increment current freq
    } // while
    NewBufIsReady = true;
//    Printf("%u\r", CurrSweepFreq);
}

void Sweep() {
    Codec.Stop();
    // Fill both bufs
    CurrSweepFreq = FreqStart; // Start freq
    FillBuf(&IBuf1);
    FillBuf(&IBuf2);
    BufToWriteTo = &IBuf2; // After end of buf1, play buf2 and fill buf1
    SignalBufEnd = true;
    Codec.TransmitBuf(IBuf1.Buf, IBuf1.Length);
}

void OnBufEnd() {
    FillBuf(BufToWriteTo);
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdShellCmdRcvd: {
                JsonUart_t* JUart = (JsonUart_t*)Msg.Ptr;
                while(JUart->TryParseRxBuff() == retvOk) {
                    ShellJson_t *JShell = (ShellJson_t*)JUart;
                    OnCmd(JShell);
                    JShell->Cmd.Reset();
                }
            } break;

            case evtIdAudioBufEnd:
                OnBufEnd();
                break;
            case evtIdAudioPlayEnd:
                Printf("PlayEnd\r");
                break;

            default: break;
        } // switch
    } // while true
}

#if 1 // ======================= Command processing ============================
void OnCmd(ShellJson_t *PShell) {
    JsonParser_t JParser;
    if(JParser.StartReadFromBuf(PShell->Cmd.IBuf, JSON_CMD_BUF_SZ) == retvOk) {
        JsonObj_t &NCmd = JParser.Root["Cmd"];
//        char* Cmd = NCmd.Value;
//        Printf("Cmd: %S\r", Cmd);
        // Handle command
        if(NCmd.ValueIsEqualTo("Ping")) PShell->ResultOk();
//        else if(NCmd.ValueIsEqualTo("Version")) PShell->Print("{\"Version\": \"%S %S\"}\r\n", APP_NAME, AppVersion);
        else if(NCmd.ValueIsEqualTo("mem")) PrintMemoryInfo();
//        else if(NCmd.ValueIsEqualTo("i2cscan")) ji2c.ScanBus();

#if 1 // ==== Audio ====
        else if(NCmd.ValueIsEqualTo("sweep")) {
            JsonObj_t &NAmpl = JParser.Root["A"];
            JsonObj_t &NFStart = JParser.Root["F1"];
            JsonObj_t &NFEnd = JParser.Root["F2"];
            JsonObj_t &NFInc = JParser.Root["FInc"];
            if(NAmpl.ToInt(&SweepAmpl) == retvOk) {
                if(NFStart.ToInt(&FreqStart) != retvOk) FreqStart = 300;
                if(NFEnd.ToInt(&FreqEnd) != retvOk) FreqEnd = 4005;
                if(NFInc.ToInt(&FreqIncrement) != retvOk) {
                    if(FreqStart == FreqEnd) FreqIncrement = 0;
                    else FreqIncrement = 2;
                }
                Sweep();
                PShell->ResultOk();
            }
            else PShell->Result("BadValue");
        }

//        else if(NCmd.ValueIsEqualTo("TestMics")) {
//            JsonObj_t &NAmpl = JParser.Root["A"];
//            int32_t Amps[7];
//            if(NAmpl.IsArray) {
//                if(NAmpl.ArrayCnt() == 7) {
//                    for(int i=0; i<7; i++) {
//                        if(NAmpl[i].ToInt(&Amps[i]) != retvOk) {
//                            PShell->Result("BadValue");
//                            return;
//                        }
//                    } // for
//                    Mics.Check(Amps, PShell);
//                }
//                else PShell->Result("BadValue");
//            }
//            else {
//                int32_t A;
//                if(NAmpl.ToInt(&A) == retvOk) {
//                    for(int i=0; i<7; i++) Amps[i] = A;
//                    Mics.Check(Amps, PShell);
//                }
//                else PShell->Result("BadValue");
//            }
//        }
//
//        else if(NCmd.ValueIsEqualTo("TestMic")) {
//            int32_t A, SpkIndx;
//            if(JParser.Root["A"].ToInt(&A) == retvOk) {
//                if(JParser.Root["SpkIndx"].ToInt(&SpkIndx) == retvOk) {
//                    if(SpkIndx >= 0 and SpkIndx <= 6) {
//                        Mics.SweepSingle(A, SpkIndx);
//                        PShell->ResultOk();
//                    }
//                    else PShell->Result("BadValue");
//                }
//                else PShell->Result("BadValue");
//            }
//            else PShell->Result("BadValue");
//        }
#endif

        else PShell->Result("Cmd Unknown");
    }
}


#endif
