/**
 *******************************************************************************
 * @file        port.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines structure and associated methods for storage of all
 *              port and state machine related items.
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
#include "Port.h"
#include "TypeC.h"
#include "PDPolicy.h"
#include "PDProtocol.h"

#ifdef FSC_HAVE_VDM
#include "vdm_callbacks.h"
#endif /* FSC_HAVE_VDM */

/* Forward Declarations */
static void SetPortDefaultConfiguration(Port_t *port);
void InitializeRegisters(Port_t *port);
void InitializeTypeCVariables(Port_t *port);
void InitializePDProtocolVariables(Port_t *port);
void InitializePDPolicyVariables(Port_t *port);

void PortInit(Port_t *port, FSC_U8 i2cAddr)
{
    port->I2cAddr = i2cAddr;
    port->PdRevContract = PD_Specification_Revision;
    SetPortDefaultConfiguration(port);
    InitializeRegisters(port);
    InitializeTypeCVariables(port);
    InitializePDProtocolVariables(port);
    InitializePDPolicyVariables(port);
}

void SetTypeCState(Port_t *port, ConnectionState state)
{
    port->ConnState = state;
    port->TypeCSubState = 0;

#ifdef FSC_DEBUG
    WriteStateLog(&port->TypeCStateLog, port->ConnState,
                  platform_get_log_time());
#endif /* FSC_DEBUG */
}

void SetPEState(Port_t *port, PolicyState_t state)
{
    port->PolicyState = state;
    port->PolicySubIndex = 0;

    port->PDTxStatus = txIdle;
    port->WaitingOnHR = FALSE;

    notify_observers(PD_STATE_CHANGED, port->I2cAddr, 0);
#ifdef FSC_DEBUG
    WriteStateLog(&port->PDStateLog, port->PolicyState,
                  platform_get_log_time());
#endif /* FSC_DEBUG */
}

/**
 * Initalize port policy variables to default. These are changed later by
 * policy manager.
 */
static void SetPortDefaultConfiguration(Port_t *port)
{
#ifdef FSC_HAVE_SNK
    port->PortConfig.SinkRequestMaxVoltage   = 0;
    port->PortConfig.SinkRequestMaxPower     = (PD_Power_as_Sink << 1);
    port->PortConfig.SinkRequestOpPower      = (PD_Power_as_Sink << 1);
    port->PortConfig.SinkGotoMinCompatible   = FALSE;
    port->PortConfig.SinkUSBSuspendOperation = No_USB_Suspend_May_Be_Set;
    port->PortConfig.SinkUSBCommCapable      = USB_Comms_Capable;
#endif /* FSC_HAVE_SNK */

#ifdef FSC_HAVE_ACCMODE
    port->PortConfig.audioAccSupport   =Type_C_Supports_Audio_Accessory;
    port->PortConfig.poweredAccSupport =Type_C_Supports_Vconn_Powered_Accessory;
#endif /* FSC_HAVE_ACCMODE */

    port->PortConfig.RpVal = RP_Value;

    switch (Type_C_State_Machine)
    {
    case 0:
        port->PortConfig.PortType = USBTypeC_Source;
        break;
    case 1:
        port->PortConfig.PortType = USBTypeC_Sink;
        break;
    case 2:
        port->PortConfig.PortType = USBTypeC_DRP;
        break;
    default:
        port->PortConfig.PortType = USBTypeC_UNDEFINED;
        break;
    }

#ifdef FSC_HAVE_DRP
    if (port->PortConfig.PortType == USBTypeC_DRP)
    {
        port->PortConfig.SrcPreferred = Type_C_Implements_Try_SRC;
        port->PortConfig.SnkPreferred = Type_C_Implements_Try_SNK;
    }
    else
    {
        port->PortConfig.SrcPreferred = FALSE;
        port->PortConfig.SnkPreferred = FALSE;
    }
#endif /* FSC_HAVE_DRP */
}

