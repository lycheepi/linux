/**
 *******************************************************************************
 * @file        PDPolicy.c
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines PD Policy state machine functionality
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

#include "vendor_info.h"
#include "PD_Types.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "TypeC.h"
#include "fusb30X.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"
#include "vdm/DisplayPort/dp_types.h"
#include "vdm/DisplayPort/dp.h"
#include "vdm/DisplayPort/interface_dp.h"
#endif /* FSC_HAVE_VDM */

#ifdef FSC_DEBUG
#include "Log.h"
#endif /* FSC_DEBUG */

/* USB PD Enable / Disable Routines */
void USBPDEnable(Port_t *port, FSC_BOOL DeviceUpdate, SourceOrSink TypeCDFP)
{
    FSC_U8 data[5];

    /* PD Reset bit */
    FSC_U8 reset = 0x02;
    DeviceWrite(port->I2cAddr, regReset, 1, &reset);

    port->requests_pr_swap_as_src = Requests_PR_Swap_As_Src;
    port->requests_pr_swap_as_snk = Requests_PR_Swap_As_Snk;

    port->IsHardReset = FALSE;
    port->IsPRSwap = FALSE;
    port->HardResetCounter = 0;

    if (port->USBPDEnabled == TRUE)
    {
        /* Check CC pin to monitor */
        if (port->CCPin == CC1)
        {
            port->Registers.Switches.TXCC1 = 1;
            port->Registers.Switches.MEAS_CC1 = 1;

            port->Registers.Switches.TXCC2 = 0;
            port->Registers.Switches.MEAS_CC2 = 0;
        }
        else if (port->CCPin == CC2)
        {
            port->Registers.Switches.TXCC2 = 1;
            port->Registers.Switches.MEAS_CC2 = 1;

            port->Registers.Switches.TXCC1 = 0;
            port->Registers.Switches.MEAS_CC1 = 0;
        }

        if (port->CCPin != CCNone)
        {
            port->USBPDActive = TRUE;

            port->PolicyIsSource = (TypeCDFP == SOURCE) ? TRUE : FALSE;
            port->PolicyIsDFP = (TypeCDFP == SOURCE) ? TRUE : FALSE;
            port->IsVCONNSource = (TypeCDFP == SOURCE) ? TRUE : FALSE;

            if (port->PolicyIsSource)
            {
                SetPEState(port, peSourceStartup);
                port->Registers.Switches.POWERROLE = 1;
                port->Registers.Switches.DATAROLE = 1;
                port->Registers.Control.ENSOP1 = SOP_P_Capable;
                port->Registers.Control.ENSOP2 = SOP_PP_Capable;
            }
            else
            {
                SetPEState(port, peSinkStartup);
                TimerDisable(&port->PolicyStateTimer);
                port->Registers.Switches.POWERROLE = 0;
                port->Registers.Switches.DATAROLE = 0;

                port->Registers.Control.ENSOP1 = 0;
                port->Registers.Control.ENSOP1DP = 0;
                port->Registers.Control.ENSOP2 = 0;
                port->Registers.Control.ENSOP2DB = 0;
            }

            port->Registers.Switches.AUTO_CRC = 0;
            port->Registers.Control.AUTO_PRE = 0;

            if (port->ManualRetriesEnabled)
            {
                port->Registers.Control.N_RETRIES = 0;
            }
            else
            {
                if (port->PdRevContract == USBPDSPECREV2p0)
                {
                    port->Registers.Control.N_RETRIES = 3;
                }
                else
                {
                    port->Registers.Control.N_RETRIES = 2;
                }
            }

            port->Registers.Control.AUTO_RETRY = 1;

            data[0] = port->Registers.Slice.byte;
            data[1] = port->Registers.Control.byte[0] | 0x40; /* TX_FLUSH */
            data[2] = port->Registers.Control.byte[1] | 0x04; /* RX_FLUSH */
            data[3] = port->Registers.Control.byte[2];
            data[4] = port->Registers.Control.byte[3];
            DeviceWrite(port->I2cAddr, regControl0, 4, &data[1]);

            if (DeviceUpdate)
            {
                DeviceWrite(port->I2cAddr, regSwitches1, 1,
                            &port->Registers.Switches.byte[1]);
            }

            /* Enable the internal oscillator for USB PD */
            port->Registers.Power.POWER = 0x7;
            port->Registers.Power.PD_RCVR_DIS = 1;
            DeviceWrite(port->I2cAddr, regPower, 1,
                        &port->Registers.Power.byte);

            /* Set to 1.5A (SinkTXNG) */
            UpdateCurrentAdvert(port, utcc1p5A);

            ResetProtocolLayer(port, TRUE);

            TimerDisable(&port->SwapSourceStartTimer);

#ifdef FSC_DEBUG
            /* Store the PD attach token in the log */
            StoreUSBPDToken(port, TRUE, pdtAttach);
#endif /* FSC_DEBUG */
        }

        port->PEIdle = FALSE;

#ifdef FSC_HAVE_VDM
        if(Attempts_Discov_SOP)
        {
            port->AutoVdmState = AUTO_VDM_INIT;
        }
        else
        {
            port->AutoVdmState = AUTO_VDM_DONE;
        }
#endif /* FSC_HAVE_VDM */
    }
}

void USBPDDisable(Port_t *port, FSC_BOOL DeviceUpdate)
{
    if (port->Registers.Control.BIST_MODE2 != 0)
    {
        port->Registers.Control.BIST_MODE2 = 0;
        DeviceWrite(port->I2cAddr, regControl1, 1,
                    &port->Registers.Control.byte[1]);
    }

    port->IsHardReset = FALSE;
    port->IsPRSwap = FALSE;

    port->PEIdle = TRUE;

#ifdef FSC_DEBUG
    if (port->USBPDActive == TRUE)
    {
        /* Store the PD detach token */
        StoreUSBPDToken(port, TRUE, pdtDetach);
    }

    /* Set the source caps updated flag to trigger an update of the GUI */
    port->SourceCapsUpdated = TRUE;
#endif /* FSC_DEBUG */

    port->PdRevContract = PD_Specification_Revision;
    port->USBPDActive = FALSE;
    port->ProtocolState = PRLDisabled;
    SetPEState(port, peDisabled);
    port->PolicyIsSource = FALSE;
    port->PolicyHasContract = FALSE;
    port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

    notify_observers(BIST_DISABLED, port->I2cAddr, 0);
    notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

    if (DeviceUpdate)
    {
        /* Disable the BMC transmitter (both CC1 & CC2) */
        port->Registers.Switches.TXCC1 = 0;
        port->Registers.Switches.TXCC2 = 0;

        /* Turn off Auto CRC */
        port->Registers.Switches.AUTO_CRC = 0;
        DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);
    }

    /* Disable the internal oscillator for USB PD */
    port->Registers.Power.POWER = 0x7;
    DeviceWrite(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);
    ProtocolFlushRxFIFO(port);
    ProtocolFlushTxFIFO(port);

    /* Mask PD Interrupts */
    port->Registers.Mask.M_COLLISION = 1;
    DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    port->Registers.MaskAdv.M_RETRYFAIL = 1;
    port->Registers.MaskAdv.M_TXSENT = 1;
    port->Registers.MaskAdv.M_HARDRST = 1;
    DeviceWrite(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    port->Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);

    /* Force VBUS check */
    port->Registers.Status.I_COMP_CHNG = 1;
}

/* USB PD Policy Engine Routines */
void USBPDPolicyEngine(Port_t *port)
{
    switch (port->PolicyState)
    {
    case peDisabled:
        break;
    case peErrorRecovery:
        PolicyErrorRecovery(port);
        break;
#if (defined(FSC_HAVE_SRC) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
    /* Source States */
    case peSourceSendHardReset:
        PolicySourceSendHardReset(port);
        break;
    case peSourceSendSoftReset:
        PolicySourceSendSoftReset(port);
        break;
    case peSourceSoftReset:
        PolicySourceSoftReset(port, port->ProtocolMsgRxSop);
        break;
    case peSourceStartup:
        PolicySourceStartup(port);
        break;
    case peSourceDiscovery:
        PolicySourceDiscovery(port);
        break;
    case peSourceSendCaps:
        PolicySourceSendCaps(port);
        break;
    case peSourceDisabled:
        PolicySourceDisabled(port);
        break;
    case peSourceTransitionDefault:
        PolicySourceTransitionDefault(port);
        break;
    case peSourceNegotiateCap:
        PolicySourceNegotiateCap(port);
        break;
    case peSourceCapabilityResponse:
        PolicySourceCapabilityResponse(port);
        break;
    case peSourceWaitNewCapabilities:
        PolicySourceWaitNewCapabilities(port);
        break;
    case peSourceTransitionSupply:
        PolicySourceTransitionSupply(port);
        break;
    case peSourceReady:
        PolicySourceReady(port);
        break;
    case peSourceGiveSourceCaps:
        PolicySourceGiveSourceCap(port);
        break;
    case peSourceGetSinkCaps:
        PolicySourceGetSinkCap(port);
        break;
    case peSourceSendPing:
        PolicySourceSendPing(port);
        break;
    case peSourceGotoMin:
        PolicySourceGotoMin(port);
        break;
    case peSourceGiveSinkCaps:
        PolicySourceGiveSinkCap(port);
        break;
    case peSourceGetSourceCaps:
        PolicySourceGetSourceCap(port);
        break;
    case peSourceSendDRSwap:
        PolicySourceSendDRSwap(port);
        break;
    case peSourceEvaluateDRSwap:
        PolicySourceEvaluateDRSwap(port);
        break;
    case peSourceSendVCONNSwap:
        PolicySourceSendVCONNSwap(port);
        break;
    case peSourceEvaluateVCONNSwap:
        PolicySourceEvaluateVCONNSwap(port);
        break;
    case peSourceSendPRSwap:
        PolicySourceSendPRSwap(port);
        break;
    case peSourceEvaluatePRSwap:
        PolicySourceEvaluatePRSwap(port);
        break;
    case peSourceAlertReceived:
        PolicySourceAlertReceived(port);
        break;
#endif /* FSC_HAVE_SRC */
#ifdef FSC_HAVE_SNK
    /* Sink States */
    case peSinkStartup:
        PolicySinkStartup(port);
        break;
    case peSinkSendHardReset:
        PolicySinkSendHardReset(port);
        break;
    case peSinkSoftReset:
        PolicySinkSoftReset(port);
        break;
    case peSinkSendSoftReset:
        PolicySinkSendSoftReset(port);
        break;
    case peSinkTransitionDefault:
        PolicySinkTransitionDefault(port);
        break;
    case peSinkDiscovery:
        PolicySinkDiscovery(port);
        break;
    case peSinkWaitCaps:
        PolicySinkWaitCaps(port);
        break;
    case peSinkEvaluateCaps:
        PolicySinkEvaluateCaps(port);
        break;
    case peSinkSelectCapability:
        PolicySinkSelectCapability(port);
        break;
    case peSinkTransitionSink:
        PolicySinkTransitionSink(port);
        break;
    case peSinkReady:
        PolicySinkReady(port);
        break;
    case peSinkGiveSinkCap:
        PolicySinkGiveSinkCap(port);
        break;
    case peSinkGetSourceCap:
        PolicySinkGetSourceCap(port);
        break;
    case peSinkGetSinkCap:
        PolicySinkGetSinkCap(port);
        break;
    case peSinkGiveSourceCap:
        PolicySinkGiveSourceCap(port);
        break;
    case peSinkSendDRSwap:
        PolicySinkSendDRSwap(port);
        break;
    case peSinkEvaluateDRSwap:
        PolicySinkEvaluateDRSwap(port);
        break;
    case peSinkSendVCONNSwap:
        PolicySinkSendVCONNSwap(port);
        break;
    case peSinkEvaluateVCONNSwap:
        PolicySinkEvaluateVCONNSwap(port);
        break;
    case peSinkSendPRSwap:
        PolicySinkSendPRSwap(port);
        break;
    case peSinkEvaluatePRSwap:
        PolicySinkEvaluatePRSwap(port);
        break;
    case peSinkAlertReceived:
        PolicySinkAlertReceived(port);
        break;
#endif /* FSC_HAVE_SNK */
    case peNotSupported:
        PolicyNotSupported(port);
        break;
#ifdef FSC_HAVE_VDM
        case peGiveVdm:
        PolicyGiveVdm(port);
        break;
#endif /* FSC_HAVE_VDM */
#ifdef FSC_HAVE_EXT_MSG
    case peGiveCountryCodes:
        PolicyGiveCountryCodes(port);
        break;
    case peGetCountryCodes:
        PolicyGetCountryCodes(port);
        break;
    case peGiveCountryInfo:
        PolicyGiveCountryInfo(port);
        break;
    case peGivePPSStatus:
        PolicyGivePPSStatus(port);
        break;
    case peGetPPSStatus:
        PolicyGetPPSStatus(port);
        break;
#endif /* FSC_HAVE_EXT_MSG */
    case PE_BIST_Receive_Mode:
        policyBISTReceiveMode(port);
        break;
    case PE_BIST_Frame_Received:
        policyBISTFrameReceived(port);
        break;
    case PE_BIST_Carrier_Mode_2:
        policyBISTCarrierMode2(port);
        break;
    case PE_BIST_Test_Data:
        policyBISTTestData(port);
        break;

    default:
#ifdef FSC_HAVE_VDM
        if ((port->PolicyState >= FIRST_VDM_STATE) &&
            (port->PolicyState <= LAST_VDM_STATE))
        {
            /* Valid VDM state */
            PolicyVdm(port);
        }
        else
#endif /* FSC_HAVE_VDM */
        {
            /* Invalid state, reset */
            PolicyInvalidState(port);
        }
        break;
    }
}

void PolicyErrorRecovery(Port_t *port)
{
    SetStateErrorRecovery(port);
}

#if (defined(FSC_HAVE_SRC) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void PolicySourceSendHardReset(Port_t *port)
{
    PolicySendHardReset(port);
}

void PolicySourceSoftReset(Port_t *port, SopType sop)
{
    PolicySendCommand(port, CMTAccept, peSourceSendCaps, 0, sop);
}

