/**
 *******************************************************************************
 * @file        dp.h
 * @author      ON Semiconductor USB-PD Firmware Team
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
#ifdef FSC_HAVE_DP

#ifndef __DISPLAYPORT_DP_H__
#define __DISPLAYPORT_DP_H__

#include "platform.h"
#include "PD_Types.h"
#include "vdm_types.h"
#include "dp_types.h"
#include "Port.h"

#ifdef FSC_HAVE_DP

/* Initialize stuff! */
void initializeDp(Port_t *port);

/* reset stuff - for detaches */
void resetDp(Port_t *port);

/* Process incoming DisplayPort messages! */
FSC_BOOL processDpCommand(Port_t *port, FSC_U32* arr_in);

/* Internal function to send Status data to port partner */
void sendStatusData(Port_t *port, doDataObject_t svdmh_in);

/* Internal function to reply to a Config request (to port partner) */
void replyToConfig(Port_t *port, doDataObject_t svdmh_in, FSC_BOOL success);

/* Evaluate a mode VDO for mode entry */
FSC_BOOL dpEvaluateModeEntry(Port_t *port, FSC_U32 mode_in);

#endif  // FSC_HAVE_DP

#endif /* __DISPLAYPORT_DP_H__ */

#endif // FSC_HAVE_DP
