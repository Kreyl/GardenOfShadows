#include "acc_mma8452.h"
#include "MsgQ.h"

Acc_t Acc;

#if !MOTION_BY_IRQ
#define AVERAGE_CNT     4
static uint32_t IArr[AVERAGE_CNT];
#endif

// Thread
static THD_WORKING_AREA(waAccThread, 128);
__noreturn
static void AccThread(void *arg) {
    chRegSetThreadName("Acc");
    while(true) Acc.Task();
}

void Acc_t::Task() {
    chThdSleepMilliseconds(108);
//    Printf("t\r");
#if MOTION_BY_IRQ
    if(PinIsHi(ACC_IRQ_GPIO, ACC_IRQ_PIN)) {  // IRQ occured
        Printf("Motion\r");
        IClearIrq();
        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdAcc));
    }
#else
    ReadAccelerations();
    uint32_t Sum =
            Accelerations.xMSB * Accelerations.xMSB +
            Accelerations.yMSB * Accelerations.yMSB +
            Accelerations.zMSB * Accelerations.zMSB;
    // Calc average
    for(int i=(AVERAGE_CNT-1); i>=1; i--) IArr[i] = IArr[i-1];
    IArr[0] = Sum;
    uint32_t Ave = 0;
    for(int i=0; i<AVERAGE_CNT; i++) Ave += IArr[i];
    Ave /= AVERAGE_CNT;

    Printf("X: %d; Y: %d; Z: %d;  Sum: %d; Ave: %d\r", Accelerations.xMSB, Accelerations.yMSB, Accelerations.zMSB, Sum, Ave);
    if(Ave > MOTION_THRESHOLD_TOP or Sum < MOTION_THRESHOLD_BOTTOM) {
        Printf("Motion\r");
        EvtQMain.SendNowOrExit(EvtMsg_t(evtIdAcc));
    }
#endif
}

void Acc_t::Init() {
#if MOTION_BY_IRQ
    // Init INT pin
    PinSetupInput(ACC_IRQ_GPIO, ACC_IRQ_PIN, pudPullDown);
#endif

    // Read WhoAmI
    uint8_t v = 0;
    IReadReg(ACC_REG_WHO_AM_I, &v);
    if(v != 0x2A) {
        Printf("Acc error: %X\r", v);
        return;
    }

    // ==== Setup initial registers ====
    // Put device to StandBy mode
    IWriteReg(ACC_REG_CONTROL1, 0b10100000);    // ODR = 50Hz, Standby
    // Setup High-Pass filter and acceleration scale
    IWriteReg(ACC_REG_XYZ_DATA_CFG, 0x01);      // No filter, scale = 4g
#if MOTION_BY_IRQ // Setup Motion Detection
    IWriteReg(ACC_FF_MT_CFG, 0b11111000);       // Latch enable, detect motion, all three axes
    IWriteReg(ACC_FF_MT_THS, ACC_MOTION_TRESHOLD);  // Threshold = acceleration/0.063. "Detected" = (a > threshold)
    IWriteReg(ACC_FF_MT_COUNT, 0);              // Debounce counter: detect when moving longer than value*20ms (depends on ODR, see below)
#endif
    // Control registers
    IWriteReg(ACC_REG_CONTROL2, 0x00);          // Normal mode
#if MOTION_BY_IRQ
    IWriteReg(ACC_REG_CONTROL3, 0b00001010);    // Freefall/motion may wake up system; IRQ output = active high, Push-Pull
    IWriteReg(ACC_REG_CONTROL4, 0b00000100);    // Freefall/motion IRQ enabled
#else
    IWriteReg(ACC_REG_CONTROL3, 0b00000010);    // IRQ output = active high, Push-Pull
    IWriteReg(ACC_REG_CONTROL4, 0b00000000);    // IRQ disabled
#endif
    IWriteReg(ACC_REG_CONTROL5, 0b00000100);    // FreeFall/motion IRQ is routed to INT1 pin
    IWriteReg(ACC_REG_CONTROL1, 0b10100001);    // DR=100 => 50Hz output data rate (ODR); Mode = Active

#if !MOTION_BY_IRQ
    // Init average
    for(int i=0; i<AVERAGE_CNT; i++) IArr[i] = 1008;
#endif

    // Thread
    chThdCreateStatic(waAccThread, sizeof(waAccThread), NORMALPRIO, (tfunc_t)AccThread, NULL);
}