void PolicySourceSendSoftReset(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTSoftReset, peSourceSendSoftReset, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
            port->PEIdle = TRUE;
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if ((port->PolicyRxHeader.NumDataObjects == 0) &&
                (port->PolicyRxHeader.MessageType == CMTAccept))
            {
                SetPEState(port, peSourceDiscovery);
            }
            else
            {
                SetPEState(port, peSourceSendHardReset);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceStartup(Port_t *port)
{
#ifdef FSC_HAVE_VDM
    FSC_S32 i;
#endif /* FSC_HAVE_VDM */

    switch (port->PolicySubIndex)
    {
    case 0:
        /* Set masks for PD */
#ifdef FSC_GSCE_FIX
        port->Registers.Mask.M_CRC_CHK = 0;     /* Added for GSCE workaround */
#endif /* FSC_GSCE_FIX */
        port->Registers.Mask.M_COLLISION = 0;
        DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
        port->Registers.MaskAdv.M_RETRYFAIL = 0;
        port->Registers.MaskAdv.M_HARDSENT = 0;
        port->Registers.MaskAdv.M_TXSENT = 0;
        port->Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(port->I2cAddr, regMaska, 1,
                    &port->Registers.MaskAdv.byte[0]);
        port->Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(port->I2cAddr, regMaskb, 1,
                    &port->Registers.MaskAdv.byte[1]);

        if (port->Registers.DeviceID.VERSION_ID == VERSION_302A)
        {
            /* Disable Rx flushing if it has been enabled */
            if (port->Registers.Control.RX_FLUSH == 1)
            {
                port->Registers.Control.RX_FLUSH = 0;
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }
        }
        else
        {
            /* Disable auto-flush RxFIFO */
            if (port->Registers.Control.BIST_TMODE == 1)
            {
                port->Registers.Control.BIST_TMODE = 0;
                DeviceWrite(port->I2cAddr, regControl3, 1,
                            &port->Registers.Control.byte[3]);
            }
        }

        port->USBPDContract.object = 0;
        port->PartnerCaps.object = 0;
        port->IsPRSwap = FALSE;
        port->IsHardReset = FALSE;
        port->PolicyIsSource = TRUE;
        port->PpsEnabled = FALSE;

        port->Registers.Switches.POWERROLE = port->PolicyIsSource;
        DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);

        ResetProtocolLayer(port, FALSE);
        port->Registers.Switches.AUTO_CRC = 1;
        DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);

        port->Registers.Power.POWER = 0xF;
        port->Registers.Power.PD_RCVR_DIS = 0; /* Enable receiver */
        DeviceWrite(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);

        port->CapsCounter = 0;
        port->CollisionCounter = 0;
        port->PolicySubIndex++;
        break;
    case 1:
        /* Wait until we reach vSafe5V and delay if coming from PR Swap */
        if ((isVBUSOverVoltage(port, VBUS_MV_VSAFE5V_L) ||
            (port->ConnState == PoweredAccessory)) &&
            (TimerExpired(&port->SwapSourceStartTimer) ||
             TimerDisabled(&port->SwapSourceStartTimer)))
        {
            /* Delay once VBus is present for potential switch delay. */
            TimerStart(&port->PolicyStateTimer, tVBusSwitchDelay);

            port->PolicySubIndex++;
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            TimerDisable(&port->PolicyStateTimer);
            TimerDisable(&port->SwapSourceStartTimer);
            SetPEState(port, peSourceSendCaps);

#ifdef FSC_HAVE_VDM
            port->mode_entered = FALSE;
            port->auto_mode_disc_tracker = 0;
            port->core_svid_info.num_svids = 0;
            for (i = 0; i < MAX_NUM_SVIDS; i++)
            {
                port->core_svid_info.svids[i] = 0;
            }
            port->AutoModeEntryObjPos = -1;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_HAVE_DP
            resetDp(port);
#endif /* FSC_HAVE_DP */
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        port->PolicySubIndex = 0;
        break;
    }
}

void PolicySourceDiscovery(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        TimerStart(&port->PolicyStateTimer, tTypeCSendSourceCap);
        port->PolicySubIndex++;
        break;
    default:
        if ((port->HardResetCounter > nHardResetCount) &&
            (port->PolicyHasContract == TRUE))
        {
            /* If we previously had a contract in place, go to ErrorRecovery */
            SetPEState(port, peErrorRecovery);
        }
        else if ((port->HardResetCounter > nHardResetCount) &&
                 (port->PolicyHasContract == FALSE))
        {
            /* Otherwise, disable and wait for detach */
            SetPEState(port, peSourceDisabled);
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            if (port->CapsCounter > nCapsCount)
                SetPEState(port, peSourceDisabled);
            else
                SetPEState(port, peSourceSendCaps);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceSendCaps(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendData(port,
                           DMTSourceCapabilities,
                           DPM_GetSourceCap(port->dpm, port),
               DPM_GetSourceCapHeader(port->dpm, port)->NumDataObjects*4,
                           peSourceSendCaps, 1,
                           SOP_TYPE_SOP, FALSE) == STAT_SUCCESS)
        {
            port->HardResetCounter = 0;
            port->CapsCounter = 0;
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if ((port->PolicyRxHeader.NumDataObjects == 1) &&
                (port->PolicyRxHeader.MessageType == DMTRequest))
            {
                SetPEState(port, peSourceNegotiateCap);

                /* Set the revision to the lowest of the two port partner */
                if ((port->PdRevContract >
                        port->PolicyRxHeader.SpecRevision) &&
                    (port->PolicyRxHeader.SpecRevision > USBPDSPECREV1p0))
                {
                    port->PdRevContract =
                            port->PolicyRxHeader.SpecRevision;

                    if (port->PdRevContract == USBPDSPECREV2p0)
                    {
                        port->Registers.Control.N_RETRIES = 3;
                    }
                    else
                    {
                        port->Registers.Control.N_RETRIES = 2;
                    }
                    DeviceWrite(port->I2cAddr, regControl3, 1,
                                &port->Registers.Control.byte[3]);
                }
            }
            else
            {
                  /* Otherwise it was a message that we didn't expect */
                  SetPEState(port, peSourceSendSoftReset);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            port->ProtocolMsgRx = FALSE;
            SetPEState(port, peSourceSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceDisabled(Port_t *port)
{
    /* Clear the USB PD contract (output power to 5V default) */
    port->USBPDContract.object = 0;

    /* Wait for a hard reset or detach... */
    if(port->loopCounter == 0)
    {
        port->PEIdle = TRUE;

        port->Registers.Power.POWER = 0x7;
        DeviceWrite(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);
    }
}

void PolicySourceTransitionDefault(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            port->Registers.Power.PD_RCVR_DIS = 1;
            DeviceWrite(port->I2cAddr, regPower, 1,
                        &port->Registers.Power.byte);

            port->PolicyHasContract = FALSE;
            port->PdRevContract = PD_Specification_Revision;
            port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

            notify_observers(BIST_DISABLED, port->I2cAddr, 0);
            notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 1:
        platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_ALL, FALSE, FALSE);
        platform_set_vbus_discharge(port->PortID, TRUE);

        if (!port->PolicyIsDFP)
        {
            port->PolicyIsDFP = TRUE;
            port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                        &port->Registers.Switches.byte[1]);

            port->Registers.Control.ENSOP1 = SOP_P_Capable;
            port->Registers.Control.ENSOP2 = SOP_PP_Capable;
            DeviceWrite(port->I2cAddr, regControl1, 1,
                        &port->Registers.Control.byte[1]);
        }

        if (port->IsVCONNSource)
        {
            /* Turn off VConn */
            port->Registers.Switches.VCONN_CC1 = 0;
            port->Registers.Switches.VCONN_CC2 = 0;
            DeviceWrite(port->I2cAddr, regSwitches0, 1,
                    &port->Registers.Switches.byte[0]);
        }

        ProtocolFlushTxFIFO(port);
        ProtocolFlushRxFIFO(port);
        ResetProtocolLayer(port, TRUE);

        TimerStart(&port->PolicyStateTimer,
                tPSHardResetMax + tSafe0V + tSrcRecover);

        port->PolicySubIndex++;
        break;
    case 2:
        if (VbusVSafe0V(port))
        {
            platform_set_vbus_discharge(port->PortID, FALSE);
            TimerStart(&port->PolicyStateTimer, tSrcRecover);
            port->PolicySubIndex++;
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            if (port->PolicyHasContract)
            {
                SetPEState(port, peErrorRecovery);
            }
            else
            {
                port->PolicySubIndex = 4;
                TimerDisable(&port->PolicyStateTimer);
            }
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_5V, TRUE, FALSE);

        /* Turn on VConn */
        if (Type_C_Sources_VCONN)
        {
            if (port->CCPin == CC1)
                port->Registers.Switches.VCONN_CC2 = 1;
            else
                port->Registers.Switches.VCONN_CC1 = 1;

            DeviceWrite(port->I2cAddr, regSwitches0, 1,
                    &port->Registers.Switches.byte[0]);
        }
        port->IsVCONNSource = TRUE;

        TimerDisable(&port->SwapSourceStartTimer);

        SetPEState(port, peSourceStartup);
        break;
    }
}

void PolicySourceNegotiateCap(Port_t *port)
{
    FSC_BOOL reqAccept = FALSE;
    FSC_U8 objPosition;
    FSC_U32 minvoltage, maxvoltage;

    objPosition = port->PolicyRxDataObj[0].FVRDO.ObjectPosition - 1;

    if (objPosition < DPM_GetSourceCapHeader(port->dpm, port)->NumDataObjects)
    {
        /* Enable PPS if pps request and capability request can be matched */
        if (DPM_GetSourceCap(port->dpm, port)[objPosition].
                PDO.SupplyType == pdoTypeAugmented)
        {
            /* Convert from 100mv (pdo) to 20mV (req) to compare */
            minvoltage = DPM_GetSourceCap(port->dpm, port)[objPosition].
                PPSAPDO.MinVoltage * 5;
            maxvoltage = DPM_GetSourceCap(port->dpm, port)[objPosition].
                PPSAPDO.MaxVoltage * 5;

            reqAccept =
                ((port->PolicyRxDataObj[0].PPSRDO.OpCurrent <=
                  DPM_GetSourceCap(port->dpm, port)[objPosition].
                      PPSAPDO.MaxCurrent) &&
                 (port->PolicyRxDataObj[0].PPSRDO.Voltage >= minvoltage) &&
                 (port->PolicyRxDataObj[0].PPSRDO.Voltage <= maxvoltage));
        }
        else if (port->PolicyRxDataObj[0].FVRDO.OpCurrent <=
            DPM_GetSourceCap(port->dpm, port)[objPosition].
                FPDOSupply.MaxCurrent)
        {
            reqAccept = TRUE;
        }
    }

    if (reqAccept)
    {
        SetPEState(port, peSourceTransitionSupply);
    }
    else
    {
        SetPEState(port, peSourceCapabilityResponse);
    }

    port->PEIdle = FALSE;
}

void PolicySourceTransitionSupply(Port_t *port)
{
    FSC_BOOL newIsPPS = FALSE;
    
    switch (port->PolicySubIndex)
    {
    case 0:
        PolicySendCommand(port, CMTAccept, peSourceTransitionSupply, 1,
                          SOP_TYPE_SOP);
        break;
    case 1:
        port->USBPDContract.object = port->PolicyRxDataObj[0].object;
        newIsPPS = (DPM_GetSourceCap(port->dpm, port)
                [port->USBPDContract.FVRDO.ObjectPosition - 1].PDO.SupplyType
                        == pdoTypeAugmented) ? TRUE : FALSE;

        if (newIsPPS && port->PpsEnabled)
        {
            /* Skip the SrcTransition period for PPS requests */
            /* Note: In a system with more than one PPS PDOs, this will need
             * to include a check to see if the request is from within the same
             * APDO (no SrcTransition delay) or a different
             * APDO (with SrcTransition delay).
             */
            port->PolicySubIndex += 2;
        }
        else
        {
            TimerStart(&port->PolicyStateTimer, tSrcTransition);
            port->PolicySubIndex++;
        }
        
        port->PpsEnabled = newIsPPS;
        
        break;
    case 2:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        port->PolicyHasContract = TRUE;
        DPM_TransitionSource(port->dpm, port,
                port->USBPDContract.FVRDO.ObjectPosition - 1);

        TimerStart(&port->PolicyStateTimer, tSourceRiseTimeout);
        port->PolicySubIndex++;

        if (port->PpsEnabled)
        {
            TimerStart(&port->PpsTimer, tPPSTimeout);
        }
        else
        {
            TimerDisable(&port->PpsTimer);
        }
        break;
    case 4:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            port->PolicySubIndex++;
        }
        else if (DPM_IsSourceCapEnabled(port->dpm, port,
                        port->USBPDContract.FVRDO.ObjectPosition - 1))
        {
            port->PolicySubIndex++;
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    default:
        if (PolicySendCommand(port, CMTPS_RDY, peSourceReady, 0,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            /* Host current must be set to 1.5A or 3.0A in explicit contract */
            if (port->Registers.Control.HOST_CUR == 0x1)
            {
                port->Registers.Control.HOST_CUR = 0x2;
                DeviceWrite(port->I2cAddr, regControl0, 1,
                        &port->Registers.Control.byte[0]);
            }

            notify_observers(PD_NEW_CONTRACT, port->I2cAddr,
                    &port->USBPDContract);
        }
        break;
    }
}

void PolicySourceCapabilityResponse(Port_t *port)
{
    if (port->PolicyHasContract)
    {
        if (port->isContractValid)
        {
            PolicySendCommand(port, CMTReject, peSourceReady, 0, SOP_TYPE_SOP); 
        }
        else
        {
            PolicySendCommand(port, CMTReject, peSourceSendHardReset, 0,
                              SOP_TYPE_SOP);
        }
    }
    else
    {
        PolicySendCommand(port, CMTReject, peSourceWaitNewCapabilities, 0,
                          SOP_TYPE_SOP);
    }
}

void PolicySourceReady(Port_t *port)
{
    if (port->ProtocolMsgRx)
    {
        /* Message received from partner */
        port->ProtocolMsgRx = FALSE;
        if (port->PolicyRxHeader.NumDataObjects == 0)
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case CMTGetSourceCap:
                SetPEState(port, peSourceSendCaps);
                break;
            case CMTGetSinkCap:
                SetPEState(port, peSourceGiveSinkCaps);
                break;
            case CMTDR_Swap:
                SetPEState(port, peSourceEvaluateDRSwap);
                break;
            case CMTPR_Swap:
                SetPEState(port, peSourceEvaluatePRSwap);
                break;
            case CMTVCONN_Swap:
                SetPEState(port, peSourceEvaluateVCONNSwap);
                break;
            case CMTSoftReset:
                SetPEState(port, peSourceSoftReset);
                break;
#if 0 /* Not implemented yet */
            case CMTGetCountryCodes:
                SetPEState(port, peGiveCountryCodes);
                break;
#endif /* 0 */
            case CMTGetPPSStatus:
                SetPEState(port, peGivePPSStatus);
                break;
            case CMTAccept:
            case CMTReject:
            case CMTNotSupported:
            case CMTWait:
                SetPEState(port, peSourceSendSoftReset);
                break;
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
        else if (port->PolicyRxHeader.Extended == 1)
        {
            switch(port->PolicyRxHeader.MessageType)
            {
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
        else
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case DMTRequest:
                SetPEState(port, peSourceNegotiateCap);
                break;
            case DMTAlert:
                SetPEState(port, peSourceAlertReceived);
                break;
#ifdef FSC_HAVE_VDM
            case DMTVenderDefined:
                convertAndProcessVdmMessage(port, port->ProtocolMsgRxSop);
                break;
#endif /* FSC_HAVE_VDM */
            case DMTBIST:
                processDMTBIST(port);
                break;
#if 0 /* Not implemented yet */
            case DMTGetCountryInfo:
                SetPEState(port, peGiveCountryInfo);
                break;
#endif /* 0 */
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
    }
    else if (port->USBPDTxFlag)
    {
        /* Has the device policy manager requested us to send a message? */
        if (port->PDTransmitHeader.NumDataObjects == 0)
        {
            switch (port->PDTransmitHeader.MessageType)
            {
            case CMTGetSinkCap:
                SetPEState(port, peSourceGetSinkCaps);
                break;
            case CMTGetSourceCap:
                SetPEState(port, peSourceGetSourceCaps);
                break;
            case CMTPing:
                SetPEState(port, peSourceSendPing);
                break;
            case CMTGotoMin:
                SetPEState(port, peSourceGotoMin);
                break;
#ifdef FSC_HAVE_DRP
            case CMTPR_Swap:
                SetPEState(port, peSourceSendPRSwap);
                break;
#endif /* FSC_HAVE_DRP */
            case CMTDR_Swap:
                SetPEState(port, peSourceSendDRSwap);
                break;
            case CMTVCONN_Swap:
                SetPEState(port, peSourceSendVCONNSwap);
                break;
            case CMTSoftReset:
                SetPEState(port, peSourceSendSoftReset);
                break;
#if 0 /* Not implemented yet */
            case CMTGetCountryCodes:
                SetPEState(port, peGetCountryCodes);
                break;
#endif /* 0 */
            default:
#ifdef FSC_DEBUG
                /* Transmit other messages directly from the GUI/DPM */
                PolicySendCommand(port, port->PDTransmitHeader.MessageType,
                        peSourceReady, 0, port->ProtocolMsgTxSop);
#endif /* FSC_DEBUG */
                break;
            }
        }
        else
        {
            switch (port->PDTransmitHeader.MessageType)
            {
            case DMTSourceCapabilities:
                SetPEState(port, peSourceSendCaps);
                break;
            case DMTVenderDefined:
#ifdef FSC_HAVE_VDM
                doVdmCommand(port);
#endif /* FSC_HAVE_VDM */
                break;
            default:
#ifdef FSC_DEBUG
                /* Transmit other messages directly from the GUI/DPM */
                PolicySendData(port, port->PDTransmitHeader.MessageType,
                               port->PDTransmitObjects,
                               port->PDTransmitHeader.NumDataObjects *
                                   sizeof(doDataObject_t),
                               peSourceReady, 0, port->PolicyMsgTxSop, FALSE);
#endif /* FSC_DEBUG */
                break;
            }
        }
        port->USBPDTxFlag = FALSE;
    }
//    else if (port->PartnerCaps.object == 0)
//    {
//        SetPEState(port, peSourceGetSinkCaps);
//    }
    else if (port->PpsEnabled && TimerExpired(&port->PpsTimer))
    {
        SetPEState(port, peSourceSendHardReset);
    }
    else if ((port->PortConfig.PortType == USBTypeC_DRP) &&
             (port->requests_pr_swap_as_src) &&
             (port->PartnerCaps.FPDOSink.DualRolePower == 1))
    {
        SetPEState(port, peSourceSendPRSwap);
    }
#ifdef FSC_HAVE_VDM
    else if (port->PolicyIsDFP && (port->AutoVdmState != AUTO_VDM_DONE))
    {
        autoVdmDiscovery(port);
    }
#endif /* FSC_HAVE_VDM */
    else
    {
        port->PEIdle = TRUE;

        if (port->SourceCurrent != utcc3p0A)
        {
            /* Set to 3.0A (SinkTXOK) */
            UpdateCurrentAdvert(port, utcc3p0A);
            updateSourceMDACHigh(port);
        }
    }

}

void PolicySourceGiveSourceCap(Port_t *port)
{
    PolicySendData(port, DMTSourceCapabilities,
                   DPM_GetSourceCap(port->dpm, port),
                   DPM_GetSourceCapHeader(port->dpm, port)->
                        NumDataObjects * sizeof(doDataObject_t),
                   peSourceReady, 0, SOP_TYPE_SOP, FALSE);
}

void PolicySourceGetSourceCap(Port_t *port)
{
    PolicySendCommand(port, CMTGetSourceCap, peSourceReady, 0, SOP_TYPE_SOP);
}

void PolicySourceGetSinkCap(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTGetSinkCap, peSourceGetSinkCaps, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if ((port->PolicyRxHeader.NumDataObjects > 0)
                    && (port->PolicyRxHeader.MessageType == DMTSinkCapabilities))
            {
                UpdateCapabilitiesRx(port, FALSE);
                SetPEState(port, peSourceReady);
            }
            else
            {
                SetPEState(port, peSourceSendHardReset);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceGiveSinkCap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    if (port->PortConfig.PortType == USBTypeC_DRP)
    {
        PolicySendData(port,
                       DMTSinkCapabilities,
                       DPM_GetSinkCap(port->dpm, port),
                       DPM_GetSinkCapHeader(port->dpm, port)->
                            NumDataObjects * sizeof(doDataObject_t),
                       peSourceReady, 0,
                       SOP_TYPE_SOP, FALSE);
    }
    else
#endif /* FSC_HAVE_DRP */
    {
        PolicySendCommand(port, CMTReject, peSourceReady,
                          0, port->ProtocolMsgRxSop);
    }
}

void PolicySourceSendPing(Port_t *port)
{
    PolicySendCommand(port, CMTPing, peSourceReady, 0, SOP_TYPE_SOP);
}

void PolicySourceGotoMin(Port_t *port)
{
    if (port->ProtocolMsgRx)
    {
        port->ProtocolMsgRx = FALSE;
        if (port->PolicyRxHeader.NumDataObjects == 0)
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case CMTSoftReset:
                SetPEState(port, peSourceSoftReset);
                break;
            default:
                /* If we receive any other command (including Reject & Wait),
                 * just go back to the ready state without changing
                 */
                break;
            }
        }
    }
    else
    {
        switch (port->PolicySubIndex)
        {
        case 0:
            PolicySendCommand(port, CMTGotoMin, peSourceGotoMin, 1,
                              SOP_TYPE_SOP);
            break;
        case 1:
            TimerStart(&port->PolicyStateTimer, tSrcTransition);
            port->PolicySubIndex++;
            break;
        case 2:
            if (TimerExpired(&port->PolicyStateTimer))
            {
                port->PolicySubIndex++;
            }
            else
            {
                port->PEIdle = TRUE;
            }
            break;
        case 3:
            /* Adjust the power supply if necessary... */
            port->PolicySubIndex++;
            break;
        case 4:
            /* Validate the output is ready prior to sending PS_RDY */
            port->PolicySubIndex++;
            break;
        default:
            PolicySendCommand(port, CMTPS_RDY, peSourceReady, 0, SOP_TYPE_SOP);
            break;
        }
    }
}

void PolicySourceSendDRSwap(Port_t *port)
{
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        Status = PolicySendCommand(port, CMTDR_Swap, peSourceSendDRSwap,
                                   1, SOP_TYPE_SOP);
        if (Status == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        else if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->PolicyIsDFP =
                            (port->PolicyIsDFP == TRUE) ? FALSE : TRUE;
                    port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
                    DeviceWrite(port->I2cAddr, regSwitches1, 1,
                                &port->Registers.Switches.byte[1]);

                    if (port->PdRevContract == USBPDSPECREV2p0)
                    {
                        /* In PD2.0, DFP controls SOP* coms */
                        if (port->PolicyIsDFP == TRUE)
                        {
                            port->Registers.Control.ENSOP1 = SOP_P_Capable;
                            port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                        }
                        else
                        {
                            port->Registers.Control.ENSOP1 = 0;
                            port->Registers.Control.ENSOP2 = 0;
                        }
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSourceReady);
                    break;
                case CMTSoftReset:
                    SetPEState(port, peSourceSoftReset);
                    break;
                default:
                    SetPEState(port, peSourceReady);
                    break;
                }
            }
            else
            {
                SetPEState(port, peSourceReady);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceEvaluateDRSwap(Port_t *port)
{
    FSC_U8 Status;

#ifdef FSC_HAVE_VDM
    /* If were are in modal operation, send a hard reset */
    if (port->mode_entered == TRUE)
    {
        SetPEState(port, peSourceSendHardReset);
        return;
    }
#endif /* FSC_HAVE_VDM */

    if (!DR_Swap_To_UFP_Supported)
    {
        PolicySendCommand(port, CMTReject, peSourceReady, 0,
                          port->ProtocolMsgRxSop);
    }
    else
    {
        Status = PolicySendCommand(port, CMTAccept, peSourceReady, 0,
                                   port->ProtocolMsgRxSop);
        if (Status == STAT_SUCCESS)
        {
            port->PolicyIsDFP = (port->PolicyIsDFP == TRUE) ? FALSE : TRUE;
            port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);

            if (port->PdRevContract == USBPDSPECREV2p0)
            {
                /* In PD2.0, DFP controls SOP* coms */
                if (port->PolicyIsDFP == TRUE)
                {
                    port->Registers.Control.ENSOP1 = SOP_P_Capable;
                    port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                }
                else
                {
                    port->Registers.Control.ENSOP1 = 0;
                    port->Registers.Control.ENSOP2 = 0;
                }
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }
        }
        else if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
    }
}

void PolicySourceSendVCONNSwap(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTVCONN_Swap, peSourceSendVCONNSwap, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    case 1:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->PolicySubIndex++;
                    break;
                case CMTWait:
                case CMTReject:
                    SetPEState(port, peSourceReady);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        if (port->IsVCONNSource)
        {
            TimerStart(&port->PolicyStateTimer, tVCONNSourceOn);
            port->PolicySubIndex++;
        }
        else
        {
            if (Type_C_Sources_VCONN)
            {
                if (port->CCPin == CC1)
                    port->Registers.Switches.VCONN_CC2 = 1;
                else
                    port->Registers.Switches.VCONN_CC1 = 1;

                DeviceWrite(port->I2cAddr, regSwitches0, 1,
                        &port->Registers.Switches.byte[0]);
            }

            port->IsVCONNSource = TRUE;

            if (port->PdRevContract == USBPDSPECREV3p0)
            {
                /* In PD3.0, VConn Source controls SOP* coms */
                port->Registers.Control.ENSOP1 = SOP_P_Capable;
                port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }

            TimerStart(&port->PolicyStateTimer, tVCONNTransition);
            port->PolicySubIndex = 4;
        }
        break;
    case 3:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    /* Turn off our VConn */
                    port->Registers.Switches.VCONN_CC1 = 0;
                    port->Registers.Switches.VCONN_CC2 = 0;
                    DeviceWrite(port->I2cAddr, regSwitches0, 1,
                                &port->Registers.Switches.byte[0]);

                    port->IsVCONNSource = FALSE;

                    if (port->PdRevContract == USBPDSPECREV3p0)
                    {
                        /* In PD3.0, VConn Source controls SOP* coms */
                        port->Registers.Control.ENSOP1 = 0;
                        port->Registers.Control.ENSOP2 = 0;
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSourceReady);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            PolicySendCommand(port, CMTPS_RDY, peSourceReady, 0, SOP_TYPE_SOP);
        }
        break;
    }
}

void PolicySourceEvaluateVCONNSwap(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Accept/Reject */
        if ((port->IsVCONNSource && VCONN_Swap_To_Off_Supported) ||
            (!port->IsVCONNSource && VCONN_Swap_To_On_Supported))
        {
            PolicySendCommand(port, CMTAccept, peSourceEvaluateVCONNSwap, 1,
                              port->ProtocolMsgRxSop);
        }
        else
        {
            PolicySendCommand(port, CMTReject, peSourceReady, 0,
                              port->ProtocolMsgRxSop);
        }
        break;
    case 1:
        /* Swap to On */
        if (port->IsVCONNSource)
        {
            TimerStart(&port->PolicyStateTimer, tVCONNSourceOn);
            port->PolicySubIndex++;
        }
        else
        {
            if (Type_C_Sources_VCONN)
            {
                if (port->CCPin == CC1)
                    port->Registers.Switches.VCONN_CC2 = 1;
                else
                    port->Registers.Switches.VCONN_CC1 = 1;

                DeviceWrite(port->I2cAddr, regSwitches0, 1,
                        &port->Registers.Switches.byte[0]);
            }

            port->IsVCONNSource = TRUE;

            if (port->PdRevContract == USBPDSPECREV3p0)
            {
                /* In PD3.0, VConn Source controls SOP* coms */
                port->Registers.Control.ENSOP1 = SOP_P_Capable;
                port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }

            TimerStart(&port->PolicyStateTimer, tVCONNTransition);
            port->PolicySubIndex = 3;
        }
        break;
    case 2:
        /* Swap to Off */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    /* Disable the VCONN source */
                    port->Registers.Switches.VCONN_CC1 = 0;
                    port->Registers.Switches.VCONN_CC2 = 0;
                    DeviceWrite(port->I2cAddr, regSwitches0, 1,
                                &port->Registers.Switches.byte[0]);

                    port->IsVCONNSource = FALSE;

                    if (port->PdRevContract == USBPDSPECREV3p0)
                    {
                        /* In PD3.0, VConn Source controls SOP* coms */
                        port->Registers.Control.ENSOP1 = 0;
                        port->Registers.Control.ENSOP2 = 0;
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSourceReady);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        /* Done - PS_RDY */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            PolicySendCommand(port, CMTPS_RDY, peSourceReady, 0,
                              port->ProtocolMsgRxSop);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySourceSendPRSwap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Send Request */
        if (PolicySendCommand(port, CMTPR_Swap, peSourceSendPRSwap, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    case 1:
        /* Wait for accept */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->WaitingOnHR = FALSE;
                    port->IsPRSwap = TRUE;
                    port->PolicyHasContract = FALSE;
                    port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

                    notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

                    TimerStart(&port->PolicyStateTimer, tSrcTransition);
                    port->PolicySubIndex++;
                    break;
                case CMTWait:
                case CMTReject:
                    SetPEState(port, peSourceReady);
                    port->IsPRSwap = FALSE;
                    port->WaitingOnHR = FALSE;
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceReady);
            port->IsPRSwap = FALSE;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        /* Turn off VBus */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_ALL, FALSE, FALSE);
            platform_set_vbus_discharge(port->PortID, TRUE);

            TimerStart(&port->PolicyStateTimer, tPSHardResetMax + tSafe0V);
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        /* Swap to Sink */
        if (VbusVSafe0V(port))
        {
            RoleSwapToAttachedSink(port);
            platform_set_vbus_discharge(port->PortID, FALSE);
            port->PolicyIsSource = FALSE;

            port->Registers.Switches.POWERROLE = port->PolicyIsSource;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);
            port->PolicySubIndex++;
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peErrorRecovery);
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 4:
        /* Signal to partner */
        Status = PolicySendCommand(port, CMTPS_RDY, peSourceSendPRSwap,
                                   5, SOP_TYPE_SOP);
        if (Status == STAT_SUCCESS)
            TimerStart(&port->PolicyStateTimer, tPSSourceOn);
        else if (Status == STAT_ERROR)
            SetPEState(port, peErrorRecovery);
        break;
    case 5:
        /* Wait for PS_Rdy */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    port->PolicySubIndex++;
                    TimerStart(&port->PolicyStateTimer, tGoodCRCDelay);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peErrorRecovery);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        /* Move on to Sink Startup */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkStartup);
            TimerDisable(&port->PolicyStateTimer);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
#endif /* FSC_HAVE_DRP */
}

