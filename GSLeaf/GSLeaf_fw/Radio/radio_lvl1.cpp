/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include "radio_lvl1.h"
#include "cc1101.h"
#include "uart.h"
#include "main.h"

#include "led.h"
#include "Sequences.h"
extern LedRGBwPower_t Led;

cc1101_t CC(CC_Setup0);


//#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOB
#define DBG_PIN1    10
#define DBG1_SET()  PinSetHi(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinSetLo(DBG_GPIO1, DBG_PIN1)
#define DBG_GPIO2   GPIOB
#define DBG_PIN2    9
#define DBG2_SET()  PinSetHi(DBG_GPIO2, DBG_PIN2)
#define DBG2_CLR()  PinSetLo(DBG_GPIO2, DBG_PIN2)
#else
#define DBG1_SET()
#define DBG1_CLR()
#endif

rLevel1_t Radio;

#if 1 // ================================ Task =================================
static THD_WORKING_AREA(warLvl1Thread, 256);
__noreturn
static void rLvl1Thread(void *arg) {
    chRegSetThreadName("rLvl1");
    while(true) {
        int8_t Rssi;
        rPkt_t RxPkt;
        CC.Recalibrate();
        uint8_t RxRslt = CC.Receive(90, &RxPkt, RPKT_LEN, &Rssi);
        if(RxRslt == retvOk) {
            Printf("Rssi=%d\r", Rssi);
            // Command from UsbHost to all lockets, no RSSI check
            if(RxPkt.TheWord == 0xCA115EA1) {
                EvtQMain.SendNowOrExit(EvtMsg_t(evtIdOpen));
                rPkt_t TxPkt;
                TxPkt.TheWord = 0x7A1E7A11;
                for(int i=0; i<4; i++) {
                    CC.Recalibrate();
                    CC.Transmit(&TxPkt, RPKT_LEN);
                    chThdSleepMilliseconds(4);
                }
            }
        }
        CC.EnterPwrDown();
        chThdSleepMilliseconds(630);
    } // while true
}
#endif // task

void rLevel1_t::TryToSleep(uint32_t SleepDuration) {
    if(SleepDuration >= MIN_SLEEP_DURATION_MS) CC.EnterPwrDown();
    chThdSleepMilliseconds(SleepDuration);
}

#if 1 // ============================
uint8_t rLevel1_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    PinSetupOut(DBG_GPIO2, DBG_PIN2, omPushPull);
#endif

//    RMsgQ.Init();
    if(CC.Init() == retvOk) {
        CC.SetTxPower(CC_Pwr0dBm);
        CC.SetPktSize(RPKT_LEN);
        CC.SetChannel(1);
//        CC.EnterPwrDown();
        // Thread
        chThdCreateStatic(warLvl1Thread, sizeof(warLvl1Thread), HIGHPRIO, (tfunc_t)rLvl1Thread, NULL);
        return retvOk;
    }
    else return retvFail;
}
#endif