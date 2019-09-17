/**
 *******************************************************************************
 * @file        vdm.c
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

#include "vdm.h"
#include "platform.h"
#include "PDPolicy.h"
#include "PD_Types.h"
#include "vdm_types.h"
#include "bitfield_translators.h"
#include "fsc_vdm_defs.h"

#ifdef FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#endif /* FSC_HAVE_DP */

/*
 * Initialize the VDM Manager (no definition/configuration object necessary).
 * returns 0 on success.
 * */
FSC_S32 initializeVdm(Port_t *port)
{
    port->vdm_timeout = FALSE;
    port->vdm_expectingresponse = FALSE;
    port->AutoModeEntryEnabled = FALSE;

#ifdef FSC_HAVE_DP
    initializeDp(port);
#endif /* FSC_HAVE_DP */

    return 0;
}

/* call this routine to issue Discover Identity commands
 * Discover Identity only valid for SOP/SOP'
 * returns 0 if successful
 * returns >0 if not SOP or SOP', or if Policy State is wrong
 */
FSC_S32 requestDiscoverIdentity(Port_t *port, SopType sop)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState == peSinkReady)
            || (port->PolicyState == peSourceReady))
    {
        /* different states for port partner discovery vs cable discovery */
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP)
        {
            __n_pe = peDfpUfpVdmIdentityRequest;
        }
        else if (sop == SOP_TYPE_SOP1)
        {
            __n_pe = peDfpCblVdmIdentityRequest;
        }
        else
        {
            return 1;
        }

        /* PD SID to be used for Discover Identity command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* does not matter for Discover Identity */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* discover identity command! */
        __vdmh.SVDM.Command = DISCOVER_IDENTITY;

        __arr[0] = __vdmh.object;
        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    /* allow cable discovery in special earlier states */
    else if ((sop == SOP_TYPE_SOP1) &&
            ((port->PolicyState == peSourceStartup)
                    || (port->PolicyState == peSourceDiscovery)))
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peSrcVdmIdentityRequest;
        /* PD SID to be used for Discover Identity command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* does not matter for Discover Identity */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* discover identity command! */
        __vdmh.SVDM.Command = DISCOVER_IDENTITY;

        __arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    else
    {
        return 1;
    }

    return 0;
}

/* call this routine to issue Discover SVID commands
 * Discover SVIDs command valid with SOP*.
 */
FSC_S32 requestDiscoverSvids(Port_t *port, SopType sop)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmSvidsRequest;

        /* PD SID to be used for Discover SVIDs command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* does not matter for Discover SVIDs */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Discover SVIDs command! */
        __vdmh.SVDM.Command = DISCOVER_SVIDS;

        arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, arr, __length, __n_pe);
    }
    return 0;
}

/* call this routine to issue Discover Modes
 * Discover Modes command valid with SOP*.
 */
FSC_S32 requestDiscoverModes(Port_t *port, SopType sop, FSC_U16 svid)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmModesRequest;

        /* Use the SVID that was discovered */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* does not matter for Discover Modes */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Discover MODES command! */
        __vdmh.SVDM.Command = DISCOVER_MODES;

        __arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    return 0;
}

/* DPM (UFP) calls this routine to request sending an attention command */
FSC_S32 requestSendAttention(Port_t *port, SopType sop, FSC_U16 svid,
                             FSC_U8 mode)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    port->originalPolicyState = port->PolicyState;

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        /* Use the SVID that needs attention */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* use the mode index that needs attention */
        __vdmh.SVDM.ObjPos = mode;
        /* we are initiating attention */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Attention command! */
        __vdmh.SVDM.Command = ATTENTION;

        __arr[0] = __vdmh.object;
        __length = 1;
        sendVdmMessage(port, sop, __arr, __length, port->PolicyState);
    }
    return 0;
}

