/**
 *******************************************************************************
 * @file        vdm_callbacks_defs.h
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

#ifndef __FSC_VDM_CALLBACKS_DEFS_H__
#define __FSC_VDM_CALLBACKS_DEFS_H__
/*
 * This file defines types for callbacks that the VDM block will use.
 * The intention is for the user to define functions that return data 
 * that VDM messages require, ex whether or not to enter a mode.
 */
#ifdef FSC_HAVE_VDM

#include "vdm_types.h"
#include "PD_Types.h"

typedef struct Port Port_t;

typedef Identity (*RequestIdentityInfo)(Port_t *port);

typedef SvidInfo (*RequestSvidInfo)(Port_t *port);

typedef ModesInfo (*RequestModesInfo)(Port_t *port, FSC_U16);

typedef FSC_BOOL (*ModeEntryRequest)(Port_t *port, FSC_U16 svid, FSC_U32 mode_index);

typedef FSC_BOOL (*ModeExitRequest)(Port_t *port, FSC_U16 svid, FSC_U32 mode_index);

typedef FSC_BOOL (*EnterModeResult)(Port_t *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);

typedef void (*ExitModeResult)(Port_t *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);

typedef void (*InformIdentity)(Port_t *port, FSC_BOOL success, SopType sop, Identity id);

typedef void (*InformSvids)(Port_t *port, FSC_BOOL success, SopType sop, SvidInfo svid_info);

typedef void (*InformModes)(Port_t *port, FSC_BOOL success, SopType sop, ModesInfo modes_info);

typedef void (*InformAttention)(Port_t *port, FSC_U16 svid, FSC_U8 mode_index);

/*
 * VDM Manager object, so I can have multiple instances intercommunicating using the same functions!
 */
typedef struct
{
    /* callbacks! */
    RequestIdentityInfo req_id_info;
    RequestSvidInfo req_svid_info;
    RequestModesInfo req_modes_info;
    ModeEntryRequest req_mode_entry;
    ModeExitRequest req_mode_exit;
    EnterModeResult enter_mode_result;
    ExitModeResult exit_mode_result;
    InformIdentity inform_id;
    InformSvids inform_svids;
    InformModes inform_modes;
    InformAttention inform_attention;
} VdmManager;

#endif /* FSC_HAVE_VDM */

#endif /* __FSC_VDM_CALLBACKS_DEFS_H__ */

