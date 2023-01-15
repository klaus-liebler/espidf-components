#pragma once

#include <inttypes.h>
#include <errorcodes.hh>
#include <cmath>
#include "esp_log.h"
#define TAG "pidcontroller"

namespace PID_T1
{

  enum class Direction
  {
    DIRECT,
    REVERSE
  };
  enum class Mode
  {
    OFF,        // output is not written
    OPENLOOP,   // output equals input
    CLOSEDLOOP, // output is set according to PID algorithm
  };

  enum class AntiWindup
  {
    OFF,
    ON_LIMIT_Y_0,
    ON_SWICH_OFF_INTEGRATOR,
  };

  template <class T>
  class Controller
  {

  public:
    Controller(
        T *input,
        T *output,
        T *setpoint,
        T outputMin,
        T outputMax,
        Mode mode,
        AntiWindup antiWindup,
        Direction direction,
        int64_t cycleTimeMs) : input(input), output(output), setpoint(setpoint), outputMin(outputMin), outputMax(outputMax), mode(mode), antiWindup(antiWindup), direction(direction), cycleTimeMs(cycleTimeMs)
    {
    }
    ErrorCode Compute(int64_t nowMs)
    {
      uint32_t timeChangeMs = (nowMs - lastTimeMs);
      if (timeChangeMs < cycleTimeMs)
        return ErrorCode::OBJECT_NOT_CHANGED;

      T e_0{0};
      switch (mode)
      {
      case Mode::CLOSEDLOOP:
        e_0 = (*this->setpoint - *this->input);
        break;
      case Mode::OPENLOOP:
        e_0 = *this->setpoint;
        break;
      default:
        return ErrorCode::TEMPORARYLY_NOT_AVAILABLE;
      }

      T y_0{0};
      if (antiWindup == AntiWindup::ON_SWICH_OFF_INTEGRATOR)
      {
        if (y_1 >= outputMax || y_1 <= outputMin)
        {
          y_0 = -a1 * y_1 - a2 * y_2 + b0_windup * e_0 + b1_windup * e_1 + b2_windup * e_2;
          y_0 = std::min(y_0, outputMax);
          y_0 = std::max(y_0, outputMin);
          *this->output = y_0;
          ESP_LOGI(TAG, "Error = %F, Raw Output %F, Output %F (in windup)", e_0, y_0, *this->output);
        }
        else
        {
          y_0 = -a1 * y_1 - a2 * y_2 + b0 * e_0 + b1 * e_1 + b2 * e_2;
          y_0 = std::min(y_0, outputMax);
          y_0 = std::max(y_0, outputMin);
          *this->output = y_0;
          ESP_LOGI(TAG, "Error = %F, Raw Output %F, Output %F (no windup)", e_0, y_0, *this->output);
        }
      }
      else if (antiWindup == AntiWindup::ON_LIMIT_Y_0)
      {
        // do anti windup by limiting y_0
        y_0 = -a1 * y_1 - a2 * y_2 + b0 * e_0 + b1 * e_1 + b2 * e_2;
        y_0 = std::min(y_0, outputMax);
        y_0 = std::max(y_0, outputMin);
        *this->output = y_0;
      }
      else
      {
        // limit output without affecting y_0
        y_0 = -a1 * y_1 - a2 * y_2 + b0 * e_0 + b1 * e_1 + b2 * e_2;
        *this->output = std::min(y_0, outputMax);
        *this->output = std::max(y_0, outputMin);
      }

      y_2 = y_1;
      y_1 = y_0;
      e_2 = e_1;
      e_1 = e_0;
      lastTimeMs = nowMs;
      return ErrorCode::OK;
    }
    ErrorCode SetMode(Mode nextMode, int64_t nowMs)
    {
      if (this->mode == nextMode)
        return ErrorCode::OBJECT_NOT_CHANGED;
      if (nextMode == Mode::CLOSEDLOOP)
      {
        y_1 = y_2 = e_2 = e_1 = 0;
        this->lastTimeMs = nowMs;
      }
      this->mode = nextMode;
      return ErrorCode::OK;
    }
    /**
     * Rule of Thumb: t_1_msecs=0.2*tv_msecs
     * Hat der analoge PID-T1-Regler zum Beispiel die Parameter KPR=10, Tn=4, Tv=0,5 und TT1=0,2*T_v= 0,1 und soll die Abtastperiode T= 0,05s betragen, dann sind a1=-1,67,a2=0,67, b0=43,4, b1=-83,31 und b2=39,96
     */
    ErrorCode SetKpTnTv(T kp, int64_t tn_msecs, int64_t tv_msecs, int64_t t_1_msecs)
    {
      if (kp == this->kpAsSetByUser && tn_msecs == this->tnAsSetByUser && tv_msecs == this->tvAsSetByUser)
        return ErrorCode::OBJECT_NOT_CHANGED;
      if (kp < 0 || tn_msecs <= 0 || tv_msecs < 0)
        return ErrorCode::INVALID_ARGUMENT_VALUES;

      this->kpAsSetByUser = kp;
      this->tnAsSetByUser = tn_msecs;
      this->tvAsSetByUser = tv_msecs;

      T Tn_s = (T)(tn_msecs) / 1000.0;
      T Tv_s = (T)(tv_msecs) / 1000.0;
      double Tcyc_s = (T)cycleTimeMs / 1000.0;
      double T_1_s = (T)(t_1_msecs) / 1000.0;

      double b_factor = (kp / (1 + (T_1_s / Tcyc_s)));
      double tv_t1_T = ((Tv_s + T_1_s) / (Tcyc_s));
      double two_tn = 2 * Tn_s;
      a1 = -1 - ((T_1_s) / (T_1_s + Tcyc_s));
      a2 = T_1_s / (T_1_s + Tcyc_s);
      b0 = b_factor * (+1 + (Tcyc_s + T_1_s) / two_tn + tv_t1_T);
      b1 = b_factor * (-1 + Tcyc_s / two_tn - 2 * tv_t1_T);
      b2 = b_factor * (+0 - T_1_s / two_tn + tv_t1_T);
      b0_windup = b_factor * (+1 + tv_t1_T);
      b1_windup = b_factor * (-1 - 2 * tv_t1_T);
      b2_windup = b_factor * (+0 + tv_t1_T);

      ESP_LOGI(TAG, "K_PR=%F Tn_s=%F Tv_s=%F Tcyc_s=%F T_1_s=%F a1=%F a2=%F b0=%F b1=%F b2=%F", kp, Tn_s, Tv_s, Tcyc_s, T_1_s, this->a1, this->a2, this->b0, this->b1, this->b2);
      return ErrorCode::OK;
    }
    ErrorCode Reset()
    {
      y_1 = y_2 = e_1 = e_2 = 0;
      return ErrorCode::OK;
    }

  private:
    T *input;    // * Pointers to the Input, Output, and Setpoint variables
    T *output;   //   This creates a hard link between the variables and the
    T *setpoint; //   PID, freeing the user from having to constantly tell us
                 //   what these values are.  with pointers we'll just know.

    T a1;
    T a2;
    T b0;
    T b1;
    T b2;
    T b0_windup;
    T b1_windup;
    T b2_windup;
    T y_1;
    T y_2;
    T e_1;
    T e_2;

    T kpAsSetByUser = 0.0;
    int64_t tnAsSetByUser = INT64_MAX;
    int64_t tvAsSetByUser = 0;

    T outputMin, outputMax;

    Mode mode;
    AntiWindup antiWindup;
    Direction direction;
    int64_t cycleTimeMs;

    int64_t lastTimeMs;
  };
}
#undef TAG