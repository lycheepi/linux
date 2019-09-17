/**
 *******************************************************************************
 * @file        Log.c
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Implements a generic state machine log interface.
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

#include "Log.h"

#ifdef FSC_DEBUG

void InitializeStateLog(StateLog *log)
{
    log->Count = 0;
    log->End = 0;
    log->Start = 0;
}

FSC_BOOL WriteStateLog(StateLog *log, FSC_U16 state, FSC_U32 time)
{
    if(!IsStateLogFull(log))
    {
        FSC_U8 index = log->End;
        log->logQueue[index].state = state;
        log->logQueue[index].time_s = time >> 16;     /* Upper 16: seconds */
        log->logQueue[index].time_ms = time & 0xFFFF; /* Lower 16: 0.1ms */

        log->End += 1;
        if(log->End == LOG_SIZE)
        {
            log->End = 0;
        }

        log->Count += 1;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

FSC_BOOL ReadStateLog(StateLog *log, FSC_U16 * state,
                      FSC_U16 * time_ms, FSC_U16 * time_s)
{
    if(!IsStateLogEmpty(log))
    {
        FSC_U8 index = log->Start;
        *state = log->logQueue[index].state;
        *time_ms = log->logQueue[index].time_ms;
        *time_s = log->logQueue[index].time_s;

        log->Start += 1;
        if(log->Start == LOG_SIZE)
        {
            log->Start = 0;
        }

        log->Count -= 1;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

FSC_U32 GetStateLog(StateLog *log, FSC_U8 *data, FSC_U8 bufLen)
{
    FSC_S32 i;
    FSC_S32 entries = log->Count;
    FSC_U16 state_temp;
    FSC_U16 time_tms_temp;
    FSC_U16 time_s_temp;
    FSC_U32 len = 0;

    for (i = 0; i < entries; i++)
    {
        if (bufLen < 5 ) { break; }
        if (ReadStateLog(log, &state_temp, &time_tms_temp, &time_s_temp))
        {
            data[len++] = state_temp;
            data[len++] = (FSC_U8) time_tms_temp;
            data[len++] = (time_tms_temp >> 8);
            data[len++] = (FSC_U8) time_s_temp;
            data[len++] = (time_s_temp) >> 8;
            bufLen -= 5;
        }
    }

    return len;
}
FSC_BOOL IsStateLogFull(StateLog *log)
{
    return (log->Count == LOG_SIZE) ? TRUE : FALSE;
}

FSC_BOOL IsStateLogEmpty(StateLog *log)
{
    return (!log->Count) ? TRUE : FALSE;
}

#endif /* FSC_DEBUG */