FSC_S32 processDiscoverIdentity(Port_t *port, SopType sop, FSC_U32* arr_in,
                                FSC_U32 length_in)
{
    doDataObject_t __vdmh_in = { 0 };
    doDataObject_t __vdmh_out = { 0 };

    IdHeader __idh;
    CertStatVdo __csvdo;
    Identity __id;
    ProductVdo __pvdo;

    FSC_U32 __arr[7] = { 0 };
    FSC_U32 __length;

    FSC_BOOL __result;

    __vdmh_in.object = arr_in[0];

    /* Must NAK or not respond to Discover ID with wrong SVID */
    if (__vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (__vdmh_in.SVDM.CommandType == INITIATOR)
    {

        if(!Responds_To_Discov_SOP) return 0;

        if ((sop == SOP_TYPE_SOP)
                && ((port->PolicyState == peSourceReady)
                        || (port->PolicyState == peSinkReady)))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peUfpVdmGetIdentity);
            __id = port->vdmm.req_id_info(port);
            SetPEState(port, peUfpVdmSendIdentity);
        }
        else if ((sop == SOP_TYPE_SOP1) && (port->PolicyState == peCblReady))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblGetIdentity);
            __id = port->vdmm.req_id_info(port);

            if (__id.nack)
            {
                SetPEState(port, peCblGetIdentityNak);
            }
            else
            {
                SetPEState(port, peCblSendIdentity);
            }
        }
        else
        {
            return 1;
        }

        /* always use PS_SID for Discover Identity, even on response */
        __vdmh_out.SVDM.SVID = PD_SID;
        /* Discovery Identity is Structured */
        __vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh_out.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* doesn't matter for Discover Identity */
        __vdmh_out.SVDM.ObjPos = 0;
        if (__id.nack)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover Identity */
        __vdmh_out.SVDM.Command = DISCOVER_IDENTITY;
        __arr[0] = __vdmh_out.object;
        __length = 1;

        if (port->PolicyState == peCblGetIdentityNak)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            /* put capabilities into ID Header */
            __idh = __id.id_header;

            /* put test ID into Cert Stat VDO Object */
            __csvdo = __id.cert_stat_vdo;

            __arr[1] = getBitsForIdHeader(__idh);
            __length++;
            __arr[2] = getBitsForCertStatVdo(__csvdo);
            __length++;

            /* Product VDO should be sent for all */
            __pvdo = __id.product_vdo;
            __arr[__length] = getBitsForProductVdo(__pvdo);
            __length++;

            /* Cable VDO should be sent when we are a Passive Cable or Active Cable */
            if ((__idh.product_type == PASSIVE_CABLE)
                    || (__idh.product_type == ACTIVE_CABLE))
            {
                CableVdo cvdo_out;
                cvdo_out = __id.cable_vdo;
                __arr[__length] = getBitsForCableVdo(cvdo_out);
                __length++;
            }

            /* AMA VDO should be sent when we are an AMA! */
            if (__idh.product_type == AMA)
            {
                AmaVdo amavdo_out;
                amavdo_out = __id.ama_vdo;

                __arr[__length] = getBitsForAmaVdo(amavdo_out);
                __length++;
            }
        }

        sendVdmMessage(port, sop, __arr, __length, port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        if (port->vdm_timeout) return 0;
        if ((port->PolicyState != peDfpUfpVdmIdentityRequest)
                && (port->PolicyState != peDfpCblVdmIdentityRequest)
                && (port->PolicyState != peSrcVdmIdentityRequest)) return 0;

        /*
         * Discover Identity responses should have at least VDM Header,
         * ID Header, and Cert Stat VDO
         */
        if (length_in < MIN_DISC_ID_RESP_SIZE)
        {
            SetPEState(port, port->originalPolicyState);
            return 1;
        }
        if ((port->PolicyState == peDfpUfpVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP))
        {
            if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peDfpUfpVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peDfpUfpVdmIdentityNaked);
                port->AutoVdmState = AUTO_VDM_DONE;
            }
        }
        else if ((port->PolicyState == peDfpCblVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP1))
        {
            if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peDfpCblVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peDfpCblVdmIdentityNaked);
            }
        }
        else if ((port->PolicyState == peSrcVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP1))
        {
            if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peSrcVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peSrcVdmIdentityNaked);
            }
        }
        else
        {
            /* TODO: something weird happened. */
        }

        if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
        {
            __id.id_header = getIdHeader(arr_in[1]);
            __id.cert_stat_vdo = getCertStatVdo(arr_in[2]);

            if ((__id.id_header.product_type == HUB)
                    || (__id.id_header.product_type == PERIPHERAL)
                    || (__id.id_header.product_type == AMA))
            {
                __id.has_product_vdo = TRUE;
                /* !!! assuming it is before AMA VDO */
                __id.product_vdo = getProductVdo(arr_in[3]);
            }

            if ((__id.id_header.product_type == PASSIVE_CABLE)
                    || (__id.id_header.product_type == ACTIVE_CABLE))
            {
                __id.has_cable_vdo = TRUE;
                __id.cable_vdo = getCableVdo(arr_in[4]);
            }

            if ((__id.id_header.product_type == AMA))
            {
                __id.has_ama_vdo = TRUE;
                /* !!! assuming it is after Product VDO */
                __id.ama_vdo = getAmaVdo(arr_in[4]);
            }
        }

        __result = (port->PolicyState == peDfpUfpVdmIdentityAcked)
                || (port->PolicyState == peDfpCblVdmIdentityAcked)
                || (port->PolicyState == peSrcVdmIdentityAcked);
        port->vdmm.inform_id(port, __result, sop, __id);
        port->vdm_expectingresponse = FALSE;
        SetPEState(port, port->originalPolicyState);
        TimerDisable(&port->VdmTimer);
        port->VdmTimerStarted = FALSE;
        return 0;
    }
}

