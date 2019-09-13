/**
 *******************************************************************************
 * @file        timer.c
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Implements a generic timer countdown mechanism.
 *
 * Software License Agreement:
 *
 * The software supplied herewith by ON Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *******************************************************************************
 */

#include "timer.h"
#include "platform.h"

void TimerStart(struct TimerObj *obj, FSC_U32 time) {
  /* Grab the current time stamp and store the wait period. */
  /* Time must be > 0 */
  obj->starttime_ = platform_get_system_time();
  obj->period_ = time;

  obj->disablecount_ = TIMER_DISABLE_COUNT;

  if (obj->period_ == 0) obj->period_ = 1;
}

void TimerRestart(struct TimerObj *obj) {
  /* Grab the current time stamp for the next period. */
  obj->starttime_ = platform_get_system_time();
}

void TimerDisable(struct TimerObj *obj) {
  /* Zero means disabled */
  obj->starttime_ = obj->period_ = 0;
}

FSC_BOOL TimerDisabled(struct TimerObj *obj) {
  /* Zero means disabled */
  return (obj->period_ == 0) ? TRUE : FALSE;
}

FSC_BOOL TimerExpired(struct TimerObj *obj) {
  FSC_U32 currenttime = platform_get_system_time();
  FSC_U32 endtime = obj->starttime_ + obj->period_;
  FSC_BOOL result = FALSE;

  if (TimerDisabled(obj)) {
      /* Disabled */
      /* TODO - possible cases where this return value might case issue? */
      result = FALSE;
  }
  else if (endtime > obj->starttime_) {
    /* No roll-over expected */
    if (currenttime == obj->starttime_) {
      result = FALSE;   /* Still waiting */
    }
    else if (currenttime > obj->starttime_ && currenttime < endtime) {
      result = FALSE;   /* Still waiting */
    }
    else if (currenttime > obj->starttime_ && currenttime > endtime) {
      result = TRUE;    /* Done! */
    }
    else {
      result = TRUE;    /* Ticker_ rolled over - Done! */
    }
  }
  else {
    /* Roll-over expected */
    if (currenttime == obj->starttime_) {
      result = FALSE;   /* Still waiting */
    }
    else if (currenttime > obj->starttime_ && currenttime > endtime) {
      result = FALSE;   /* Still waiting */
    }
    else if (currenttime < obj->starttime_ && currenttime < endtime) {
      result = FALSE;   /* Ticker_ rolled over - Still waiting */
    }
    else {
      result = TRUE;    /* Ticker_ rolled over - Done! */
    }
  }

  /* Check for auto-disable if expired and not explicitly disabled */
  if (result) {
    if (obj->disablecount_-- == 0) {
      TimerDisable(obj);
    }
  }

  return result;
}

FSC_U32 TimerRemaining(struct TimerObj *obj)
{
  FSC_U32 currenttime = platform_get_system_time();
  FSC_U32 endtime = obj->starttime_ + obj->period_;

  if (TimerDisabled(obj)) {
    return 0;
  }

  /* If expired before it could be handled, return a minimum delay */
  if (TimerExpired(obj)) {
    return 1;
  }

  /* Check for rollover... */
  if (endtime < currenttime) {
    return (0xFFFFFFFF - currenttime) - endtime;
  }
  else {
    return endtime - currenttime;
  }
}

