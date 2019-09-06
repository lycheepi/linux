/**
 *******************************************************************************
 * @file        vdm_config.h
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
#ifndef __FSC_VDM_CONFIG_H__
#define __FSC_VDM_CONFIG_H__

#include "platform.h"
#include "Port.h"

#ifdef FSC_HAVE_VDM
/* Functions to configure VDM code! */
void ConfigureVdmResponses(Port_t *port, FSC_U8* bytes);
void ReadVdmConfiguration(Port_t *port, FSC_U8* data);

/* Functions specifically to configure DisplayPort code! */
#ifdef FSC_HAVE_DP
void configDp (Port_t *port, FSC_BOOL enabled, FSC_U32 status);
void configAutoDpModeEntry (Port_t *port, FSC_BOOL enabled, FSC_U32 mask, FSC_U32 value);
void WriteDpControls(Port_t *port, FSC_U8* data);
void ReadDpControls(Port_t *port, FSC_U8* data);
void ReadDpStatus(Port_t *port, FSC_U8* data);
#endif /* FSC_HAVE_DP */

#endif /* FSC_HAVE_VDM */

#endif /* __FSC_VDM_CONFIG_H__ */

