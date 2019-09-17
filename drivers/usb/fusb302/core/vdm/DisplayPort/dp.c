/**
 *******************************************************************************
 * @file        dp.c
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
#include "dp.h"
#include "interface_dp.h"
#include "vdm.h"
#include "bitfield_translators.h"
#include "PD_Types.h"

#ifdef FSC_HAVE_DP

void initializeDp(Port_t *port)
{
    port->DpCaps.word = 0x0;
    port->DpStatus.word = 0x0;
    port->DpConfig.word = 0x0;

    port->DpPpStatus.word = 0x0;
    port->DpPpRequestedConfig.word = 0x0;
    port->DpPpConfig.word = 0x0;

    port->DpModeEntryMask.word = 0x0;
    port->DpModeEntryValue.word = 0x0;

    port->DpEnabled = FALSE;
    port->DpAutoModeEntryEnabled = FALSE;
    port->DpModeEntered = 0x0;
}

void resetDp(Port_t *port)
{
    port->DpConfig.word = 0x0;

    port->DpPpStatus.word = 0x0;
    port->DpPpRequestedConfig.word = 0x0;
    port->DpPpConfig.word = 0x0;

    port->DpModeEntered = 0x0;
}

/*
 * Process special DisplayPort commands
 * Returns TRUE when the message isn't processed
 * and FALSE otherwise
 */
FSC_BOOL processDpCommand(Port_t *port, FSC_U32* arr_in)
{
    doDataObject_t          __svdmh_in = {0};
    DisplayPortStatus_t     __stat;
    DisplayPortConfig_t     __config;

    if (port->DpEnabled == FALSE) return TRUE;
    __svdmh_in.object = arr_in[0];

    switch (__svdmh_in.SVDM.Command) {
    case DP_COMMAND_STATUS:
        if (__svdmh_in.SVDM.CommandType == INITIATOR) {
            if (port->DpModeEntered == FALSE) return TRUE;
            __stat.word = arr_in[1];
            informStatus(port, __stat);
            /* get updated info from system */
            updateStatusData(port);
            /* send it out */
            sendStatusData(port, __svdmh_in);
        } else {
            __stat.word = arr_in[1];
            informStatus(port, __stat);
        }
        break;
    case DP_COMMAND_CONFIG:
        if (__svdmh_in.SVDM.CommandType == INITIATOR) {
            if (port->DpModeEntered == FALSE) return TRUE;
            __config.word = arr_in[1];
            if (DpReconfigure(port, __config) == TRUE) {
                /* if pin reconfig is successful */
                replyToConfig(port, __svdmh_in, TRUE);
            } else {
                /* if pin reconfig is NOT successful */
                replyToConfig(port, __svdmh_in, FALSE);
            }
        } else {
            if (__svdmh_in.SVDM.CommandType == RESPONDER_ACK) {
                informConfigResult(port, TRUE);
            } else {
                informConfigResult(port, FALSE);
            }
        }
        break;
    default:
        /* command not recognized */
        return TRUE;
    }
    return FALSE;
}

void sendStatusData(Port_t *port, doDataObject_t svdmh_in)
{
    doDataObject_t  __svdmh_out = {0};
    FSC_U32         __length_out;
    FSC_U32         __arr_out[2] = {0};

    __length_out = 0;

    /* reflect most fields */
    __svdmh_out.object = svdmh_in.object;

    __svdmh_out.SVDM.Version        = STRUCTURED_VDM_VERSION;
    __svdmh_out.SVDM.CommandType    = RESPONDER_ACK;

    __arr_out[0] = __svdmh_out.object;
    __length_out++;

    __arr_out[1] = port->DpStatus.word;
    __length_out++;

    sendVdmMessage(port, SOP_TYPE_SOP, __arr_out, __length_out, port->PolicyState);
}

void replyToConfig(Port_t *port, doDataObject_t svdmh_in, FSC_BOOL success)
{
    doDataObject_t  __svdmh_out = {0};
    FSC_U32          __length_out;
    FSC_U32         __arr_out[2] = {0};

    __length_out = 0;

    /* reflect most fields */
    __svdmh_out.object = svdmh_in.object;

    __svdmh_out.SVDM.Version    = STRUCTURED_VDM_VERSION;
    if (success == TRUE) {
        __svdmh_out.SVDM.CommandType = RESPONDER_ACK;
    } else {
        __svdmh_out.SVDM.CommandType    = RESPONDER_NAK;
    }

    __arr_out[0] = __svdmh_out.object;
    __length_out++;

    sendVdmMessage(port, SOP_TYPE_SOP, __arr_out, __length_out, port->PolicyState);
}

