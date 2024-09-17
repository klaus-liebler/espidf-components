#pragma once

#include <cinttypes>
#include <errorcodes.hh>
#include <cmath>
#include "esp_log.h"
#define TAG "pidcontroller"
namespace PID
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
    ON_LIMIT_INTEGRATOR,
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
      if (this->mode == Mode::OFF)
        return ErrorCode::TEMPORARYLY_NOT_AVAILABLE;
      if(this->mode == Mode::OPENLOOP){
        *output=*setpoint;
        return ErrorCode::OK;
      }
      int64_t timeChangeMs = (nowMs - lastTimeMs);
      if (timeChangeMs < cycleTimeMs)
        return ErrorCode::OBJECT_NOT_CHANGED;

      /*Compute all the working error variables*/
      T input = *this->input;
      T err=*this->setpoint - input;
      //Proportional fraction
      T Y_P = kp * err;

      //Derivative fraction
      T Y_D = kd * (Y_P - last_Y_P);

      //Integral fraction
      //first, accumulate
      T Y_I_local{0};
      bool windupActive{true};
      if(Y_P+Y_D > outputMin-workingPointOffset && Y_P+Y_D < outputMax-workingPointOffset){
        Y_I_local = this->Y_I+(ki * Y_P);
        windupActive=false;
      }
      
      /*Bei dieser Logik wird der Integrator auf einen bestimmten Wert gesetzt, der dann ggf. nicht schnell genug abgebaut wird
      T maxY_I=outputMax-workingPointOffset-Y_P-Y_D;
      T minY_I=outputMin-workingPointOffset-Y_P-Y_D;
      if(Y_I>maxY_I && Y_I<minY_I){
        //if there is no valid Y_I, because Y_P and Y_D are already too large, do the best we can: set Y_I=0
        Y_I=0;
        windupActive=true; 
      }else if (Y_I> maxY_I){//if it is too large, set to the largest possible value
        Y_I=maxY_I;
        windupActive=true;
      }else if(Y_I<minY_I){//if Y_i is too small, set it to the smallest value
        Y_I=minY_I;
        windupActive=true;
      }
      */

      *this->output = std::clamp(Y_P+Y_D+Y_I_local+workingPointOffset, outputMin, outputMax);

      ESP_LOGI(TAG, "err=%.1f, PID=(%.1f, %.1f, %.1f), anti-windup %s", err, Y_P, Y_I_local, Y_D, windupActive?"on":"off");

      this->Y_I=Y_I_local;
      last_Y_P = Y_P;
      lastTimeMs = nowMs;
      return ErrorCode::OK;
    }
    
    ErrorCode SetWorkingPointOffset(T workingPointOffset){
      this->workingPointOffset=workingPointOffset;
      return ErrorCode::OK;
    }
    
    
    ErrorCode SetMode(Mode nextMode, int64_t nowMs)
    {
      if (this->mode == nextMode)
        return ErrorCode::OBJECT_NOT_CHANGED;
      if (nextMode == Mode::CLOSEDLOOP)
      {
        Reset();
        this->lastTimeMs = nowMs;
      }
      this->mode = nextMode;
      return ErrorCode::OK;
    }


    ErrorCode SetKpTnTv(T kp, int64_t tn_msecs, int64_t tv_msecs)
    {
      if (kp == this->kpAsSetByUser && tn_msecs == this->tnAsSetByUser && tv_msecs == this->tvAsSetByUser)
        return ErrorCode::OBJECT_NOT_CHANGED;
      if (kp < 0 || tn_msecs < 0 || tv_msecs < 0)
        return ErrorCode::INVALID_ARGUMENT_VALUES;

      this->kpAsSetByUser = kp;
      this->tnAsSetByUser = tn_msecs;
      this->tvAsSetByUser = tv_msecs;

      T Tn_s = (T)(tn_msecs) / 1000.0;
      T Tv_s = (T)(tv_msecs) / 1000.0;
      float cycleTimeInSeconds = (double)cycleTimeMs / 1000.0;

      this->kp = kp;
      this->ki = (1 / Tn_s) * cycleTimeInSeconds;
      this->kd = Tv_s / cycleTimeInSeconds;
      ESP_LOGI(TAG, "kp=%F ki=%F kd=%F", this->kp, this->ki, this->kd);
      return ErrorCode::OK;
    }
    ErrorCode Reset()
    {
      this->Y_I = 0;
      this->last_Y_P = kp * (*this->setpoint - *this->input);
      ESP_LOGD(TAG, "PID-Controller has been resetted to zero values");
      return ErrorCode::OK;
    }

  private:
    T *input;    // * Pointers to the Input, Output, and Setpoint variables
    T *output;   //   This creates a hard link between the variables and the
    T *setpoint; //   PID, freeing the user from having to constantly tell us
                 //   what these values are.  with pointers we'll just know.

    T kp;
    T ki;
    T kd;

    T workingPointOffset{0.0};
    T kpAsSetByUser{0.0};
    T tnAsSetByUser = INT_MAX;
    T tvAsSetByUser = 0;

    T outputMin, outputMax;

    Mode mode;
    AntiWindup antiWindup;
    Direction direction;
    int64_t cycleTimeMs;

    int64_t lastTimeMs;
    T Y_I{0};
    T last_Y_P{0};
  };
}
#undef TAG