/**
 *******************************************************************************
 * @file        dp_system_stubs.c
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

#include "platform.h"
#include "interface_dp.h"

#ifdef FSC_HAVE_DP

void informStatus(Port_t *port, DisplayPortStatus_t stat)
{
    /* TODO: 'system' should implement this */
    /* this function is called to inform the 'system' of the DP status of the port partner */
    port->DpPpStatus.word = stat.word;
}

void informConfigResult (Port_t *port, FSC_BOOL success)
{
    /* TODO: 'system' should implement this */
    /* this function is called when a config message is either ACKd or NAKd by the other side */
    if (success == TRUE) port->DpPpConfig.word = port->DpPpRequestedConfig.word;
}

void updateStatusData(Port_t *port)
{
    /* TODO: 'system' should implement this */
    /* this function is called to get an update of our status - to be sent to the port partner */
}

FSC_BOOL DpReconfigure(Port_t *port, DisplayPortConfig_t config)
{
    /* TODO: 'system' should implement this */
    /* called with a DisplayPort configuration to do! */
    /* return TRUE if/when successful, FALSE otherwise */
    port->DpConfig.word = config.word;
    /* must actually change configurations here before returning TRUE! */
    return TRUE;
}

#endif /* FSC_HAVE_DP */

