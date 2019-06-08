#pragma once

#include "color.h"

#define IDLESND_DIRNAME     "IdleSnd"
#define SIGNAL_DIRNAME      "SignalSnd"

class Settings_t {
public:
    struct {
        struct {
            uint32_t Max_s = 12, Min_s = 4;
        } SndPeriod;
        Color_t Clr = clBlue;
    } Idle;

    struct {
        struct {
            Color_t Clr = clYellow;
            uint32_t Duration_s = 1;
        } Flash;
    } Signal;
    void Load();
};
