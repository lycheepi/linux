/**
 ****************************************//*************************************
 * @file        core.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       This file should be included by the platform. It includes
 *              functions that the core provides.
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

#include "core.h"
#include "TypeC.h"
#include "PDProtocol.h"
#include "PDPolicy.h"
#include "TypeC_Types.h"
#include "PD_Types.h"
#include "version.h"
/*
 * Call this function to initialize the core.
 */
void core_initialize(Port_t *port, FSC_U8 i2cAddr)
{
    PortInit(port, i2cAddr);
    core_enable_typec(port, TRUE);
    core_set_state_unattached(port);
}

/*
 * Call this function to enable or disable the core Type-C state machine.
 */
void core_enable_typec(Port_t *port, FSC_BOOL enable)
{
    port->SMEnabled = enable;
}

/*
 * Call this function to enable or disable the core PD state machines.
 */
void core_enable_pd(Port_t *port, FSC_BOOL enable)
{
    port->USBPDEnabled = enable;
}

/*
 * Call this function to run the state machines.
 */
void core_state_machine(Port_t *port)
{
    FSC_U8 data = port->Registers.Control.byte[3] | 0x40;  /* Hard Reset bit */

    /* Check on HardReset timeout (shortcut for SenderResponse timeout) */
    if ((port->WaitingOnHR == TRUE) &&
        TimerExpired(&port->PolicyStateTimer))
    {
        DeviceWrite(port->I2cAddr, regControl3, 1, &data);
    }

    /* Update the current port being used and process the port */
    /* The Protocol and Policy functions are called from within this call */
    StateMachineTypeC(port);
}

/*
 * Check for the next required timeout to support timer interrupt functionality
 */
FSC_U32 core_get_next_timeout(Port_t *port)
{
  FSC_U32 time = 0;
  FSC_U32 nexttime = 0xFFFFFFFF;

  time = TimerRemaining(&port->PpsTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->StateTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->VBusPollTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->LoopCountTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->PolicyStateTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->ProtocolTimer);
  if (time > 0 && time < nexttime) nexttime = time;

#ifdef FSC_HAVE_VDM
  time = TimerRemaining(&port->VdmTimer);
  if (time > 0 && time < nexttime) nexttime = time;
#endif /* FSC_HAVE_VDM */

  time = TimerRemaining(&port->SwapSourceStartTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->PDDebounceTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  time = TimerRemaining(&port->CCDebounceTimer);
  if (time > 0 && time < nexttime) nexttime = time;

  if (nexttime == 0xFFFFFFFF) nexttime = 0;

  return nexttime;
}

FSC_U8 core_get_rev_lower(void)
{
    return FSC_TYPEC_CORE_FW_REV_LOWER;
}

FSC_U8 core_get_rev_middle(void)
{
    return FSC_TYPEC_CORE_FW_REV_MIDDLE;
}

FSC_U8 core_get_rev_upper(void)
{
    return FSC_TYPEC_CORE_FW_REV_UPPER;
}

FSC_U8 core_get_cc_orientation(Port_t *port)
{
    return port->CCPin;
}

void core_send_hard_reset(Port_t *port)
{
#ifdef FSC_DEBUG
    SendUSBPDHardReset(port);
#endif
}

void core_set_manual_retries(Port_t *port, FSC_BOOL retries)
{
    port->ManualRetriesEnabled = retries;
}

FSC_BOOL core_get_manual_retries(Port_t *port)
{
    return port->ManualRetriesEnabled;
}

void core_set_state_unattached(Port_t *port)
{
    SetStateUnattached(port);
}

void core_reset_pd(Port_t *port)
{
    port->USBPDEnabled = TRUE;
    USBPDEnable(port, TRUE, port->sourceOrSink);
}

FSC_U16 core_get_advertised_current(Port_t *port)
{
    FSC_U16 current = 0;

    if (port->sourceOrSink == SINK)
    {
        if (port->PolicyHasContract)
        {
            /* If there is a PD contract - return contract current. */
            /* TODO - add PPS handling, etc. */
            current = port->USBPDContract.FVRDO.OpCurrent * 10;
        }
        else
        {
            /* Otherwise, return the TypeC advertised value... or... */
            /* Note for Default: This can be
             * 500mA for USB 2.0
             * 900mA for USB 3.1
             * Up to 1.5A for USB BC 1.2
             */
            switch (port->SinkCurrent)
            {
            case utccDefault:
                current = 500;
                break;
            case utcc1p5A:
                current = 1500;
                break;
            case utcc3p0A:
                current = 3000;
                break;
            case utccNone:
            default:
                current = 0;
                break;
            }
        }
    }
    return current;
}

void core_set_advertised_current(Port_t *port, FSC_U8 value)
{
    UpdateCurrentAdvert(port, value);
}

void core_set_drp(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    port->PortConfig.PortType = USBTypeC_DRP;
    port->PortConfig.SnkPreferred = FALSE;
    port->PortConfig.SrcPreferred = FALSE;
    SetStateUnattached(port);
#endif /* FSC_HAVE_DRP */
}

void core_set_try_snk(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    port->PortConfig.PortType = USBTypeC_DRP;
    port->PortConfig.SnkPreferred = TRUE;
    port->PortConfig.SrcPreferred = FALSE;
    SetStateUnattached(port);
#endif /* FSC_HAVE_DRP */
}

void core_set_try_src(Port_t *port)
{
#ifdef FSC_HAVE_DRP
    port->PortConfig.PortType = USBTypeC_DRP;
    port->PortConfig.SnkPreferred = FALSE;
    port->PortConfig.SrcPreferred = TRUE;
    SetStateUnattached(port);
#endif /* FSC_HAVE_DRP */
}

void core_set_source(Port_t *port)
{
#ifdef FSC_HAVE_SRC
    port->PortConfig.PortType = USBTypeC_Source;
    SetStateUnattached(port);
#endif /* FSC_HAVE_SRC */
}

void core_set_sink(Port_t *port)
{
#ifdef FSC_HAVE_SNK
    port->PortConfig.PortType = USBTypeC_Sink;
    SetStateUnattached(port);
#endif /* FSC_HAVE_SNK */
}