void PolicySourceEvaluatePRSwap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Send either the Accept or Reject command */
        if (!Accepts_PR_Swap_As_Src ||
            port->PortConfig.PortType != USBTypeC_DRP ||
            DPM_GetSourceCap(port->dpm, port)[0].
                FPDOSupply.DualRolePower == FALSE)
        {
            PolicySendCommand(port, CMTReject, peSourceReady, 0,
                                  port->ProtocolMsgRxSop);
        }
        else if (port->PartnerCaps.object == 0)
        {
            PolicySendCommand(port, CMTWait, peSourceGetSinkCaps, 0,
                              port->ProtocolMsgRxSop);
        }
        else if(port->PartnerCaps.FPDOSink.DualRolePower == FALSE ||
            /* Determine Accept/Reject based on DRP bit in current PDO */
                ((DPM_GetSourceCap(port->dpm, port)[0].FPDOSupply.
                    ExternallyPowered == TRUE) &&
            /* Must also reject if we are ext powered and partner is not */
                 (port->PartnerCaps.FPDOSink.SupplyType == pdoTypeFixed) &&
                 (port->PartnerCaps.FPDOSink.ExternallyPowered == FALSE)))
        {
            PolicySendCommand(port, CMTReject, peSourceReady, 0,
                              port->ProtocolMsgRxSop);
        }
        else
        {
            if (PolicySendCommand(port, CMTAccept, peSourceEvaluatePRSwap, 1,
                                  port->ProtocolMsgRxSop) == STAT_SUCCESS)
            {
                port->IsPRSwap = TRUE;
                port->PolicyHasContract = FALSE;
                port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

                notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

                RoleSwapToAttachedSink(port);
                TimerStart(&port->PolicyStateTimer, tSrcTransition);
            }
        }
        break;
    case 1:
        /* Disable our VBus */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_ALL, FALSE, FALSE);
            platform_set_vbus_discharge(port->PortID, TRUE);
            TimerStart(&port->PolicyStateTimer, tPSHardResetMax + tSafe0V);
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        /* Wait on discharge */
        if (VbusVSafe0V(port))
        {
            TimerStart(&port->PolicyStateTimer, tSrcTransition);
            port->PolicyIsSource = FALSE;
            port->Registers.Switches.POWERROLE = port->PolicyIsSource;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);
            port->PolicySubIndex++;
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peErrorRecovery);
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        /* Wait on transition time */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            platform_set_vbus_discharge(port->PortID, FALSE);
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 4:
        /* Signal to partner */
        Status = PolicySendCommand(port, CMTPS_RDY, peSourceEvaluatePRSwap, 5,
                                   SOP_TYPE_SOP);
        if (Status == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tPSSourceOn);
        }
        else if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
        break;
    case 5:
        /* Wait to receive a PS_RDY message from the new DFP */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    port->PolicySubIndex++;
                    port->IsPRSwap = FALSE;
                    TimerStart(&port->PolicyStateTimer, tGoodCRCDelay);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            port->IsPRSwap = FALSE;
            SetPEState(port, peErrorRecovery);
            port->PEIdle = FALSE;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        /* Move on to sink startup */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkStartup);
            TimerDisable(&port->PolicyStateTimer);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