FSC_S32 processDiscoverSvids(Port_t *port, SopType sop, FSC_U32* arr_in,
                             FSC_U32 length_in)
{
    doDataObject_t __vdmh_in = { 0 };
    doDataObject_t __vdmh_out = { 0 };

    SvidInfo __svid_info;

    FSC_U32 __i;
    FSC_U16 __top16;
    FSC_U16 __bottom16;

    FSC_U32 __arr[7] = { 0 };
    FSC_U32 __length;

    __vdmh_in.object = arr_in[0];

    /* Must NAK or not respond to Discover SVIDs with wrong SVID */
    if (__vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (__vdmh_in.SVDM.CommandType == INITIATOR)
    {
        if ((sop == SOP_TYPE_SOP)
                && ((port->PolicyState == peSourceReady)
                        || (port->PolicyState == peSinkReady)))
        {
            port->originalPolicyState = port->PolicyState;
            /* assuming that the splitting of SVID info is done outside this block */
            __svid_info = port->vdmm.req_svid_info(port);
            SetPEState(port, peUfpVdmGetSvids);
            SetPEState(port, peUfpVdmSendSvids);
        }
        else if ((sop == SOP_TYPE_SOP1) && (port->PolicyState == peCblReady))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblGetSvids);
            __svid_info = port->vdmm.req_svid_info(port);
            if (__svid_info.nack)
            {
                SetPEState(port, peCblGetSvidsNak);
            }
            else
            {
                SetPEState(port, peCblSendSvids);
            }
        }
        else
        {
            return 1;
        }

        /* always use PS_SID for Discover SVIDs, even on response */
        __vdmh_out.SVDM.SVID = PD_SID;
        /* Discovery SVIDs is Structured */
        __vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh_out.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* doesn't matter for Discover SVIDs */
        __vdmh_out.SVDM.ObjPos = 0;
        if (__svid_info.nack)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover SVIDs */
        __vdmh_out.SVDM.Command = DISCOVER_SVIDS;

        __length = 0;
        __arr[__length] = __vdmh_out.object;
        __length++;

        if (port->PolicyState == peCblGetSvidsNak)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            FSC_U32 i;
            /* prevent segfaults */
            if (__svid_info.num_svids > MAX_NUM_SVIDS)
            {
                SetPEState(port, port->originalPolicyState);
                return 1;
            }

            for (i = 0; i < __svid_info.num_svids; i++)
            {
                /* check if i is even */
                if (!(i & 0x1))
                {
                    __length++;

                    /* setup new word to send */
                    __arr[__length - 1] = 0;

                    /* if even, shift SVID up to the top 16 bits */
                    __arr[__length - 1] |= __svid_info.svids[i];
                    __arr[__length - 1] <<= 16;
                }
                else
                {
                    /* if odd, fill out the bottom 16 bits */
                    __arr[__length - 1] |= __svid_info.svids[i];
                }
            }
        }

        sendVdmMessage(port, sop, __arr, __length, port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        __svid_info.num_svids = 0;

        if (port->PolicyState != peDfpVdmSvidsRequest)
        {
            return 1;
        }
        else if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
        {
            for (__i = 1; __i < length_in; __i++)
            {
                __top16 = (arr_in[__i] >> 16) & 0x0000FFFF;
                __bottom16 = (arr_in[__i] >> 0) & 0x0000FFFF;

                /* if top 16 bits are 0, we're done getting SVIDs */
                if (__top16 == 0x0000)
                {
                    break;
                }
                else
                {
                    __svid_info.svids[2 * (__i - 1)] = __top16;
                    __svid_info.num_svids += 1;
                }
                /* if bottom 16 bits are 0 we're done getting SVIDs */
                if (__bottom16 == 0x0000)
                {
                    break;
                }
                else
                {
                    __svid_info.svids[2 * (__i - 1) + 1] = __bottom16;
                    __svid_info.num_svids += 1;
                }
            }
            SetPEState(port, peDfpVdmSvidsAcked);
        }
        else
        {
            SetPEState(port, peDfpVdmSvidsNaked);
        }

        port->vdmm.inform_svids(port, port->PolicyState == peDfpVdmSvidsAcked,
                                sop, __svid_info);

        port->vdm_expectingresponse = FALSE;
        TimerDisable(&port->VdmTimer);
        port->VdmTimerStarted = FALSE;
        SetPEState(port, port->originalPolicyState);
        return 0;
    }

    return 0;
}

