/**
 *******************************************************************************
 * @file        interface_dp.h
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
#ifndef _FSC_INTERFACE_DP_H
#define _FSC_INTERFACE_DP_H

#include "platform.h"
#include "dp_types.h"
#include "Port.h"

#ifdef FSC_HAVE_DP
/*
 *  Functions in the core to be called by the system
 * Initiate status request - called by 'system' to get status of port partner
 */
void requestDpStatus(Port_t *port);
/*
 *  Initiate config request - called by 'system' to configure port partner
 */
void requestDpConfig(Port_t *port, DisplayPortConfig_t in);

/* Functions in the core to be called by the system
 *  functions for the system to implement
 */
void informStatus(Port_t *port, DisplayPortStatus_t stat);
void updateStatusData(Port_t *port);
void informConfigResult (Port_t *port, FSC_BOOL success);
FSC_BOOL DpReconfigure(Port_t *port, DisplayPortConfig_t config);

#endif /* FSC_HAVE_DP */

#endif /* _FSC_INTERFACE_DP_H */