#else
    PolicySendCommand(port, CMTReject, peSourceReady, 0,port->ProtocolMsgRxSop);
#endif /* FSC_HAVE_DRP */
}

void PolicySourceWaitNewCapabilities(Port_t *port)
{
    if(port->loopCounter == 0)
    {
        port->PEIdle = TRUE;

        port->Registers.Mask.M_COMP_CHNG = 0;
        DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
        port->Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(port->I2cAddr, regMaska,1,&port->Registers.MaskAdv.byte[0]);
        port->Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(port->I2cAddr, regMaskb,1,&port->Registers.MaskAdv.byte[1]);
    }

    switch (port->PolicySubIndex)
    {
    case 0:
        /* Wait for Policy Manager to change source capabilities */
        break;
    default:
        /* Transition to peSourceSendCapabilities */
        SetPEState(port, peSourceSendCaps);
        break;
    }
}
#endif /* FSC_HAVE_SRC */


void PolicySourceAlertReceived(Port_t *port)
{
    if (port->PolicyRxDataObj[0].ADO.Battery ||
        port->PolicyRxDataObj[0].ADO.OTP ||
        port->PolicyRxDataObj[0].ADO.OVP ||
        port->PolicyRxDataObj[0].ADO.OpCondition ||
        port->PolicyRxDataObj[0].ADO.Input)
    {
        /* Send Get_Status */
        notify_observers(ALERT_EVENT, port->I2cAddr,
                &port->PolicyRxDataObj[0].object);
    }

    SetPEState(port, peSourceReady);
}

void PolicyNotSupported(Port_t *port)
{
    if (port->PdRevContract == USBPDSPECREV2p0)
    {
        PolicySendCommand(port, CMTReject,
            port->PolicyIsSource ? peSourceReady : peSinkReady, 0,
            port->ProtocolMsgRxSop);
    }
    else
    {
        PolicySendCommand(port, CMTNotSupported,
            port->PolicyIsSource ? peSourceReady : peSinkReady, 0,
            port->ProtocolMsgRxSop);
    }
}

#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(Port_t *port)
{
    PolicySendHardReset(port);
}

void PolicySinkSoftReset(Port_t *port)
{
    if (PolicySendCommand(port, CMTAccept, peSinkWaitCaps, 0,
                          SOP_TYPE_SOP) == STAT_SUCCESS)
    {
        TimerStart(&port->PolicyStateTimer, tSinkWaitCap);
    }
}

