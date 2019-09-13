/**
 *******************************************************************************
 * @file        Log.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines a generic state machine log interface.
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

#ifndef FSC_LOG_H
#define	FSC_LOG_H

#include "platform.h"

#ifdef FSC_DEBUG

#define LOG_SIZE 64

typedef struct{
    FSC_U16 state;
    FSC_U16 time_ms;
    FSC_U16 time_s;
} StateLogEntry;

typedef struct{
    StateLogEntry logQueue[ LOG_SIZE ];
    FSC_U8 Start;
    FSC_U8 End;
    FSC_U8 Count;
} StateLog;

/**
 * @brief Initializes the log values.
 * 
 * @param log Pointer to the log structure
 * @return None
 */
void InitializeStateLog(StateLog *log);

/**
 * @brief Write a single state entry to the log and advance the log pointer
 * 
 * @param log Pointer to the log structure
 * @param state Current state enum value
 * @param time Packed value - upper 16: seconds, lower 16: 0.1ms
 * @return TRUE if successful (i.e. the log isn't full)
 */
FSC_BOOL WriteStateLog(StateLog *log, FSC_U16 state, FSC_U32 time);

/**
 * @brief Read a single entry from the log and advance the log pointer
 * 
 * @param log Pointer to the log structure
 * @param state Current state enum value
 * @param time_tenthms Timestamp (fraction value - 0.1ms resolution)
 * @param time_s Timestamp (seconds value)
 * @return TRUE if successful (i.e. the log isn't empty)
 */
FSC_BOOL ReadStateLog(StateLog *log, FSC_U16 *state,
                      FSC_U16 *time_tenthms, FSC_U16 *time_s);

/**
 * @brief Fill a byte buffer with log data
 *
 * @param log Pointer to the log structure
 * @param data Byte array to copy log data into
 * @param bufLen Available space in data buffer
 * @return Number of bytes written into buffer
 */
FSC_U32 GetStateLog(StateLog *log, FSC_U8 *data, FSC_U8 bufLen);

/**
 * @brief Check state of log, whether Full or Empty.
 * 
 * @param log Pointer to the log structure
 * @return TRUE or FALSE, whether Full or Empty
 */
FSC_BOOL IsStateLogFull(StateLog *log);
FSC_BOOL IsStateLogEmpty(StateLog *log);

#endif // FSC_DEBUG

#endif	/* FSC_LOG_H */