void InitializeRegisters(Port_t *port)
{
    FSC_U8 reset = 0x01;
    DeviceWrite(port->I2cAddr, regReset, 1, &reset);

    DeviceRead(port->I2cAddr, regDeviceID, 1, &port->Registers.DeviceID.byte);
    DeviceRead(port->I2cAddr, regSwitches0,1,&port->Registers.Switches.byte[0]);
    DeviceRead(port->I2cAddr, regSwitches1,1,&port->Registers.Switches.byte[1]);
    DeviceRead(port->I2cAddr, regMeasure, 1, &port->Registers.Measure.byte);
    DeviceRead(port->I2cAddr, regSlice, 1, &port->Registers.Slice.byte);
    DeviceRead(port->I2cAddr, regControl0, 1, &port->Registers.Control.byte[0]);
    DeviceRead(port->I2cAddr, regControl1, 1, &port->Registers.Control.byte[1]);
    DeviceRead(port->I2cAddr, regControl2, 1, &port->Registers.Control.byte[2]);
    DeviceRead(port->I2cAddr, regControl3, 1, &port->Registers.Control.byte[3]);
    DeviceRead(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    DeviceRead(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);
    DeviceRead(port->I2cAddr, regReset, 1, &port->Registers.Reset.byte);
    DeviceRead(port->I2cAddr, regOCPreg, 1, &port->Registers.OCPreg.byte);
    DeviceRead(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    DeviceRead(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);
    DeviceRead(port->I2cAddr, regStatus0a, 1, &port->Registers.Status.byte[0]);
    DeviceRead(port->I2cAddr, regStatus1a, 1, &port->Registers.Status.byte[1]);
    DeviceRead(port->I2cAddr, regInterrupta, 1,&port->Registers.Status.byte[2]);
    DeviceRead(port->I2cAddr, regInterruptb, 1,&port->Registers.Status.byte[3]);
    DeviceRead(port->I2cAddr, regStatus0, 1, &port->Registers.Status.byte[4]);
    DeviceRead(port->I2cAddr, regStatus1, 1, &port->Registers.Status.byte[5]);
    DeviceRead(port->I2cAddr, regInterrupt, 1, &port->Registers.Status.byte[6]);
}

void InitializeTypeCVariables(Port_t *port)
{
    port->Registers.Mask.byte = 0xFF;
    DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    port->Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    port->Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);

    /* Enable interrupt Pin */
    port->Registers.Control.INT_MASK = 0;
    DeviceWrite(port->I2cAddr, regControl0, 1,&port->Registers.Control.byte[0]);

    /* These two control values allow detection of Ra-Ra or Ra-Open.
     * Enabling them will allow some corner case devices to connect where
     * they might not otherwise.
     */
    port->Registers.Control.TOG_RD_ONLY = 0;
    DeviceWrite(port->I2cAddr, regControl2, 1,&port->Registers.Control.byte[2]);
    port->Registers.Control4.TOG_USRC_EXIT = 0;
    DeviceWrite(port->I2cAddr, regControl4, 1, &port->Registers.Control4.byte);

    port->TCIdle = TRUE;
    port->SMEnabled = FALSE;

    SetTypeCState(port, Disabled);

    port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;
    port->CCPin = CCNone;
    port->C2ACable = FALSE;

    TimerDisable(&port->StateTimer);
    TimerDisable(&port->LoopCountTimer);
    TimerDisable(&port->PDDebounceTimer);
    TimerDisable(&port->CCDebounceTimer);
    TimerDisable(&port->VBusPollTimer);

    resetDebounceVariables(port);

#ifdef FSC_DTS
    port->DTSMode = FALSE;
#endif /* FSC_DTS */

#ifdef FSC_HAVE_SNK
    /* Clear the current advertisement initially */
    port->SinkCurrent = utccNone;
#endif /* FSC_HAVE_SNK */

#ifdef FSC_DEBUG
    InitializeStateLog(&port->TypeCStateLog);
#endif /* FSC_DEBUG */
}

void InitializePDProtocolVariables(Port_t *port)
{
#ifdef FSC_GSCE_FIX
    port->NextGSCE = FALSE;
#endif /* FSC_GSCE_FIX */

    port->DoTxFlush = FALSE;
    port->ManualRetriesEnabled = FALSE;
    port->DoManualRetry = FALSE;
    port->ManualRetries = 0;
}

void InitializePDPolicyVariables(Port_t *port)
{
    port->ProtocolCheckRxBeforeTx = FALSE;
    port->isContractValid = FALSE;

    port->IsHardReset = FALSE;
    port->IsPRSwap = FALSE;

    port->WaitingOnHR = FALSE;

    port->PEIdle = TRUE;
    port->USBPDActive = FALSE;
    port->USBPDEnabled = TRUE;

#ifdef FSC_DEBUG
    port->SourceCapsUpdated = FALSE;
#endif /* FSC_DEBUG */

    /* Source Caps & Header */
    port->src_cap_header.word = 0;
    port->src_cap_header.NumDataObjects = NUMBER_OF_SRC_PDOS_ENABLED;
    port->src_cap_header.MessageType    = DMTSourceCapabilities;
    port->src_cap_header.SpecRevision   = PD_Specification_Revision;

    VIF_InitializeSrcCaps(port->src_caps);

    /* Sink Caps & Header */
    port->snk_cap_header.word = 0;
    port->snk_cap_header.NumDataObjects = NUMBER_OF_SNK_PDOS_ENABLED;
    port->snk_cap_header.MessageType    = DMTSinkCapabilities;
    port->snk_cap_header.SpecRevision   = PD_Specification_Revision;

    VIF_InitializeSnkCaps(port->snk_caps);

#ifdef FSC_HAVE_VDM
    InitializeVdmManager(port);
    vdmInitDpm(port);
    port->auto_mode_disc_tracker = 0;
    port->AutoModeEntryObjPos = -1;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_DEBUG
    InitializeStateLog(&port->PDStateLog);
#endif /* FSC_DEBUG */
}

void SetConfiguredCurrent(Port_t *port)
{
    switch (port->PortConfig.RpVal)
    {
    case 0:
        port->SourceCurrent = utccDefault;
        break;
    case 1:
        port->SourceCurrent = utcc1p5A;
        break;
    case 2:
        port->SourceCurrent = utcc3p0A;
        break;
    default:
        port->SourceCurrent = utccNone;
        break;
    }
    updateSourceCurrent(port);
}