void PolicySinkSendSoftReset(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTSoftReset, peSinkSendSoftReset, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if ((port->PolicyRxHeader.NumDataObjects == 0) &&
                (port->PolicyRxHeader.MessageType == CMTAccept))
            {
                SetPEState(port, peSinkWaitCaps);
                TimerStart(&port->PolicyStateTimer, tSinkWaitCap);
            }
            else 
            {
                SetPEState(port, peSinkSendHardReset);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySinkTransitionDefault(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        port->Registers.Power.POWER = 0x7;
        port->Registers.Power.PD_RCVR_DIS = 1;
        DeviceWrite(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);

        /* Set up VBus measure interrupt to watch for vSafe0V */
        port->Registers.Measure.MEAS_VBUS = 1;
        port->Registers.Measure.MDAC = VBUS_MDAC_0P84V;
        DeviceWrite(port->I2cAddr, regMeasure, 1,
                    &port->Registers.Measure.byte);

        port->IsHardReset = TRUE;
        port->PolicyHasContract = FALSE;
        port->PdRevContract = PD_Specification_Revision;
        port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

        notify_observers(BIST_DISABLED, port->I2cAddr, 0);

        notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

        TimerStart(&port->PolicyStateTimer, tPSHardResetMax + tSafe0V);

        if (port->PolicyIsDFP)
        {
            port->PolicyIsDFP = FALSE;
            port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                        &port->Registers.Switches.byte[1]);

            port->Registers.Control.ENSOP1 = 0;
            port->Registers.Control.ENSOP2 = 0;
            DeviceWrite(port->I2cAddr, regControl1, 1,
                        &port->Registers.Control.byte[1]);
        }

        if (port->IsVCONNSource)
        {
            port->Registers.Switches.VCONN_CC1 = 0;
            port->Registers.Switches.VCONN_CC2 = 0;
            DeviceWrite(port->I2cAddr, regSwitches0, 1,
                        &port->Registers.Switches.byte[0]);
            port->IsVCONNSource = FALSE;
        }

        ProtocolFlushTxFIFO(port);
        ProtocolFlushRxFIFO(port);
        ResetProtocolLayer(port, TRUE);
        port->PolicySubIndex++;
        break;
    case 1:
        if (port->Registers.Status.I_COMP_CHNG)
        {
            port->PolicySubIndex++;

            /* Set up VBus measure interrupt to watch for vSafe5V */
            port->Registers.Measure.MEAS_VBUS = 1;
            port->Registers.Measure.MDAC = VBUS_MDAC_4P62;
            DeviceWrite(port->I2cAddr, regMeasure, 1,
                        &port->Registers.Measure.byte);

            TimerStart(&port->PolicyStateTimer, tSrcRecoverMax + tSrcTurnOn);
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            if (port->PolicyHasContract)
            {
                SetPEState(port, peErrorRecovery);
            }
            else
            {
                SetPEState(port, peSinkStartup);
                TimerDisable(&port->PolicyStateTimer);
            }
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        if (port->Registers.Status.I_COMP_CHNG)
        {
            port->PolicySubIndex++;
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            if (port->PolicyHasContract)
            {
                SetPEState(port, peErrorRecovery);
            }
            else
            {
                SetPEState(port, peSinkStartup);
                TimerDisable(&port->PolicyStateTimer);
            }
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        SetPEState(port, peSinkStartup);
        TimerDisable(&port->PolicyStateTimer);
        break;
    }
}

void PolicySinkStartup(Port_t *port)
{
#ifdef FSC_HAVE_VDM
    FSC_S32 i;
#endif /* FSC_HAVE_VDM */
    port->IsHardReset = FALSE;

    /* Force check for VBUS */
    port->Registers.Status.I_COMP_CHNG = 1;

#ifdef FSC_GSCE_FIX
    port->Registers.Mask.M_CRC_CHK = 0;     /* Added for GSCE workaround */
#endif /* FSC_GSCE_FIX */
    port->Registers.Mask.M_COLLISION = 0;
    DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    port->Registers.MaskAdv.M_RETRYFAIL = 0;
    port->Registers.MaskAdv.M_HARDSENT = 0;
    port->Registers.MaskAdv.M_TXSENT = 0;
    port->Registers.MaskAdv.M_HARDRST = 0;
    DeviceWrite(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    port->Registers.MaskAdv.M_GCRCSENT = 0;
    DeviceWrite(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);

    if (port->Registers.DeviceID.VERSION_ID == VERSION_302A)
    {
        if (port->Registers.Control.RX_FLUSH == 1)
        {
            port->Registers.Control.RX_FLUSH = 0;
            DeviceWrite(port->I2cAddr, regControl1, 1,
                        &port->Registers.Control.byte[1]);
        }
    }
    else
    {
        if (port->Registers.Control.BIST_TMODE == 1)
        {
            port->Registers.Control.BIST_TMODE = 0;
            DeviceWrite(port->I2cAddr, regControl3, 1,
                        &port->Registers.Control.byte[3]);
        }
    }

    port->USBPDContract.object = 0;
    port->PartnerCaps.object = 0;
    port->IsPRSwap = FALSE;
    port->PolicyIsSource = FALSE;
    port->PpsEnabled = FALSE;

    port->Registers.Switches.POWERROLE = port->PolicyIsSource;
    DeviceWrite(port->I2cAddr, regSwitches1, 1,
            &port->Registers.Switches.byte[1]);

    ResetProtocolLayer(port, FALSE);

    port->Registers.Switches.AUTO_CRC = 1;
    DeviceWrite(port->I2cAddr, regSwitches1, 1,
            &port->Registers.Switches.byte[1]);

    port->Registers.Power.POWER = 0xF;
    port->Registers.Power.PD_RCVR_DIS = 0; /* Enable receiver */
    DeviceWrite(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);

    port->CapsCounter = 0;
    port->CollisionCounter = 0;
    TimerDisable(&port->PolicyStateTimer);
    SetPEState(port, peSinkDiscovery);

#ifdef FSC_HAVE_VDM
    port->auto_mode_disc_tracker = 0;

    port->mode_entered = FALSE;

    port->core_svid_info.num_svids = 0;
    for (i = 0; i < MAX_NUM_SVIDS; i++)
    {
        port->core_svid_info.svids[i] = 0;
    }
    port->AutoModeEntryObjPos = -1;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_HAVE_DP
    resetDp(port);
#endif /* FSC_HAVE_DP */
}

void PolicySinkDiscovery(Port_t *port)
{
    SetPEState(port, peSinkWaitCaps);
    TimerStart(&port->PolicyStateTimer, tTypeCSinkWaitCap);
}

void PolicySinkWaitCaps(Port_t *port)
{
    if (port->ProtocolMsgRx)
    {
        port->ProtocolMsgRx = FALSE;
        if ((port->PolicyRxHeader.NumDataObjects > 0) &&
            (port->PolicyRxHeader.MessageType == DMTSourceCapabilities))
        {
            UpdateCapabilitiesRx(port, TRUE);

            /* Align our PD spec rev with the port partner */
            if (port->PdRevContract > port->PolicyRxHeader.SpecRevision &&
                port->PolicyRxHeader.SpecRevision > USBPDSPECREV1p0)
            {
                port->PdRevContract = port->PolicyRxHeader.SpecRevision;
            }

            if (port->PdRevContract == USBPDSPECREV2p0)
            {
                port->Registers.Control.N_RETRIES = 3;
            }
            else
            {
                port->Registers.Control.N_RETRIES = 2;
            }
            DeviceWrite(port->I2cAddr, regControl3, 1,
                        &port->Registers.Control.byte[3]);

            SetPEState(port, peSinkEvaluateCaps);
        }
        else if ((port->PolicyRxHeader.NumDataObjects == 0) &&
                 (port->PolicyRxHeader.MessageType == CMTSoftReset))
        {
            SetPEState(port, peSinkSoftReset);
        }
    }
    else if ((port->PolicyHasContract == TRUE) &&
             (port->HardResetCounter > nHardResetCount))
    {
        SetPEState(port, peErrorRecovery);
    }
    else if ((TimerExpired(&port->PolicyStateTimer)) &&
             (port->HardResetCounter <= nHardResetCount))
    {
        SetPEState(port, peSinkSendHardReset);
    }
    else
    {
        port->PEIdle = TRUE;
    }
}

void PolicySinkEvaluateCaps(Port_t *port)
{
    FSC_S32 i, reqPos = 0;
    FSC_U32 objVoltage = 0;
    FSC_U32 objCurrent = 0, objPower, MaxPower = 0, SelVoltage = 0, ReqCurrent;

    port->HardResetCounter = 0;
    port->PortConfig.SinkRequestMaxVoltage = 0;

    /* Calculate max voltage - algorithm in case of non-compliant ordering */
    for (i = 0; i < DPM_GetSinkCapHeader(port->dpm, port)->NumDataObjects; i++)
    {
        port->PortConfig.SinkRequestMaxVoltage  =
            ((DPM_GetSinkCap(port->dpm, port)[i].FPDOSink.Voltage >
              port->PortConfig.SinkRequestMaxVoltage) ?
                DPM_GetSinkCap(port->dpm, port)[i].FPDOSink.Voltage :
                port->PortConfig.SinkRequestMaxVoltage);
    }

    /* Going to select the highest power object that we are compatible with */
    for (i = 0; i < port->CapsHeaderReceived.NumDataObjects; i++)
    {
        switch (port->CapsReceived[i].PDO.SupplyType)
        {
        case pdoTypeFixed:
            objVoltage = port->CapsReceived[i].FPDOSupply.Voltage;
            if (objVoltage > port->PortConfig.SinkRequestMaxVoltage )
            {
                /* If the voltage is greater than our limit... */
                continue;
            }
            else
            {
                /* Calculate the power for comparison */
                objCurrent = port->CapsReceived[i].FPDOSupply.MaxCurrent;
                objPower = objVoltage * objCurrent;
            }
            break;
        case pdoTypeVariable:
        case pdoTypeBattery:
        case pdoTypeAugmented:
        default:
            /* Ignore other supply types for now */
            objPower = 0;
            break;
        }

        /* Look for highest power */
        if (objPower >= MaxPower)
        {
            MaxPower = objPower;
            SelVoltage = objVoltage;
            reqPos = i + 1;
        }
    }

    if ((reqPos > 0) && (SelVoltage > 0))
    {
        /* Check if PPS was selected */
        port->PpsEnabled = (port->CapsReceived[reqPos - 1].PDO.SupplyType
                == pdoTypeAugmented) ? TRUE : FALSE;

        port->PartnerCaps.object = port->CapsReceived[0].object;

        /* Initialize common fields */
        port->SinkRequest.FVRDO.ObjectPosition = reqPos & 0x07;
        port->SinkRequest.FVRDO.GiveBack =
            port->PortConfig.SinkGotoMinCompatible;
        port->SinkRequest.FVRDO.NoUSBSuspend =
            port->PortConfig.SinkUSBSuspendOperation;
        port->SinkRequest.FVRDO.USBCommCapable =
            port->PortConfig.SinkUSBCommCapable;
        ReqCurrent = port->PortConfig.SinkRequestOpPower / SelVoltage;

        if (port->PpsEnabled)
        {
            port->SinkRequest.PPSRDO.OpCurrent = ReqCurrent & 0x7F;
            port->SinkRequest.PPSRDO.Voltage = SelVoltage & 0x7FF;
            port->SinkRequest.PPSRDO.UnChnkExtMsgSupport = 0;
        }
        else
        {
            /* Set the current based on the selected voltage (in 10mA units) */
            port->SinkRequest.FVRDO.OpCurrent = (ReqCurrent & 0x3FF);
            ReqCurrent = port->PortConfig.SinkRequestMaxPower / SelVoltage;
            port->SinkRequest.FVRDO.MinMaxCurrent = (ReqCurrent & 0x03FF);
        }

        /* If the give back flag is set there can't be a mismatch */
        if (port->PortConfig.SinkGotoMinCompatible)
        {
            port->SinkRequest.FVRDO.CapabilityMismatch = FALSE;
        }
        else
        {
            /* If the max power available is less than the max requested... */
            if (objCurrent < ReqCurrent)
            {
                port->SinkRequest.FVRDO.CapabilityMismatch = TRUE;
                port->SinkRequest.FVRDO.MinMaxCurrent =
                    DPM_GetSinkCap(port->dpm, port)[0].
                        FPDOSink.OperationalCurrent;
                port->SinkRequest.FVRDO.OpCurrent = objCurrent;
            }
            else
            {
                port->SinkRequest.FVRDO.CapabilityMismatch = FALSE;
            }
        }

        SetPEState(port, peSinkSelectCapability);
    }
    else
    {
        /* For now, we are just going to go back to the wait state
         * instead of sending a reject or reset (may change in future)
         */
        SetPEState(port, peSinkWaitCaps);
        TimerStart(&port->PolicyStateTimer, tTypeCSinkWaitCap);
    }
}

void PolicySinkSelectCapability(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendData(port, DMTRequest, &port->SinkRequest,
                           sizeof(doDataObject_t), peSinkSelectCapability, 1,
                           SOP_TYPE_SOP, FALSE) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    case 1:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    /* Check if PPS was selected (Here as well, for GUI req) */
                    port->PpsEnabled = (port->CapsReceived[
                        port->SinkRequest.FVRDO.ObjectPosition - 1].PDO.
                            SupplyType == pdoTypeAugmented) ? TRUE : FALSE;
                    
                    port->PolicyHasContract = TRUE;
                    port->USBPDContract.object = port->SinkRequest.object;
                    TimerStart(&port->PolicyStateTimer, tPSTransition);
                    SetPEState(port, peSinkTransitionSink);

                    if (port->PpsEnabled == TRUE)
                    {
                        TimerStart(&port->PpsTimer, tPPSRequest);
                    }
                    break;
                case CMTWait:
                case CMTReject:
                    if (port->PolicyHasContract)
                    {
                        SetPEState(port, peSinkReady);
                    }
                    else
                    {
                        SetPEState(port, peSinkWaitCaps);

                        /* Make sure we don't send reset to prevent loop */
                        port->HardResetCounter = nHardResetCount + 1;
                    }
                    break;
                case CMTSoftReset:
                    SetPEState(port, peSinkSoftReset);
                    break;
                default:
                    SetPEState(port, peSinkSendSoftReset);
                    break;
                }
            }
            else
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case DMTSourceCapabilities:
                    UpdateCapabilitiesRx(port, TRUE);
                    SetPEState(port, peSinkEvaluateCaps);
                    break;
                default:
                    SetPEState(port, peSinkSendSoftReset);
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySinkTransitionSink(Port_t *port)
{
    port->PEIdle = FALSE;

    if (port->ProtocolMsgRx)
    {
        port->ProtocolMsgRx = FALSE;
        if (port->PolicyRxHeader.NumDataObjects == 0)
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case CMTPS_RDY:
            {
                SetPEState(port, peSinkReady);
                notify_observers(PD_NEW_CONTRACT, port->I2cAddr,
                        &port->USBPDContract);

                if (port->PpsEnabled)
                {
                    /* 80% --> (V * 20) * (80 / 100) */
                    port->DetachThreshold =
                        port->USBPDContract.PPSRDO.Voltage * 16;
                }
                else
                {
                    if (port->PartnerCaps.FPDOSupply.Voltage == 100)
                    {
                        port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;
                    }
                    else
                    {
                        /* 80% --> (V * 50) * (80 / 100) */
                        port->DetachThreshold =
                            port->PartnerCaps.FPDOSupply.Voltage * 40;
                    }
                }
                break;
            }
            case CMTSoftReset:
                SetPEState(port, peSinkSoftReset);
                break;
            default:
                SetPEState(port, peSinkSendHardReset);
                break;
            }
        }
        else
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case DMTSourceCapabilities:
                UpdateCapabilitiesRx(port, TRUE);
                SetPEState(port, peSinkEvaluateCaps);
                break;
            default:
                SetPEState(port, peSinkSendHardReset);
                break;
            }
        }
    }
    else if (TimerExpired(&port->PolicyStateTimer))
    {
        SetPEState(port, peSinkSendHardReset);
    }
    else
    {
        port->PEIdle = TRUE;
    }
}

void PolicySinkReady(Port_t *port)
{
    if (port->ProtocolMsgRx)
    {
        /* Handle a received message */
        port->ProtocolMsgRx = FALSE;
        if (port->PolicyRxHeader.NumDataObjects == 0)
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case CMTGotoMin:
                SetPEState(port, peSinkTransitionSink);
                TimerStart(&port->PolicyStateTimer, tPSTransition);
                break;
            case CMTGetSinkCap:
                SetPEState(port, peSinkGiveSinkCap);
                break;
            case CMTGetSourceCap:
                SetPEState(port, peSinkGiveSourceCap);
                break;
            case CMTDR_Swap:
                SetPEState(port, peSinkEvaluateDRSwap);
                break;
            case CMTPR_Swap:
                SetPEState(port, peSinkEvaluatePRSwap);
                break;
            case CMTVCONN_Swap:
                SetPEState(port, peSinkEvaluateVCONNSwap);
                break;
            case CMTSoftReset:
                SetPEState(port, peSinkSoftReset);
                break;
#if 0 /* Not implemented yet */
            case CMTGetCountryCodes:
                SetPEState(port, peGiveCountryCodes);
                break;
#endif /* 0 */
            /* Unexpected messages */
            case CMTAccept:
            case CMTReject:
            case CMTNotSupported:
            case CMTWait:
                SetPEState(port, peSinkSendSoftReset);
                break;
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
        else if (port->PolicyRxHeader.Extended == 1)
        {
            switch(port->PolicyRxHeader.MessageType)
            {
#if 0 /* Not implemented yet */
            case EXTStatus:
                /* todo inform policy manager */
                /* Send Get PPS Status in response to any event flags */
                /* Make sure data message is correct */
                if ((port->PolicyRxDataObj[0].ExtHeader.DataSize ==
                        EXT_STATUS_MSG_BYTES) &&
                    (port->PolicyRxDataObj[1].SDB.OCP ||
                     port->PolicyRxDataObj[1].SDB.OTP ||
                     (port->PpsEnabled && port->PolicyRxDataObj[1].SDB.CVorCF)))
                {
                    PolicySendCommand(port, CMTGetPPSStatus, peSinkReady,
                                      0, port->ProtocolMsgTxSop);
                }
                break;
#endif /* 0 */
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
        else
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case DMTSourceCapabilities:
                UpdateCapabilitiesRx(port, TRUE);
                SetPEState(port, peSinkEvaluateCaps);
                break;
            case DMTAlert:
                SetPEState(port, peSinkAlertReceived);
                break;
#if 0 /* Not implemented yet */
            case DMTGetCountryInfo:
                SetPEState(port, peGiveCountryInfo);
                break;
#endif /* 0 */
#ifdef FSC_HAVE_VDM
            case DMTVenderDefined:
                convertAndProcessVdmMessage(port, port->ProtocolMsgRxSop);
                break;
#endif /* FSC_HAVE_VDM */
            case DMTBIST:
                processDMTBIST(port);
                break;
            default:
                SetPEState(port, peNotSupported);
                break;
            }
        }
    }
    else if (port->USBPDTxFlag)
    {
        /* Has the device policy manager requested us to send a message? */
        if (port->PDTransmitHeader.NumDataObjects == 0)
        {
            switch (port->PDTransmitHeader.MessageType)
            {
            case CMTGetSourceCap:
                SetPEState(port, peSinkGetSourceCap);
                break;
            case CMTGetSinkCap:
                SetPEState(port, peSinkGetSinkCap);
                break;
            case CMTDR_Swap:
                SetPEState(port, peSinkSendDRSwap);
                break;
            case CMTVCONN_Swap:
                SetPEState(port, peSinkSendVCONNSwap);
                break;
#ifdef FSC_HAVE_DRP
            case CMTPR_Swap:
                SetPEState(port, peSinkSendPRSwap);
                break;
#endif /* FSC_HAVE_DRP */
            case CMTSoftReset:
                SetPEState(port, peSinkSendSoftReset);
                break;
#if 0 /* Not implemented yet */
            case CMTGetCountryCodes:
                SetPEState(port, peGetCountryCodes);
                break;
#endif /* 0 */
            case CMTGetPPSStatus:
                SetPEState(port, peGetPPSStatus);
                break;
            default:
#ifdef FSC_DEBUG
                /* Transmit other messages directly from the GUI/DPM */
                PolicySendCommand(port, port->PDTransmitHeader.MessageType,
                        peSinkReady, 0, port->ProtocolMsgTxSop);
#endif /* FSC_DEBUG */
                break;
            }
        }
        else
        {
            switch (port->PDTransmitHeader.MessageType)
            {
            case DMTRequest:
                port->SinkRequest.object = port->PDTransmitObjects[0].object;
                SetPEState(port, peSinkSelectCapability);
                break;
            case DMTVenderDefined:
#ifdef FSC_HAVE_VDM
                doVdmCommand(port);
#endif /* FSC_HAVE_VDM */
                break;
            default:
#ifdef FSC_DEBUG
                /* Transmit other messages directly from the GUI/DPM */
                PolicySendData(port, port->PDTransmitHeader.MessageType,
                               port->PDTransmitObjects,
                               port->PDTransmitHeader.NumDataObjects *
                                sizeof(doDataObject_t),
                               peSinkReady, 0, port->ProtocolMsgTxSop, FALSE);
#endif /* FSC_DEBUG */
                break;
            }
        }
        port->USBPDTxFlag = FALSE;
    }
    else if ((port->PortConfig.PortType == USBTypeC_DRP) &&
             (port->requests_pr_swap_as_snk) &&
             (port->PartnerCaps.FPDOSupply.DualRolePower == TRUE))
    {
        SetPEState(port, peSinkSendPRSwap);
    }
#ifdef FSC_HAVE_VDM
    else if (port->PolicyIsDFP && (port->AutoVdmState != AUTO_VDM_DONE))
    {
        autoVdmDiscovery(port);
    }
#endif /* FSC_HAVE_VDM */
    else if (port->PpsEnabled == TRUE && TimerExpired(&port->PpsTimer))
    {
        /* PPS Timer send sink request to keep PPS */
        /* Request stored in the SinkRequest object */
        SetPEState(port, peSinkSelectCapability);
    }
    else
    {
        /* Wait for VBUSOK or HARDRST or GCRCSENT */
        port->PEIdle = TRUE;
    }
}