FSC_S32 processDiscoverModes(Port_t *port, SopType sop, FSC_U32* arr_in,
                             FSC_U32 length_in)
{
    doDataObject_t __vdmh_in = { 0 };
    doDataObject_t __vdmh_out = { 0 };

    ModesInfo __modes_info = { 0 };

    FSC_U32 __i;
    FSC_U32 __arr[7] = { 0 };
    FSC_U32 __length;

    __vdmh_in.object = arr_in[0];
    if (__vdmh_in.SVDM.CommandType == INITIATOR)
    {
        if ((sop == SOP_TYPE_SOP)
                && ((port->PolicyState == peSourceReady)
                        || (port->PolicyState == peSinkReady)))
        {
            port->originalPolicyState = port->PolicyState;
            __modes_info = port->vdmm.req_modes_info(port, __vdmh_in.SVDM.SVID);
            SetPEState(port, peUfpVdmGetModes);
            SetPEState(port, peUfpVdmSendModes);
        }
        else if ((sop == SOP_TYPE_SOP1) && (port->PolicyState == peCblReady))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblGetModes);
            __modes_info = port->vdmm.req_modes_info(port, __vdmh_in.SVDM.SVID);

            if (__modes_info.nack)
            {
                SetPEState(port, peCblGetModesNak);
            }
            else
            {
                SetPEState(port, peCblSendModes);
            }
        }
        else
        {
            return 1;
        }

        /* reply with SVID we're being asked about */
        __vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;
        /* Discovery Modes is Structured */
        __vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh_out.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* doesn't matter for Discover Modes */
        __vdmh_out.SVDM.ObjPos = 0;
        if (__modes_info.nack)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover Modes */
        __vdmh_out.SVDM.Command = DISCOVER_MODES;

        __length = 0;
        __arr[__length] = __vdmh_out.object;
        __length++;

        if (port->PolicyState == peCblGetModesNak)
        {
            __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else
        {
            FSC_U32 j;

            for (j = 0; j < __modes_info.num_modes; j++)
            {
                __arr[j + 1] = __modes_info.modes[j];
                __length++;
            }
        }

        sendVdmMessage(port, sop, __arr, __length, port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        if (port->PolicyState != peDfpVdmModesRequest)
        {
            return 1;
        }
        else
        {
            if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                __modes_info.svid = __vdmh_in.SVDM.SVID;
                __modes_info.num_modes = length_in - 1;

                for (__i = 1; __i < length_in; __i++)
                {
                    __modes_info.modes[__i - 1] = arr_in[__i];
                }
                SetPEState(port, peDfpVdmModesAcked);
            }
            else
            {
                SetPEState(port, peDfpVdmModesNaked);
            }

            port->vdmm.inform_modes(port,
                                    port->PolicyState == peDfpVdmModesAcked,
                                    sop, __modes_info);
            port->vdm_expectingresponse = FALSE;
            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
            SetPEState(port, port->originalPolicyState);
        }

        return 0;
    }
}

