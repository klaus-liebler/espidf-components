#pragma once
#include <common.hh>

// Kleiner, wiederverwendbarer Helper fuer die "absolute Deadline"-Idiom, die vor diesem Merge
// bereits unabhaengig voneinander in led::Animator (dieses Repo), single_led.hh (labathome/
// factory_in_a_box) und mehreren sensact-applicationmodel-Apps (cOnOff/cSinglePWM/cRgbw, dort
// unter "autoOffCfg"/"autoOffCalc") nachgebaut wurde.
namespace led
{
    struct AutoOffDeadline
    {
        tms_t deadline{INT64_MAX};

        // relativeDurationMs == 0 bedeutet "nie" (Deadline auf INT64_MAX).
        void Arm(tms_t now, tms_t relativeDurationMs)
        {
            deadline = (relativeDurationMs == 0) ? INT64_MAX : (now + relativeDurationMs);
        }

        bool Expired(tms_t now) const
        {
            return now >= deadline;
        }
    };
}