void PolicySinkGiveSinkCap(Port_t *port)
{
    PolicySendData(port, DMTSinkCapabilities,
                   DPM_GetSinkCap(port->dpm, port),
                   DPM_GetSinkCapHeader(port->dpm, port)->NumDataObjects *
                    sizeof(doDataObject_t),
                   peSinkReady, 0, SOP_TYPE_SOP, FALSE);
}

void PolicySinkGetSinkCap(Port_t *port)
{
    PolicySendCommand(port, CMTGetSinkCap, peSinkReady, 0, SOP_TYPE_SOP);
}

void PolicySinkGiveSourceCap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    if (port->PortConfig.PortType == USBTypeC_DRP)
    {
        PolicySendData(port, DMTSourceCapabilities,
                       DPM_GetSourceCap(port->dpm, port),
                       DPM_GetSourceCapHeader(port->dpm,port)->NumDataObjects *
                        sizeof(doDataObject_t),
                       peSinkReady, 0,
                       SOP_TYPE_SOP, FALSE);
    }
    else
#endif /* FSC_HAVE_DRP */
    {
        PolicySendCommand(port, CMTReject, peSinkReady, 0, SOP_TYPE_SOP);
    }
}

void PolicySinkGetSourceCap(Port_t *port)
{
    PolicySendCommand(port, CMTGetSourceCap, peSinkReady, 0, SOP_TYPE_SOP);
}

void PolicySinkSendDRSwap(Port_t *port)
{
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        Status = PolicySendCommand(port, CMTDR_Swap, peSinkSendDRSwap, 1,
                                   SOP_TYPE_SOP);
        if (Status == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        else if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->PolicyIsDFP =
                            (port->PolicyIsDFP == TRUE) ? FALSE : TRUE;

                    port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
                    DeviceWrite(port->I2cAddr, regSwitches1, 1,
                                &port->Registers.Switches.byte[1]);

                    if (port->PdRevContract == USBPDSPECREV2p0)
                    {
                        /* In PD2.0, DFP controls SOP* coms */
                        if (port->PolicyIsDFP == TRUE)
                        {
                            port->Registers.Control.ENSOP1 = SOP_P_Capable;
                            port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                        }
                        else
                        {
                            port->Registers.Control.ENSOP1 = 0;
                            port->Registers.Control.ENSOP2 = 0;
                        }
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSinkReady);
                    break;
                case CMTSoftReset:
                    SetPEState(port, peSinkSoftReset);
                    break;
                default:
                    SetPEState(port, peSinkReady);
                    break;
                }
            }
            else
            {
                SetPEState(port, peSinkReady);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySinkEvaluateDRSwap(Port_t *port)
{
    FSC_U8 Status;

#ifdef FSC_HAVE_VDM
    if (port->mode_entered == TRUE)
    {
        SetPEState(port, peSinkSendHardReset);
        return;
    }
#endif /* FSC_HAVE_VDM */

    if (!DR_Swap_To_DFP_Supported)
    {
        PolicySendCommand(port, CMTReject, peSinkReady, 0, SOP_TYPE_SOP);
    }
    else
    {
        Status = PolicySendCommand(port, CMTAccept, peSinkReady, 0,
                                   SOP_TYPE_SOP);
        if (Status == STAT_SUCCESS)
        {
            port->PolicyIsDFP = (port->PolicyIsDFP == TRUE) ? FALSE : TRUE;
            port->Registers.Switches.DATAROLE = port->PolicyIsDFP;
            DeviceWrite(port->I2cAddr, regSwitches1, 1,
                    &port->Registers.Switches.byte[1]);

            if (port->PdRevContract == USBPDSPECREV2p0)
            {
                /* In PD2.0, DFP controls SOP* coms */
                if (port->PolicyIsDFP == TRUE)
                {
                    port->Registers.Control.ENSOP1 = SOP_P_Capable;
                    port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                }
                else
                {
                    port->Registers.Control.ENSOP1 = 0;
                    port->Registers.Control.ENSOP2 = 0;
                }
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }
        }
        else if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
    }
}

void PolicySinkSendVCONNSwap(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTVCONN_Swap, peSinkSendVCONNSwap, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    case 1:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->PolicySubIndex++;
                    break;
                case CMTWait:
                case CMTReject:
                    SetPEState(port, peSinkReady);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        if (port->IsVCONNSource)
        {
            TimerStart(&port->PolicyStateTimer, tVCONNSourceOn);
            port->PolicySubIndex++;
        }
        else
        {
            if (Type_C_Sources_VCONN)
            {
                if (port->CCPin == CC1)
                {
                    port->Registers.Switches.VCONN_CC2 = 1;
                    port->Registers.Switches.PDWN2 = 0;
                }
                else
                {
                    port->Registers.Switches.VCONN_CC1 = 1;
                    port->Registers.Switches.PDWN1 = 0;
                }

                DeviceWrite(port->I2cAddr, regSwitches0, 1,
                        &port->Registers.Switches.byte[0]);
            }

            port->IsVCONNSource = TRUE;

            if (port->PdRevContract == USBPDSPECREV3p0)
            {
                /* In PD3.0, VConn Source controls SOP* coms */
                port->Registers.Control.ENSOP1 = SOP_P_Capable;
                port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }

            TimerStart(&port->PolicyStateTimer, tVCONNTransition);
            port->PolicySubIndex = 4;
        }
        break;
    case 3:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    /* Turn off our VConn */
                    port->Registers.Switches.VCONN_CC1 = 0;
                    port->Registers.Switches.VCONN_CC2 = 0;
                    port->Registers.Switches.PDWN1 = 1;
                    port->Registers.Switches.PDWN2 = 1;
                    DeviceWrite(port->I2cAddr, regSwitches0, 1,
                                &port->Registers.Switches.byte[0]);

                    port->IsVCONNSource = FALSE;

                    if (port->PdRevContract == USBPDSPECREV3p0)
                    {
                        /* In PD3.0, VConn Source controls SOP* coms */
                        port->Registers.Control.ENSOP1 = 0;
                        port->Registers.Control.ENSOP2 = 0;
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSinkReady);
                    break;
                default:
                    /* For all other commands received, simply ignore them */
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            PolicySendCommand(port, CMTPS_RDY, peSinkReady, 0, SOP_TYPE_SOP);
        }
        break;
    }
}

void PolicySinkEvaluateVCONNSwap(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if ((port->IsVCONNSource && VCONN_Swap_To_Off_Supported) ||
            (!port->IsVCONNSource && VCONN_Swap_To_On_Supported))
        {
            PolicySendCommand(port, CMTAccept, peSinkEvaluateVCONNSwap, 1,
                    SOP_TYPE_SOP);
        }
        else
        {
            PolicySendCommand(port, CMTReject, peSinkReady, 0,
                    SOP_TYPE_SOP);
        }
        break;
    case 1:
        if (port->IsVCONNSource)
        {
            TimerStart(&port->PolicyStateTimer, tVCONNSourceOn);
            port->PolicySubIndex++;
        }
        else
        {
            if (Type_C_Sources_VCONN)
            {
                if (port->CCPin == CC1)
                {
                    port->Registers.Switches.VCONN_CC2 = 1;
                    port->Registers.Switches.PDWN2 = 0;
                }
                else
                {
                    port->Registers.Switches.VCONN_CC1 = 1;
                    port->Registers.Switches.PDWN1 = 0;
                }

                DeviceWrite(port->I2cAddr, regSwitches0, 1,
                        &port->Registers.Switches.byte[0]);
            }

            port->IsVCONNSource = TRUE;

            if (port->PdRevContract == USBPDSPECREV3p0)
            {
                /* In PD3.0, VConn Source controls SOP* coms */
                port->Registers.Control.ENSOP1 = SOP_P_Capable;
                port->Registers.Control.ENSOP2 = SOP_PP_Capable;
                DeviceWrite(port->I2cAddr, regControl1, 1,
                            &port->Registers.Control.byte[1]);
            }

            TimerStart(&port->PolicyStateTimer, tVCONNTransition);

            /* Move on to sending the PS_RDY message after the timer expires */
            port->PolicySubIndex = 3;
        }
        break;
    case 2:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    port->Registers.Switches.VCONN_CC1 = 0;
                    port->Registers.Switches.VCONN_CC2 = 0;
                    port->Registers.Switches.PDWN1 = 1;
                    port->Registers.Switches.PDWN2 = 1;
                    DeviceWrite(port->I2cAddr, regSwitches0, 1,
                                &port->Registers.Switches.byte[0]);

                    port->IsVCONNSource = FALSE;

                    if (port->PdRevContract == USBPDSPECREV3p0)
                    {
                        /* In PD3.0, VConn Source controls SOP* coms */
                        port->Registers.Control.ENSOP1 = 0;
                        port->Registers.Control.ENSOP2 = 0;
                        DeviceWrite(port->I2cAddr, regControl1, 1,
                                    &port->Registers.Control.byte[1]);
                    }

                    SetPEState(port, peSinkReady);
                    break;
                default:
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSourceSendHardReset);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    default:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            PolicySendCommand(port, CMTPS_RDY, peSinkReady, 0, SOP_TYPE_SOP);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicySinkSendPRSwap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Send swap message */
        if (PolicySendCommand(port, CMTPR_Swap, peSinkSendPRSwap, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    case 1:
        /* Wait for Accept */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTAccept:
                    port->WaitingOnHR = FALSE;
                    port->IsPRSwap = TRUE;
                    port->PolicyHasContract = FALSE;
                    port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

                    notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

                    TimerStart(&port->PolicyStateTimer, tPSSourceOff);
                    port->PolicySubIndex++;
                    break;
                case CMTWait:
                case CMTReject:
                    port->WaitingOnHR = FALSE;
                    SetPEState(port, peSinkReady);
                    port->IsPRSwap = FALSE;
                    break;
                default:
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peSinkReady);
            port->IsPRSwap = FALSE;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        /* Wait for PS_RDY and perform the swap */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    RoleSwapToAttachedSource(port);
                    port->PolicyIsSource = TRUE;
                    port->Registers.Switches.POWERROLE = port->PolicyIsSource;
                    DeviceWrite(port->I2cAddr, regSwitches1, 1,
                                &port->Registers.Switches.byte[1]);
                    TimerDisable(&port->PolicyStateTimer);
                    port->PolicySubIndex++;
                    break;
                default:
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peErrorRecovery);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        /* Wait on VBus */
        if (isVSafe5V(port))
        {
            /* Delay once VBus is present for potential switch delay. */
            TimerStart(&port->PolicyStateTimer, tVBusSwitchDelay);

            port->PolicySubIndex++;
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 4:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            TimerDisable(&port->PolicyStateTimer);

            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 5:
        Status = PolicySendCommand(port, CMTPS_RDY, peSourceStartup,
                                   0, SOP_TYPE_SOP);
        if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
        else if (Status == STAT_SUCCESS)
        {
            TimerStart(&port->SwapSourceStartTimer, tSwapSourceStart);
        }
        break;
    default:
        port->PolicySubIndex = 0;
        break;
    }
#endif /* FSC_HAVE_DRP */
}

void PolicySinkEvaluatePRSwap(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Send either the Accept or Reject */
        if (!Accepts_PR_Swap_As_Snk ||
            (port->PortConfig.PortType != USBTypeC_DRP) ||
            (port->CapsReceived[0].FPDOSupply.DualRolePower == FALSE))
        {
            PolicySendCommand(port, CMTReject, peSinkReady, 0,
                              port->ProtocolMsgRxSop);
        }
        else
        {
            port->IsPRSwap = TRUE;
            if (PolicySendCommand(port, CMTAccept, peSinkEvaluatePRSwap, 1,
                                  SOP_TYPE_SOP) == STAT_SUCCESS)
            {
                port->PolicyHasContract = FALSE;
                port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;

                notify_observers(PD_NO_CONTRACT, port->I2cAddr, 0);

                TimerStart(&port->PolicyStateTimer, tPSSourceOff);
            }
        }
        break;
    case 1:
        /* Wait for PS_RDY and perform the swap */
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if (port->PolicyRxHeader.NumDataObjects == 0)
            {
                switch (port->PolicyRxHeader.MessageType)
                {
                case CMTPS_RDY:
                    RoleSwapToAttachedSource(port);
                    port->PolicyIsSource = TRUE;

                    port->Registers.Switches.POWERROLE = port->PolicyIsSource;
                    DeviceWrite(port->I2cAddr, regSwitches1, 1,
                                &port->Registers.Switches.byte[1]);

                    TimerDisable(&port->PolicyStateTimer);
                    port->PolicySubIndex++;
                    break;
                default:
                    break;
                }
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port, peErrorRecovery);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        /* Wait for VBUS to rise */
        if (isVSafe5V(port))
        {
            /* Delay once VBus is present for potential switch delay. */
            TimerStart(&port->PolicyStateTimer, tVBusSwitchDelay);

            port->PolicySubIndex++;
        }
        else
        {
            TimerStart(&port->VBusPollTimer, tVBusPollShort);
            port->PEIdle = TRUE;
        }
        break;
    case 3:
        if (TimerExpired(&port->PolicyStateTimer))
        {
            TimerDisable(&port->PolicyStateTimer);
            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 4:
        Status = PolicySendCommand(port, CMTPS_RDY, peSourceStartup,
                                   0, SOP_TYPE_SOP);
        if (Status == STAT_ERROR)
        {
            SetPEState(port, peErrorRecovery);
        }
        else if (Status == STAT_SUCCESS)
        {
            port->IsPRSwap = FALSE;
            TimerStart(&port->SwapSourceStartTimer, tSwapSourceStart);
        }
        break;
    default:
        port->PolicySubIndex = 0;
        break;
    }
#else
    PolicySendCommand(port, CMTReject, peSinkReady, 0, port->ProtocolMsgRxSop);
#endif /* FSC_HAVE_DRP */
}
#endif /* FSC_HAVE_SNK */

void PolicySinkAlertReceived(Port_t *port)
{
    if (port->PolicyRxDataObj[0].ADO.Battery ||
        port->PolicyRxDataObj[0].ADO.OCP ||
        port->PolicyRxDataObj[0].ADO.OTP ||
        port->PolicyRxDataObj[0].ADO.OpCondition ||
        port->PolicyRxDataObj[0].ADO.Input)
    {
        /* Send Get_Status */
        notify_observers(ALERT_EVENT, port->I2cAddr,
                         &port->PolicyRxDataObj[0].object);
    }

    SetPEState(port, peSinkReady);
}

