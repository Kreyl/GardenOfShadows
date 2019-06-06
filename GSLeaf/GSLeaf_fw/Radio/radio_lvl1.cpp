/*
 * radio_lvl1.cpp
 *
 *  Created on: Nov 17, 2013
 *      Author: kreyl
 */

#include "radio_lvl1.h"
#include "cc1101.h"
#include "uart.h"
#include "led.h"
#include "vibro.h"

cc1101_t CC(CC_Setup0);
extern int32_t ID;

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
rPkt_t Pkt;
extern LedRGB_t Led;
extern LedSmooth_t Lumos;
extern Vibro_t Vibro;

#if 1 // ================================ Task =================================
static THD_WORKING_AREA(warLvl1Thread, 256);
__noreturn
static void rLvl1Thread(void *arg) {
    chRegSetThreadName("rLvl1");
    Radio.ITask();
}
#endif // task

__noreturn
void rLevel1_t::ITask() {
    while(true) {
        CC.Recalibrate();
        uint8_t RxRslt = CC.Receive(7, &Pkt, RPKT_LEN, &Rssi);
        if(RxRslt == retvOk) {
//            Printf("\rRssi=%d", Rssi);
            // Process received
            Led.SetColor(Color_t(Pkt.R, Pkt.G, Pkt.B));
            Lumos.SetBrightness(Pkt.W);
            Vibro.Set(Pkt.VibroPwr);

            // Send all data in queue
            while(TxBuf.Get(&Pkt) == retvOk) {
                chThdSleepMilliseconds(2); // Let receiver think. Waste some ms here.
                CC.Transmit(&Pkt, RPKT_LEN);
            } // while
        } // if rcvd
    } // while true
}

void rLevel1_t::TryToSleep(uint32_t SleepDuration) {
    if(SleepDuration >= MIN_SLEEP_DURATION_MS) CC.PowerOff();
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
        CC.SetChannel(ID);
        chThdCreateStatic(warLvl1Thread, sizeof(warLvl1Thread), HIGHPRIO, (tfunc_t)rLvl1Thread, NULL);
        return retvOk;
    }
    else return retvFail;
}
#endif