FSC_S32 processEnterMode(Port_t *port, SopType sop, FSC_U32* arr_in,
                         FSC_U32 length_in)
{
    doDataObject_t __svdmh_in = { 0 };
    doDataObject_t __svdmh_out = { 0 };

    FSC_BOOL __mode_entered;
    FSC_U32 __arr_out[7] = { 0 };
    FSC_U32 __length_out;

    __svdmh_in.object = arr_in[0];
    if (__svdmh_in.SVDM.CommandType == INITIATOR)
    {
        if ((sop == SOP_TYPE_SOP)
                && ((port->PolicyState == peSourceReady)
                        || (port->PolicyState == peSinkReady)))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peUfpVdmEvaluateModeEntry);
            __mode_entered = port->vdmm.req_mode_entry(port,
                                                       __svdmh_in.SVDM.SVID,
                                                       __svdmh_in.SVDM.ObjPos);

            /* if DPM says OK, respond with ACK */
            if (__mode_entered)
            {
                SetPEState(port, peUfpVdmModeEntryAck);
                /* entered mode successfully */
                __svdmh_out.SVDM.CommandType = RESPONDER_ACK;
            }
            else
            {
                SetPEState(port, peUfpVdmModeEntryNak);
                /* NAK if mode not entered */
                __svdmh_out.SVDM.CommandType = RESPONDER_NAK;
            }
        }
        else if ((sop == SOP_TYPE_SOP1) && (port->PolicyState == peCblReady))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblEvaluateModeEntry);
            __mode_entered = port->vdmm.req_mode_entry(port,
                                                       __svdmh_in.SVDM.SVID,
                                                       __svdmh_in.SVDM.ObjPos);
            /* if DPM says OK, respond with ACK */
            if (__mode_entered)
            {
                SetPEState(port, peCblModeEntryAck);
                /* entered mode successfully */
                __svdmh_out.SVDM.CommandType = RESPONDER_ACK;
            }
            else
            {
                SetPEState(port, peCblModeEntryNak);
                /* NAK if mode not entered */
                __svdmh_out.SVDM.CommandType = RESPONDER_NAK;
            }
        }
        else
        {
            return 1;
        }

        /*
         * most of the message response will be the same
         * whether we entered the mode or not
         */
        /* reply with SVID we're being asked about */
        __svdmh_out.SVDM.SVID = __svdmh_in.SVDM.SVID;
        /* Enter Mode is Structured */
        __svdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __svdmh_out.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* reflect the object position */
        __svdmh_out.SVDM.ObjPos = __svdmh_in.SVDM.ObjPos;
        /* Reply with same command, Enter Mode */
        __svdmh_out.SVDM.Command = ENTER_MODE;

        __arr_out[0] = __svdmh_out.object;
        __length_out = 1;

        sendVdmMessage(port, sop, __arr_out, __length_out,
                       port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        if (__svdmh_in.SVDM.CommandType != RESPONDER_ACK)
        {
            SetPEState(port, peDfpVdmModeEntryNaked);
            port->vdmm.enter_mode_result(port, FALSE, __svdmh_in.SVDM.SVID,
                                         __svdmh_in.SVDM.ObjPos);
        }
        else
        {
            SetPEState(port, peDfpVdmModeEntryAcked);
            port->vdmm.enter_mode_result(port, TRUE, __svdmh_in.SVDM.SVID,
                                         __svdmh_in.SVDM.ObjPos);
        }
        SetPEState(port, port->originalPolicyState);
        port->vdm_expectingresponse = FALSE;
        return 0;
    }
}