#ifdef FSC_HAVE_VDM
void PolicyGiveVdm(Port_t *port)
{
    port->PEIdle = TRUE;

    if (port->ProtocolMsgRx &&
        port->PolicyRxHeader.MessageType == DMTVenderDefined)
    {
        /* Received something while trying to transmit */
        sendVdmMessageFailed(port);
    }
    else if (port->sendingVdmData)
    {

        FSC_U8 result = PolicySendData(port, DMTVenderDefined,
                                   port->vdm_msg_obj,
                                   port->vdm_msg_length*sizeof(doDataObject_t),
                                   port->vdm_next_ps, 0,
                                   port->VdmMsgTxSop, FALSE);
        if (result == STAT_SUCCESS)
        {
            if (expectingVdmResponse(port))
            {
                startVdmTimer(port, port->PolicyState);
            }
            else
            {
                resetPolicyState(port);
            }
            port->sendingVdmData = FALSE;
        }
        else if (result == STAT_ERROR)
        {
            sendVdmMessageFailed(port);
            port->sendingVdmData = FALSE;
        }

        port->PEIdle = FALSE;
    }
    else
    {
        sendVdmMessageFailed(port);
    }

    if (port->VdmTimerStarted && (TimerExpired(&port->VdmTimer)))
    {
        vdmMessageTimeout(port);
    }
}

void PolicyVdm(Port_t *port)
{
    FSC_U8 result;

    port->PEIdle = TRUE;

    if (port->ProtocolMsgRx)
    {
        port->ProtocolMsgRx = FALSE;

        if (port->PolicyRxHeader.NumDataObjects != 0)
        {
            switch (port->PolicyRxHeader.MessageType)
            {
            case DMTVenderDefined:
                convertAndProcessVdmMessage(port, port->ProtocolMsgRxSop);
                break;
            default:
                resetPolicyState(port);
                port->ProtocolMsgRx = TRUE;
                break;
            }
        }
        else
        {
            resetPolicyState(port);
            port->ProtocolMsgRx = TRUE;
        }
    }
    else
    {
        if (port->sendingVdmData)
        {
            result = PolicySendData(port, DMTVenderDefined,
                                    port->vdm_msg_obj,
                                    port->vdm_msg_length*sizeof(doDataObject_t),
                                    port->vdm_next_ps, 0,
                                    port->VdmMsgTxSop, FALSE);
            if (result == STAT_SUCCESS || result == STAT_ERROR)
            {
                port->sendingVdmData = FALSE;
            }

            port->PEIdle = FALSE;
        }
    }

    if (port->VdmTimerStarted && (TimerExpired(&port->VdmTimer)))
    {
        if (port->PolicyState == peDfpUfpVdmIdentityRequest)
        {
            port->AutoVdmState = AUTO_VDM_DONE;
        }
        vdmMessageTimeout(port);
    }
}

#endif /* FSC_HAVE_VDM */

void PolicyInvalidState(Port_t *port)
{
    /* reset if we get to an invalid state */
    if (port->PolicyIsSource)
    {
        SetPEState(port, peSourceSendHardReset);
    }
    else
    {
        SetPEState(port, peSinkSendHardReset);
    }
}

/* General PD Messaging */
void PolicySendHardReset(Port_t *port)
{
    FSC_U8 data;
    if (!port->IsHardReset)
    {
        port->HardResetCounter++;
        port->IsHardReset = TRUE;
        data = port->Registers.Control.byte[3] | 0x40;
        DeviceWrite(port->I2cAddr, regControl3, 1, &data);
    }
}

FSC_U8 PolicySendCommand(Port_t *port, FSC_U8 Command, PolicyState_t nextState,
                         FSC_U8 subIndex, SopType sop)
{
    FSC_U8 Status = STAT_BUSY;
    switch (port->PDTxStatus)
    {
    case txIdle:
        port->PolicyTxHeader.word = 0;
        port->PolicyTxHeader.NumDataObjects = 0;
        port->PolicyTxHeader.MessageType = Command;
        port->PolicyTxHeader.PortDataRole = port->PolicyIsDFP;
        if (sop == SOP_TYPE_SOP)
        {
            port->PolicyTxHeader.PortPowerRole = port->PolicyIsSource;
        }
        else
        {
            /* Cable plug field when SOP' & SOP'', currently not used */
            port->PolicyTxHeader.PortPowerRole = 0;
        }
        port->PolicyTxHeader.SpecRevision = port->PdRevContract;
        port->ProtocolMsgTxSop = sop;
        port->PDTxStatus = txSend;

        /* Shortcut to transmit */
        if (port->ProtocolState == PRLIdle) ProtocolIdle(port);
        break;
    case txSend:
    case txBusy:
    case txWait:
        /* Waiting for GoodCRC or timeout of the protocol */
        /* May want to put in a second level timeout in case there's
         * an issue with the protocol getting hung
         */
        break;
    case txSuccess:
        SetPEState(port, nextState);
        port->PolicySubIndex = subIndex;
        Status = STAT_SUCCESS;
        break;
    case txError:
        /* Didn't receive a GoodCRC message... */
        if (port->PolicyState == peSourceSendSoftReset)
        {
            SetPEState(port, peSourceSendHardReset);
        }
        else if (port->PolicyState == peSinkSendSoftReset)
        {
            SetPEState(port, peSinkSendHardReset);
        }
        else if (port->PolicyIsSource)
        {
            SetPEState(port, peSourceSendSoftReset);
        }
        else
        {
            SetPEState(port, peSinkSendSoftReset);
        }
        Status = STAT_ERROR;
        break;
    case txCollision:
        port->CollisionCounter++;
        if (port->CollisionCounter > nRetryCount)
        {
            if (port->PolicyIsSource)
                SetPEState(port, peSourceSendHardReset);
            else
                SetPEState(port, peSinkSendHardReset);

            port->PDTxStatus = txReset;
            Status = STAT_ERROR;
        }
        else
        {
            /* Clear the transmitter status for the next operation */
            port->PDTxStatus = txIdle;
        }
        break;
    default:
        if (port->PolicyIsSource)
            SetPEState(port, peSourceSendHardReset);
        else
            SetPEState(port, peSinkSendHardReset);

        port->PDTxStatus = txReset;
        Status = STAT_ERROR;
        break;
    }
    return Status;
}

FSC_U8 PolicySendData(Port_t *port, FSC_U8 MessageType, void* data,
                      FSC_U32 len, PolicyState_t nextState,
                      FSC_U8 subIndex, SopType sop, FSC_BOOL extMsg)
{
    FSC_U8 Status = STAT_BUSY;
    FSC_U32 i;
    FSC_U8* pData = (FSC_U8*)data;
    FSC_U8* pOutBuf = (FSC_U8*)port->PolicyTxDataObj;

    switch (port->PDTxStatus)
    {
    case txIdle:
    {
        port->PolicyTxHeader.word = 0;
        port->PolicyTxHeader.NumDataObjects = len / 4;
        port->PolicyTxHeader.MessageType = MessageType;
        port->PolicyTxHeader.PortDataRole = port->PolicyIsDFP;
        if (sop == SOP_TYPE_SOP)
        {
            port->PolicyTxHeader.PortPowerRole = port->PolicyIsSource;
        }
        else
        {
            /* Cable plug field when SOP' & SOP'', currently not used */
            port->PolicyTxHeader.PortPowerRole = 0;
        }
        port->PolicyTxHeader.SpecRevision = port->PdRevContract;

        if (extMsg == TRUE)
        {
#ifdef FSC_HAVE_EXT_MSG
            /* Set extended bit */
            port->PolicyTxHeader.Extended = 1;
            /* Initialize extended messaging state */
            port->ExtChunkOffset = 0;
            port->ExtChunkNum = 0;
            port->ExtTxOrRx = TXing;
            port->ExtWaitTxRx = FALSE;
            /* Set extended header */
            port->ExtTxHeader.word = 0;
            len = (len > EXT_MAX_MSG_LEN) ? EXT_MAX_MSG_LEN : len;
            port->ExtTxHeader.Chunked = 1;
            port->ExtTxHeader.DataSize = len;
            /* Set the tx buffer */
            pOutBuf = port->ExtMsgBuffer;
#endif /* FSC_HAVE_EXT_MSG */
        }

        /* Copy message */
        for (i = 0; i < len; i++)
        {
            pOutBuf[i] = pData[i];
        }

        if (port->PolicyState == peSourceSendCaps)
        {
                port->CapsCounter++;
        }
        port->ProtocolMsgTxSop = sop;
        port->PDTxStatus = txSend;

        /* Shortcut to transmit */
        if (port->ProtocolState == PRLIdle) ProtocolIdle(port);
        break;
    }
    case txSend:
    case txBusy:
    case txWait:
        /* Waiting for GoodCRC or timeout of the protocol */
        /* May want to put in a second level timeout in case there's
         * an issue with the protocol getting hung
         */
        break;
    case txCollision:
        Status = STAT_ERROR;
        break;
    case txSuccess:
#ifdef FSC_HAVE_EXT_MSG
        if (extMsg == TRUE &&
            port->ExtChunkOffset < port->ExtTxHeader.DataSize)
        {
            port->PDTxStatus = txBusy;
            break;
        }
#endif /* FSC_HAVE_EXT_MSG */
        SetPEState(port, nextState);
        port->PolicySubIndex = subIndex;
        Status = STAT_SUCCESS;
        break;
    case txError:
        /* Didn't receive a GoodCRC message... */
        if (port->PolicyState == peSourceSendCaps &&
            port->PolicyHasContract == FALSE)
        {
            SetPEState(port, peSourceDiscovery);
        }
        else if (port->PolicyIsSource)
        {
            SetPEState(port, peSourceSendSoftReset);
        }
        else
        {
            SetPEState(port, peSinkSendSoftReset);
        }

        Status = STAT_ERROR;
        break;
    default:
        if (port->PolicyIsSource)
            SetPEState(port, peSourceSendHardReset);
        else
            SetPEState(port, peSinkSendHardReset);

        port->PDTxStatus = txReset;
        Status = STAT_ERROR;
        break;
    }

    return Status;
}

void UpdateCapabilitiesRx(Port_t *port, FSC_BOOL IsSourceCaps)
{
    FSC_U32 i;
#ifdef FSC_DEBUG
    /* Set the flag to indicate that the received capabilities are valid */
    port->SourceCapsUpdated = IsSourceCaps;
#endif /* FSC_DEBUG */
    port->CapsHeaderReceived.word = port->PolicyRxHeader.word;

    for (i = 0; i < port->CapsHeaderReceived.NumDataObjects; i++)
        port->CapsReceived[i].object = port->PolicyRxDataObj[i].object;

    for (i = port->CapsHeaderReceived.NumDataObjects; i < 7; i++)
        port->CapsReceived[i].object = 0;

    port->PartnerCaps.object = port->CapsReceived[0].object;
}

#ifdef FSC_HAVE_EXT_MSG
void PolicyGiveCountryCodes(Port_t *port)
{
    FSC_U32 noCodes = gCountry_codes[0] | gCountry_codes[1] << 8;
    
    PolicySendData(port, EXTCountryCodes, gCountry_codes, noCodes*2+2,
                   port->PolicyIsSource ? peSourceReady : peSinkReady, 0,
                   SOP_TYPE_SOP, TRUE);
}

void PolicyGetCountryCodes(Port_t *port)
{
     PolicySendCommand(port, CMTGetCountryCodes,
                      port->PolicyIsSource ? peSourceReady : peSinkReady, 0,
                      SOP_TYPE_SOP);
}

void PolicyGiveCountryInfo(Port_t *port)
{
    /* Allocate 4-byte buffer so we don't need a full 260 byte data */
    FSC_U8 buf[4] = {0};
    CountryInfoReq *req = (CountryInfoReq*)(port->PolicyRxDataObj);
    /* Echo the first to bytes of country code */
    CountryInfoResp *resp = (CountryInfoResp*)buf;
    resp->CountryCode[1] = req->CountryCode[0];
    resp->CountryCode[0] = req->CountryCode[1];
    resp->Reserved[0] = resp->Reserved[1] = 0;
    PolicySendData(port, EXTCountryInfo, resp, sizeof(FSC_U32),
                   (port->PolicyIsSource == TRUE) ? peSourceReady : peSinkReady,
                           0, SOP_TYPE_SOP, TRUE);
}
#endif /* FSC_HAVE_EXT_MSG */

#ifdef FSC_HAVE_EXT_MSG
void PolicyGetPPSStatus(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    case 0:
        if (PolicySendCommand(port, CMTGetPPSStatus, peGetPPSStatus, 1,
                              SOP_TYPE_SOP) == STAT_SUCCESS)
        {
            TimerStart(&port->PolicyStateTimer, tSenderResponse);
            port->WaitingOnHR = TRUE;
        }
        break;
    default:
        if (port->ProtocolMsgRx)
        {
            port->ProtocolMsgRx = FALSE;
            if ((port->PolicyRxHeader.NumDataObjects > 0) &&
                (port->PolicyRxHeader.MessageType == EXTPPSStatus))
            {
                SetPEState(port,
                           port->PolicyIsSource ? peSourceReady : peSinkReady);
            }
            else
            {
                SetPEState(port, port->PolicyIsSource ?
                        peSourceSendHardReset : peSinkSendHardReset);
            }
        }
        else if (TimerExpired(&port->PolicyStateTimer))
        {
            SetPEState(port,
                       port->PolicyIsSource ? peSourceReady : peSinkReady);
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }
}

void PolicyGivePPSStatus(Port_t *port)
{
    PPSStatus_t ppsstatus;
    
    switch (port->PolicySubIndex)
    {
    case 0:
        /* Only need to get the values once */
        ppsstatus.OutputVoltage = platform_get_pps_voltage(port->PortID) / 20;
        ppsstatus.OutputCurrent = 0xFF; /* Not supported field for now */
        ppsstatus.byte[3] = 0x00;
        
        port->PolicySubIndex++;
        /* Fall through */
    case 1:
        PolicySendData(port, EXTPPSStatus, ppsstatus.byte, 4,
                       port->PolicyIsSource ? peSourceReady : peSinkReady, 0,
                       SOP_TYPE_SOP, TRUE);
        break;
    default:
        break;
    }
}
#endif /* FSC_HAVE_EXT */

/* BIST Receive Mode */
void policyBISTReceiveMode(Port_t *port)
{
    /* Not Implemented */
    /* Tell protocol layer to go to BIST Receive Mode
     * Go to BIST_Frame_Received if a test frame is received
     * Transition to SRC_Transition_to_Default, SNK_Transition_to_Default,
     * or CBL_Ready when Hard_Reset received
     */
}

void policyBISTFrameReceived(Port_t *port)
{
    /* Not Implemented */
    /* Consume BIST Transmit Test Frame if received
     * Transition back to BIST_Frame_Received when a BIST Test Frame
     * has been received. Transition to SRC_Transition_to_Default,
     * SNK_Transition_to_Default, or CBL_Ready when Hard_Reset received
     */
}