/*
 * Evaluate DP Mode Entry
 * Returns TRUE if mode can be entered, FALSE otherwise
 */
FSC_BOOL dpEvaluateModeEntry(Port_t *port, FSC_U32 mode_in)
{
    DisplayPortCaps_t field_mask;
    DisplayPortCaps_t temp;

    if (port->DpEnabled == FALSE) return FALSE;
    if (port->DpAutoModeEntryEnabled == FALSE) return FALSE;

    /*
     * mask works on fields at a time, so fix that here for incomplete values
     * field must be set to all 0s in order to be unmasked
     */
    field_mask.word = port->DpModeEntryMask.word;
    if (field_mask.field0) field_mask.field0 = 0x3;
    if (field_mask.field1) field_mask.field1 = 0x3;
    if (field_mask.field2) field_mask.field2 = 0x1;
    if (field_mask.field3) field_mask.field3 = 0x1;
    if (field_mask.field4) field_mask.field4 = 0x3F;
    if (field_mask.field5) field_mask.field5 = 0x1F;
    field_mask.fieldrsvd0 = 0x3;
    field_mask.fieldrsvd1 = 0x3;
    field_mask.fieldrsvd2 = 0x7FF;

    /* for unmasked fields, at least one bit must match */
    temp.word = mode_in & port->DpModeEntryValue.word;

    /* then, forget about the masked fields */
    temp.word = temp.word | field_mask.word;

    /* at this point, if every field is non-zero, enter the mode */
    if ((temp.field0 != 0) && (temp.field1 != 0) && (temp.field2 != 0)
            && (temp.field3 != 0) && (temp.field4 != 0) && (temp.field5 != 0))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void requestDpStatus(Port_t *port)
{
    doDataObject_t  __svdmh = {0};
    FSC_U32          __length = 0;
    FSC_U32          __arr[2] = {0};

    /* DisplayPort SID */
    __svdmh.SVDM.SVID           = DP_SID;
    /* structured VDM Header */
    __svdmh.SVDM.VDMType        = STRUCTURED_VDM;
    /* version 1.0 = 0 */
    __svdmh.SVDM.Version        = STRUCTURED_VDM_VERSION;
    /* saved mode position */
    __svdmh.SVDM.ObjPos         = port->DpModeEntered & 0x7;
    /* we are initiating */
    __svdmh.SVDM.CommandType    = INITIATOR;
    /* DisplayPort Status */
    __svdmh.SVDM.Command        = DP_COMMAND_STATUS;

    __arr[0] = __svdmh.object;
    __length++;

    __arr[1] = port->DpStatus.word;
    __length++;

    sendVdmMessageWithTimeout(port, SOP_TYPE_SOP, __arr, __length, peDpRequestStatus);
}

/* Initiate config request - called by 'system' to configure port partner */
void requestDpConfig(Port_t *port, DisplayPortConfig_t in)
{
    doDataObject_t  __svdmh = {0};
    FSC_U32          __length = 0;
    FSC_U32          __arr[2] = {0};

    port->DpPpRequestedConfig.word = in.word;

    /* DisplayPort SID */
    __svdmh.SVDM.SVID           = DP_SID;
    /* structured VDM Header */
    __svdmh.SVDM.VDMType        = STRUCTURED_VDM;
    /* version 1.0 = 0 */
    __svdmh.SVDM.Version        = STRUCTURED_VDM_VERSION;
    /* saved mode position */
    __svdmh.SVDM.ObjPos         = port->DpModeEntered & 0x7;
    /* we are initiating */
    __svdmh.SVDM.CommandType    = INITIATOR;
    /* DisplayPort Config */
    __svdmh.SVDM.Command        = DP_COMMAND_CONFIG;

    __arr[0] = __svdmh.object;
    __length++;

    __arr[1] = in.word;
    __length++;

    sendVdmMessage(port, SOP_TYPE_SOP, __arr, __length, port->PolicyState);
}

#endif /* FSC_HAVE_DP */