FSC_S32 processExitMode(Port_t *port, SopType sop, FSC_U32* arr_in,
                        FSC_U32 length_in)
{
    doDataObject_t __vdmh_in = { 0 };
    doDataObject_t __vdmh_out = { 0 };

    FSC_BOOL __mode_exited;
    FSC_U32 __arr[7] = { 0 };
    FSC_U32 __length;

    __vdmh_in.object = arr_in[0];
    if (__vdmh_in.SVDM.CommandType == INITIATOR)
    {
        if ((sop == SOP_TYPE_SOP)
                && ((port->PolicyState == peSourceReady)
                        || (port->PolicyState == peSinkReady)))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peUfpVdmModeExit);
            __mode_exited = port->vdmm.req_mode_exit(port, __vdmh_in.SVDM.SVID,
                                                     __vdmh_in.SVDM.ObjPos);

            /* if DPM says OK, respond with ACK */
            if (__mode_exited)
            {
                SetPEState(port, peUfpVdmModeExitAck);
                /* exited mode successfully */
                __vdmh_out.SVDM.CommandType = RESPONDER_ACK;
            }
            else
            {
                SetPEState(port, peUfpVdmModeExitNak);
                /* NAK if mode not exited */
                __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
            }
        }
        else if ((sop == SOP_TYPE_SOP1) && (port->PolicyState == peCblReady))
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblModeExit);
            __mode_exited = port->vdmm.req_mode_exit(port, __vdmh_in.SVDM.SVID,
                                                     __vdmh_in.SVDM.ObjPos);

            /* if DPM says OK, respond with ACK */
            if (__mode_exited)
            {
                SetPEState(port, peCblModeExitAck);
                /* exited mode successfully */
                __vdmh_out.SVDM.CommandType = RESPONDER_ACK;
            }
            else
            {
                SetPEState(port, peCblModeExitNak);
                /* NAK if mode not exited */
                __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
            }
        }
        else
        {
            return 1;
        }

        /*
         * most of the message response will be the same whether we
         * exited the mode or not
         */
        /* reply with SVID we're being asked about */
        __vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;
        /* Exit Mode is Structured */
        __vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh_out.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* reflect the object position */
        __vdmh_out.SVDM.ObjPos = __vdmh_in.SVDM.ObjPos;
        /* Reply with same command, Exit Mode */
        __vdmh_out.SVDM.Command = EXIT_MODE;

        __arr[0] = __vdmh_out.object;
        __length = 1;

        sendVdmMessage(port, sop, __arr, __length, port->originalPolicyState);
        return 0;
    }
    else
    {
        if (__vdmh_in.SVDM.CommandType == RESPONDER_BUSY)
        {
            port->vdmm.exit_mode_result(port, FALSE, __vdmh_in.SVDM.SVID,
                                        __vdmh_in.SVDM.ObjPos);
            if (port->originalPolicyState == peSourceReady)
            {
                SetPEState(port, peSourceHardReset);
            }
            else if (port->originalPolicyState == peSinkReady)
            {
                SetPEState(port, peSinkHardReset);
            }
            else
            {
                /* TODO: should never reach here, but you never know... */
            }
        }
        else if (__vdmh_in.SVDM.CommandType == RESPONDER_NAK)
        {
            port->vdmm.exit_mode_result(port, FALSE, __vdmh_in.SVDM.SVID,
                                        __vdmh_in.SVDM.ObjPos);
            SetPEState(port, port->originalPolicyState);

            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
        }
        else
        {
            SetPEState(port, peDfpVdmExitModeAcked);
            port->vdmm.exit_mode_result(port, TRUE, __vdmh_in.SVDM.SVID,
                                        __vdmh_in.SVDM.ObjPos);
            SetPEState(port, port->originalPolicyState);

            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
        }
        port->vdm_expectingresponse = FALSE;
        return 0;
    }
}
FSC_S32 processAttention(Port_t *port, SopType sop, FSC_U32* arr_in,
                         FSC_U32 length_in)
{
    doDataObject_t __vdmh_in = { 0 };
    __vdmh_in.object = arr_in[0];

    port->originalPolicyState = port->PolicyState;
    SetPEState(port, peDfpVdmAttentionRequest);
    port->vdmm.inform_attention(port, __vdmh_in.SVDM.SVID,
                                __vdmh_in.SVDM.ObjPos);
    SetPEState(port, port->originalPolicyState);

    return 0;
}
FSC_S32 processSvidSpecific(Port_t *port, SopType sop, FSC_U32* arr_in,
                            FSC_U32 length_in)
{
    doDataObject_t __vdmh_out = { 0 };
    doDataObject_t __vdmh_in = { 0 };
    FSC_U32 __arr[7] = { 0 };
    FSC_U32 __length;

    __vdmh_in.object = arr_in[0];

#ifdef FSC_HAVE_DP
    if (__vdmh_in.SVDM.SVID == DP_SID)
    {
        if (!processDpCommand(port, arr_in))
        {
            return 0; /* DP code will send response, so return */
        }
    }
#endif /* FSC_HAVE_DP */

    /* in this case the command is unrecognized. Reply with a NAK. */
    /* reply with SVID we received */
    __vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;
    /* All are structured in this switch-case */
    __vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
    /* version 1.0 or 2.0 */
    __vdmh_out.SVDM.Version =
        (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
    /* value doesn't matter here */
    __vdmh_out.SVDM.ObjPos = 0;
    /* Command unrecognized, so NAK */
    __vdmh_out.SVDM.CommandType = RESPONDER_NAK;
    /* Reply with same command */
    __vdmh_out.SVDM.Command = __vdmh_in.SVDM.Command;

    __arr[0] = __vdmh_out.object;
    __length = 1;

    sendVdmMessage(port, sop, __arr, __length, port->originalPolicyState);
    return 0;
}

/* returns 0 on success, 1+ otherwise */
FSC_S32 processVdmMessage(Port_t *port, SopType sop, FSC_U32* arr_in,
                          FSC_U32 length_in)
{
    FSC_S32 result = 0;
    doDataObject_t __vdmh_in = { 0 };

    __vdmh_in.object = arr_in[0];

    if (__vdmh_in.SVDM.VDMType == STRUCTURED_VDM)
    {
        if ((port->PdRevContract == USBPDSPECREV2p0 &&
            __vdmh_in.SVDM.Version != V1P0) ||
            (port->PdRevContract == USBPDSPECREV3p0 &&
            __vdmh_in.SVDM.Version != V2P0))
        {
            /* Invalid structured VDM version */
            return 1;
        }

        // different actions for different commands
        switch (__vdmh_in.SVDM.Command)
        {
        case DISCOVER_IDENTITY:
            result = processDiscoverIdentity(port, sop, arr_in, length_in);
            break;
        case DISCOVER_SVIDS:
            result = processDiscoverSvids(port, sop, arr_in, length_in);
            break;
        case DISCOVER_MODES:
            result = processDiscoverModes(port, sop, arr_in, length_in);
            break;
        case ENTER_MODE:
            result = processEnterMode(port, sop, arr_in, length_in);
            break;
        case EXIT_MODE:
            result = processExitMode(port, sop, arr_in, length_in);
            break;
        case ATTENTION:
            result = processAttention(port, sop, arr_in, length_in);
            break;
        default:
            // SVID-Specific commands go here
            result = processSvidSpecific(port, sop, arr_in, length_in);
            break;
        }

        return result;
    }
    else
    {
        /* TODO: Unstructured messages */
        /* Unstructured VDM's not supported at this time */
        if (port->PdRevContract == USBPDSPECREV3p0)
        {
            /* Not supported in PD3.0, ignored in PD2.0 */
            SetPEState(port, peNotSupported);
        }
        return 1;
    }
}

/* call this function to enter a mode */
FSC_S32 requestEnterMode(Port_t *port, SopType sop, FSC_U16 svid,
                         FSC_U32 mode_index)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmModeEntryRequest;

        /* Use SVID specified upon function call */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* select mode */
        __vdmh.SVDM.ObjPos = mode_index;
        /* we are initiating mode entering */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Enter Mode command! */
        __vdmh.SVDM.Command = ENTER_MODE;

        __arr[0] = __vdmh.object;
        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    return 0;
}

