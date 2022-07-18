#pragma once

#include <inttypes.h>
#include <errorcodes.hh>
#include "esp_log.h"
#define TAG "pidcontroller"

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
template <class T>
class PIDController
{

public:
  PIDController(
      T *input,
      T *output,
      T *setpoint,
      T outputMin,
      T outputMax,
      Mode mode,
      Direction direction,
      int64_t cycleTimeMs) : input(input), output(output), setpoint(setpoint), outputMin(outputMin), outputMax(outputMax), mode(mode), direction(direction), cycleTimeMs(cycleTimeMs)
  {
  }
  ErrorCode Compute(int64_t nowMs)
  {
    if (this->mode == Mode::OFF)
      return ErrorCode::TEMPORARYLY_NOT_AVAILABLE;
    uint32_t timeChangeMs = (nowMs - lastTimeMs);
    if (timeChangeMs < cycleTimeMs)
      return ErrorCode::OBJECT_NOT_CHANGED;

    /*Compute all the working error variables*/
    double input = *this->input;

    double Y_P = kp * (*this->setpoint - input);
    this->Y_I += (ki * Y_P);
    double Y_D = kd * (Y_P - last_Y_P);

    // limit integrator
    this->Y_I = Y_I > outputMax ? outputMax : Y_I < outputMin ? outputMin
                                                              : Y_I;

    double output = Y_P + Y_I + Y_D;

    // limit output & anti integrator windup
    if (output > outputMax)
    {
      output = outputMax;
      this->Y_I -= (ki * Y_P);
    }
    else if (output < outputMin)
    {
      output = outputMin;
      this->Y_I -= (ki * Y_P);
    }

    *this->output = output;

    last_Y_P = Y_P;
    lastTimeMs = nowMs;
    return ErrorCode::OK;
  }
  ErrorCode SetMode(Mode nextMode, int64_t nowMs)
  {
    if (this->mode == nextMode)
      return ErrorCode::OBJECT_NOT_CHANGED;
    if (nextMode == Mode::CLOSEDLOOP)
    {
      this->Y_I = 0;
      this->last_Y_P = kp * (*this->setpoint - *this->input);
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

    T Tn_s = (T)(tn_msecs)/1000.0;
    T Tv_s = (T)(tv_msecs)/1000.0;
    double cycleTimeInSeconds = (double)cycleTimeMs / 1000.0;
    

    this->kp = kp;
    this->ki = (1 / Tn_s) * cycleTimeInSeconds;
    this->kd = Tv_s / cycleTimeInSeconds;
    ESP_LOGI(TAG, "kp=%F ki=%F kd=%F", this->kp, this->ki, this->kd);
    return ErrorCode::OK;
  }
  ErrorCode Reset(){
    this->Y_I=0;
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

  T kpAsSetByUser = 0.0;
  int64_t tnAsSetByUser = INT64_MAX;
  int64_t tvAsSetByUser = 0;

  T outputMin, outputMax;

  Mode mode;
  Direction direction;
  int64_t cycleTimeMs;

  int64_t lastTimeMs;
  T Y_I;
  T last_Y_P;
};

#undef TAG