/* BIST Carrier Mode and Eye Pattern */
void policyBISTCarrierMode2(Port_t *port)
{
    switch (port->PolicySubIndex)
    {
    default:
    case 0:
        /* Tell protocol layer to go to BIST_Carrier_Mode_2 */
        port->Registers.Control.BIST_MODE2 = 1;
        DeviceWrite(port->I2cAddr, regControl1, 1,
                    &port->Registers.Control.byte[1]);

        /* Set the bit to enable the transmitter */
        port->Registers.Control.TX_START = 1;
        DeviceWrite(port->I2cAddr, regControl0, 1,
                    &port->Registers.Control.byte[0]);
        port->Registers.Control.TX_START = 0;

        TimerStart(&port->PolicyStateTimer, tBISTContMode);

        port->PolicySubIndex = 1;
        break;
    case 1:
        /* Disable and wait on GoodCRC */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            /* Disable BIST_Carrier_Mode_2 (PD_RESET does not do this) */
            port->Registers.Control.BIST_MODE2 = 0;
            DeviceWrite(port->I2cAddr, regControl1, 1,
                        &port->Registers.Control.byte[1]);

            /* Delay to allow preamble to finish */
            TimerStart(&port->PolicyStateTimer, tGoodCRCDelay);

            port->PolicySubIndex++;
        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    case 2:
        /* Transition to SRC_Transition_to_Default, SNK_Transition_to_Default,
         * or CBL_Ready when BISTContModeTimer times out
         */
        if (TimerExpired(&port->PolicyStateTimer))
        {
            ProtocolFlushTxFIFO(port);

            if (port->PolicyIsSource)
            {
#if (defined(FSC_HAVE_SRC) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
                SetPEState(port, peSourceSendHardReset);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */
            }
            else
            {
#ifdef FSC_HAVE_SNK
                SetPEState(port, peSinkSendHardReset);
#endif /* FSC_HAVE_SNK */
            }

        }
        else
        {
            port->PEIdle = TRUE;
        }
        break;
    }

}

void policyBISTTestData(Port_t *port)
{
    if (port->Registers.DeviceID.VERSION_ID == VERSION_302A)
    {
        /* Set the bit for RX_FLUSH when the state machine is woken up here */
        DeviceWrite(port->I2cAddr, regControl1, 1,
                    &port->Registers.Control.byte[1]);
    }

    /* Waiting for HR */
    port->PEIdle = TRUE;
}

#ifdef FSC_HAVE_VDM
void InitializeVdmManager(Port_t *port)
{
    initializeVdm(port);

    /* configure callbacks */
    port->vdmm.req_id_info = &vdmRequestIdentityInfo;
    port->vdmm.req_svid_info = &vdmRequestSvidInfo;
    port->vdmm.req_modes_info = &vdmRequestModesInfo;
    port->vdmm.enter_mode_result = &vdmEnterModeResult;
    port->vdmm.exit_mode_result = &vdmExitModeResult;
    port->vdmm.inform_id = &vdmInformIdentity;
    port->vdmm.inform_svids = &vdmInformSvids;
    port->vdmm.inform_modes = &vdmInformModes;
    port->vdmm.inform_attention = &vdmInformAttention;
    port->vdmm.req_mode_entry = &vdmModeEntryRequest;
    port->vdmm.req_mode_exit = &vdmModeExitRequest;
}

void convertAndProcessVdmMessage(Port_t *port, SopType sop)
{
    /* Form the word arrays that VDM block expects */
    FSC_U32 i;
    FSC_U32 vdm_arr[7];

    for (i = 0; i < port->PolicyRxHeader.NumDataObjects; i++)
    {
        vdm_arr[i] = 0;
        vdm_arr[i] = port->PolicyRxDataObj[i].object;
    }

    processVdmMessage(port, sop, vdm_arr, port->PolicyRxHeader.NumDataObjects);
}

void sendVdmMessage(Port_t *port, SopType sop, FSC_U32* arr,
                    FSC_U32 length, PolicyState_t next_ps)
{
    FSC_U32 i;
    /* 'cast' to type that PolicySendData expects
     * didn't think this was necessary, but it fixed some problems. - Gabe
     */
    port->vdm_msg_length = length;
    port->vdm_next_ps = next_ps;
    for (i = 0; i < port->vdm_msg_length; i++)
    {
        port->vdm_msg_obj[i].object = arr[i];
    }
    port->VdmMsgTxSop = sop;
    port->sendingVdmData = TRUE;
    port->ProtocolCheckRxBeforeTx = TRUE;
    port->VdmTimerStarted = FALSE;
    SetPEState(port, peGiveVdm);
}

void doVdmCommand(Port_t *port)
{
    FSC_U32 command;
    FSC_U32 svid;
    FSC_U32 mode_index;
    SopType sop;

    if (port->PDTransmitObjects[0].UVDM.VDMType == 0)
    {
        /* Transmit unstructured VDM messages directly from the GUI/DPM */
        PolicySendData(port, DMTVenderDefined,
                       port->PDTransmitObjects,
                       port->PDTransmitHeader.NumDataObjects *
                           sizeof(doDataObject_t),
                       port->PolicyIsSource == TRUE ? peSourceReady:peSinkReady,
                       0, port->PolicyMsgTxSop, FALSE);
        return;
    }

    command = port->PDTransmitObjects[0].byte[0] & 0x1F;
    svid = 0;
    svid |= (port->PDTransmitObjects[0].byte[3] << 8);
    svid |= (port->PDTransmitObjects[0].byte[2] << 0);

    mode_index = 0;
    mode_index = port->PDTransmitObjects[0].byte[1] & 0x7;

    /* PolicyMsgTxSop must be set with correct type by calling
     * routine when setting USBPDTxFlag
     */
    sop = port->PolicyMsgTxSop;

#ifdef FSC_HAVE_DP
    if (svid == DP_SID)
    {
        if (command == DP_COMMAND_STATUS)
        {
            requestDpStatus(port);
        }
        else if (command == DP_COMMAND_CONFIG)
        {
            DisplayPortConfig_t temp;
            temp.word = port->PDTransmitObjects[1].object;
            requestDpConfig(port, temp);
        }
    }
#endif /* FSC_HAVE_DP */

    if (command == DISCOVER_IDENTITY)
    {
        requestDiscoverIdentity(port, sop);
    }
    else if (command == DISCOVER_SVIDS)
    {
        requestDiscoverSvids(port, sop);
    }
    else if (command == DISCOVER_MODES)
    {
        requestDiscoverModes(port, sop, svid);
    }
    else if (command == ENTER_MODE)
    {
        requestEnterMode(port, sop, svid, mode_index);
    }
    else if (command == EXIT_MODE)
    {
        requestExitMode(port, sop, svid, mode_index);
    }
}

/* This function assumes we're already in either Source or Sink Ready states! */
void autoVdmDiscovery(Port_t *port)
{
    /* Wait for protocol layer to become idle */
    if (port->PDTxStatus == txIdle)
    {
        switch (port->AutoVdmState)
        {
        case AUTO_VDM_INIT:
        case AUTO_VDM_DISCOVER_ID_PP:
            requestDiscoverIdentity(port, SOP_TYPE_SOP);
            port->AutoVdmState = AUTO_VDM_DISCOVER_SVIDS_PP;
            break;
        case AUTO_VDM_DISCOVER_SVIDS_PP:
            requestDiscoverSvids(port, SOP_TYPE_SOP);
            port->AutoVdmState = AUTO_VDM_DISCOVER_MODES_PP;
            break;
        case AUTO_VDM_DISCOVER_MODES_PP:
            if (port->auto_mode_disc_tracker == port->core_svid_info.num_svids)
            {
                port->AutoVdmState = AUTO_VDM_ENTER_MODE_PP;
                port->auto_mode_disc_tracker = 0;
            }
            else
            {
                requestDiscoverModes(port, SOP_TYPE_SOP,
                    port->core_svid_info.svids[port->auto_mode_disc_tracker]);
                port->auto_mode_disc_tracker++;
            }
            break;
        case AUTO_VDM_ENTER_MODE_PP:
            if (port->AutoModeEntryObjPos > 0)
            {
                requestEnterMode(port, SOP_TYPE_SOP, port->requestSvid,
                                 port->AutoModeEntryObjPos);
                port->AutoVdmState = AUTO_VDM_DP_GET_STATUS;
            }
            else
            {
                port->AutoVdmState = AUTO_VDM_DONE;
            }
            break;
        case AUTO_VDM_DP_GET_STATUS:
#ifdef FSC_HAVE_DP
            if (port->DpModeEntered)
            {
                requestDpStatus(port);
            }
#endif /* FSC_HAVE_DP */
            port->AutoVdmState = AUTO_VDM_DONE;
            break;
        default:
            port->AutoVdmState = AUTO_VDM_DONE;
            break;
        }
    }
}

#endif /* FSC_HAVE_VDM */

/* This function is FUSB302 specific */
SopType TokenToSopType(FSC_U8 data)
{
    SopType ret;
    FSC_U8 maskedSop = data & 0xE0;

    if (maskedSop == SOP_CODE_SOP)              ret = SOP_TYPE_SOP;
    else if (maskedSop == SOP_CODE_SOP1)        ret = SOP_TYPE_SOP1;
    else if (maskedSop == SOP_CODE_SOP2)        ret = SOP_TYPE_SOP2;
    else if (maskedSop == SOP_CODE_SOP1_DEBUG)  ret = SOP_TYPE_SOP2_DEBUG;
    else if (maskedSop == SOP_CODE_SOP2_DEBUG)  ret = SOP_TYPE_SOP2_DEBUG;
    else                                        ret = SOP_TYPE_ERROR;

    return ret;
}

void processDMTBIST(Port_t *port)
{
    FSC_U8 bdo = port->PolicyRxDataObj[0].byte[3] >> 4;

    notify_observers(BIST_ENABLED, port->I2cAddr, 0);

    switch (bdo)
    {
    case BDO_BIST_Carrier_Mode_2:
        /* Only enter BIST for 5V contract */
        if (DPM_GetSourceCap(port->dpm, port)[
                port->USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage
                == 100)
        {
            SetPEState(port, PE_BIST_Carrier_Mode_2);
            port->ProtocolState = PRLIdle;
        }

        /* Don't idle here - mode setup in next state */
        break;
    case BDO_BIST_Test_Data:
    default:
        /* Only enter BIST for 5V contract */
        if (DPM_GetSourceCap(port->dpm, port)[
                port->USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage
                == 100)
        {
            if (port->Registers.DeviceID.VERSION_ID == VERSION_302A)
            {
                /* Mask for VBUS and Hard Reset */
                port->Registers.Mask.byte = 0xFF;
                port->Registers.Mask.M_VBUSOK = 0;
                port->Registers.Mask.M_COMP_CHNG = 0;
                DeviceWrite(port->I2cAddr, regMask, 1,
                            &port->Registers.Mask.byte);
                port->Registers.MaskAdv.byte[0] = 0xFF;
                port->Registers.MaskAdv.M_HARDRST = 0;
                DeviceWrite(port->I2cAddr, regMaska, 1,
                            &port->Registers.MaskAdv.byte[0]);
                port->Registers.MaskAdv.M_GCRCSENT = 1;
                DeviceWrite(port->I2cAddr, regMaskb, 1,
                            &port->Registers.MaskAdv.byte[1]);

                /* Enable RxFIFO flushing */
                port->Registers.Control.RX_FLUSH = 1;
                /* TODO - Does this need a write? */
            }
            else
            {
                /* Auto-flush RxFIFO */
                port->Registers.Control.BIST_TMODE = 1;
                DeviceWrite(port->I2cAddr, regControl3, 1,
                            &port->Registers.Control.byte[3]);
            }

            SetPEState(port, PE_BIST_Test_Data);

            /* Disable Protocol layer so we don't read FIFO */
            port->ProtocolState = PRLDisabled;
        }

        port->PEIdle = TRUE;
        break;
    }
}

#ifdef FSC_DEBUG
void SendUSBPDHardReset(Port_t *port)
{
    /* Src or Snk Hard Reset */
    if (port->PolicyIsSource)
    {
        SetPEState(port, peSourceSendHardReset);
    }
    else
    {
        SetPEState(port, peSinkSendHardReset);
    }
}

#ifdef FSC_HAVE_SNK
void WriteSinkRequestSettings(Port_t *port, FSC_U8* abytData)
{
    FSC_U32 uintPower;
    port->PortConfig.SinkGotoMinCompatible = *abytData & 0x01 ? TRUE : FALSE;
    port->PortConfig.SinkUSBSuspendOperation = *abytData & 0x02 ? TRUE : FALSE;
    port->PortConfig.SinkUSBCommCapable = *abytData++ & 0x04 ? TRUE : FALSE;

    /* Voltage resolution is 50mV */
    port->PortConfig.SinkRequestMaxVoltage = (FSC_U32) *abytData++;
    port->PortConfig.SinkRequestMaxVoltage  |= ((FSC_U32) (*abytData++) << 8);

    uintPower = (FSC_U32) *abytData++;
    uintPower |= ((FSC_U32) (*abytData++) << 8);
    uintPower |= ((FSC_U32) (*abytData++) << 16);
    uintPower |= ((FSC_U32) (*abytData++) << 24);

    /* Power resolution is 0.5mW */
    port->PortConfig.SinkRequestOpPower = uintPower;

    uintPower = (FSC_U32) *abytData++;
    uintPower |= ((FSC_U32) (*abytData++) << 8);
    uintPower |= ((FSC_U32) (*abytData++) << 16);
    uintPower |= ((FSC_U32) (*abytData++) << 24);

    /* Power resolution is 0.5mW */
    port->PortConfig.SinkRequestMaxPower = uintPower;

    /* We could try resetting and re-evaluating the source caps here,
     * but lets not do anything until requested by the user
     * (soft reset or detach)
     */
}

void ReadSinkRequestSettings(Port_t *port, FSC_U8* abytData)
{
    *abytData = port->PortConfig.SinkGotoMinCompatible ? 0x01 : 0;
    *abytData |= port->PortConfig.SinkUSBSuspendOperation ? 0x02 : 0;
    *abytData++ |= port->PortConfig.SinkUSBCommCapable ? 0x04 : 0;
    *abytData++ = (FSC_U8) (port->PortConfig.SinkRequestMaxVoltage & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestMaxVoltage >> 8)&0xFF);
    *abytData++ = (FSC_U8) (port->PortConfig.SinkRequestOpPower & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestOpPower >> 8) & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestOpPower >> 16) & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestOpPower >> 24) & 0xFF);
    *abytData++ = (FSC_U8) (port->PortConfig.SinkRequestMaxPower & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestMaxPower >> 8) & 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestMaxPower >> 16)& 0xFF);
    *abytData++ = (FSC_U8) ((port->PortConfig.SinkRequestMaxPower >> 24)& 0xFF);
}
#endif /* FSC_HAVE_SNK */

#endif /* FSC_DEBUG */