/* call this function to exit a mode */
FSC_S32 requestExitMode(Port_t *port, SopType sop, FSC_U16 svid,
                        FSC_U32 mode_index)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmModeExitRequest;

        /* Use SVID specified upon function call */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version =
            (port->PdRevContract == USBPDSPECREV2p0) ? V1P0 : V2P0;
        /* select mode */
        __vdmh.SVDM.ObjPos = mode_index;
        /* we are initiating mode entering */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Exit Mode command! */
        __vdmh.SVDM.Command = EXIT_MODE;

        __arr[0] = __vdmh.object;
        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    return 0;
}

/* call this function to send an attention command to specified SOP type */
FSC_S32 sendAttention(SopType sop, FSC_U32 obj_pos)
{
    // TODO
    return 1;
}

void sendVdmMessageWithTimeout(Port_t *port, SopType sop, FSC_U32* arr,
                               FSC_U32 length, FSC_S32 n_pe)
{
    sendVdmMessage(port, sop, arr, length, n_pe);
    port->vdm_expectingresponse = TRUE;
}

FSC_BOOL expectingVdmResponse(Port_t *port)
{
    return port->vdm_expectingresponse;
}

void startVdmTimer(Port_t *port, FSC_S32 n_pe)
{
    /* start the appropriate timer */
    switch (n_pe)
    {
    case peDfpUfpVdmIdentityRequest:
    case peDfpCblVdmIdentityRequest:
    case peSrcVdmIdentityRequest:
    case peDfpVdmSvidsRequest:
    case peDfpVdmModesRequest:
        TimerStart(&port->VdmTimer, tVDMSenderResponse);
        port->VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeEntryRequest:
        TimerStart(&port->VdmTimer, tVDMWaitModeEntry);
        port->VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeExitRequest:
        TimerStart(&port->VdmTimer, tVDMWaitModeExit);
        port->VdmTimerStarted = TRUE;
        break;
    case peDpRequestStatus:
        TimerStart(&port->VdmTimer, tVDMSenderResponse);
        port->VdmTimerStarted = TRUE;
        break;
    default:
        TimerDisable(&port->VdmTimer);
        /* timeout immediately */
        port->VdmTimerStarted = TRUE;
        break;
    }
}

void sendVdmMessageFailed(Port_t *port)
{
    resetPolicyState(port);
}

/* call this function when VDM Message Timer expires */
void vdmMessageTimeout(Port_t *port)
{
    resetPolicyState(port);
}

void resetPolicyState(Port_t *port)
{
    /* fake empty id Discover Identity for NAKs */
    Identity __id = { 0 };
    SvidInfo __svid_info = { 0 };
    ModesInfo __modes_info = { 0 };

    port->vdm_expectingresponse = FALSE;
    port->VdmTimerStarted = FALSE;

    if (port->PolicyState == peGiveVdm)
    {
        SetPEState(port, port->vdm_next_ps);
    }

    switch (port->PolicyState)
    {
    case peDfpUfpVdmIdentityRequest:
        SetPEState(port, peDfpUfpVdmIdentityNaked);
        /* informing of a NAK */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpCblVdmIdentityRequest:
        SetPEState(port, peDfpCblVdmIdentityNaked);
        /* informing of a NAK from cable */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP1, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmSvidsRequest:
        SetPEState(port, peDfpVdmSvidsNaked);
        port->vdmm.inform_svids(port, FALSE, SOP_TYPE_SOP, __svid_info);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModesRequest:
        SetPEState(port, peDfpVdmModesNaked);
        port->vdmm.inform_modes(port, FALSE, SOP_TYPE_SOP, __modes_info);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModeEntryRequest:
        SetPEState(port, peDfpVdmModeEntryNaked);
        port->vdmm.enter_mode_result(port, FALSE, 0, 0);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModeExitRequest:
        port->vdmm.exit_mode_result(port, FALSE, 0, 0);

        /* if Mode Exit request is NAKed, go to hard reset state! */
        if (port->originalPolicyState == peSinkReady)
        {
            SetPEState(port, peSinkHardReset);
        }
        else if (port->originalPolicyState == peSourceReady)
        {
            SetPEState(port, peSourceHardReset);
        }
        else
        {
            // TODO: should never reach here, but...
        }
        SetPEState(port, port->originalPolicyState);
        return;
    case peSrcVdmIdentityRequest:
        SetPEState(port, peSrcVdmIdentityNaked);
        /* informing of a NAK from cable */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP1, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    default:
        SetPEState(port, port->originalPolicyState);
        break;
    }
}

FSC_BOOL evaluateModeEntry(Port_t *port, FSC_U32 mode_in)
{
    return (mode_in == MODE_AUTO_ENTRY && port->AutoModeEntryEnabled) ? TRUE : FALSE;
}

#endif // FSC_HAVE_VDM
