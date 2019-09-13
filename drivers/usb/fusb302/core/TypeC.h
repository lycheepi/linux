/**
 *******************************************************************************
 * @file        typec.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines Type-C state machine functionality.
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

#ifndef _FSC_TYPEC_H_
#define	_FSC_TYPEC_H_

#include "platform.h"
#include "Port.h"

/* Type C Timing Parameters */
#define tAMETimeout     900 * TICK_SCALE_TO_MS /* Alternate Mode Entry Time */
#define tCCDebounce     120 * TICK_SCALE_TO_MS
#define tPDDebounce     15  * TICK_SCALE_TO_MS
#define tDRPTry         90  * TICK_SCALE_TO_MS
#define tDRPTryWait     600 * TICK_SCALE_TO_MS
#define tErrorRecovery  30  * TICK_SCALE_TO_MS /* Delay in Error Recov State */

#define tDeviceToggle   3   * TICK_SCALE_TO_MS /* CC pin measure toggle */
#define tTOG2           30  * TICK_SCALE_TO_MS /* DRP Toggle timing */
#define tIllegalCable   500 * TICK_SCALE_TO_MS /* Reconnect loop reset time */
#define tOrientedDebug  100 * TICK_SCALE_TO_MS /* DebugAcc Orient Delay */
#define tLoopReset      100 * TICK_SCALE_TO_MS
#define tAttachWaitPoll 20  * TICK_SCALE_TO_MS /* Periodic poll in AW.Src */
#define tAttachWaitAdv  20  * TICK_SCALE_TO_MS /* Switch from Default to correct
                                                  advertisement in AW.Src */

/* Attach-Detach loop count - Halt after N loops */
#define MAX_CABLE_LOOP  20

void StateMachineTypeC(Port_t *port);
void StateMachineDisabled(Port_t *port);
void StateMachineErrorRecovery(Port_t *port);
void StateMachineUnattached(Port_t *port);

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(Port_t *port);
void StateMachineAttachedSink(Port_t *port);
void StateMachineTryWaitSink(Port_t *port);
void StateMachineDebugAccessorySink(Port_t *port);
#endif /* FSC_HAVE_SNK */

#if (defined(FSC_HAVE_DRP) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void StateMachineTrySink(Port_t *port);
#endif /* FSC_HAVE_DRP || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(Port_t *port);
void StateMachineTryWaitSource(Port_t *port);
#ifdef FSC_HAVE_DRP
void StateMachineUnattachedSource(Port_t *port);    /* AW.Snk -> Unattached */
#endif /* FSC_HAVE_DRP */
void StateMachineAttachedSource(Port_t *port);
void StateMachineTrySource(Port_t *port);
void StateMachineDebugAccessorySource(Port_t *port);
#endif /* FSC_HAVE_SRC */

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(Port_t *port);
void StateMachineAudioAccessory(Port_t *port);
void StateMachinePoweredAccessory(Port_t *port);
void StateMachineUnsupportedAccessory(Port_t *port);
void SetStateAudioAccessory(Port_t *port);
#endif /* FSC_HAVE_ACCMODE */

#ifdef FSC_DTS
void StateMachineAttachWaitDebugSink(Port_t *port);
void StateMachineAttachedDebugSink(Port_t *port);
void StateMachineAttachWaitDebugSource(Port_t *port);
void StateMachineAttachedDebugSource(Port_t *port);
void StateMachineTryDebugSource(Port_t *port);
void StateMachineTryWaitDebugSink(Port_t *port);
void StateMachineUnattachedDebugSource(Port_t *port);
#endif /* FSC_DTS */

void SetStateErrorRecovery(Port_t *port);
void SetStateUnattached(Port_t *port);

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(Port_t *port);
void SetStateAttachedSink(Port_t *port);
void SetStateDebugAccessorySink(Port_t *port);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(Port_t *port);
#endif /* FSC_HAVE_DRP */
void SetStateTryWaitSink(Port_t *port);
#endif /* FSC_HAVE_SNK */

#if (defined(FSC_HAVE_DRP) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void SetStateTrySink(Port_t *port);
#endif /* FSC_HAVE_DRP || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(Port_t *port);
void SetStateAttachedSource(Port_t *port);
void SetStateDebugAccessorySource(Port_t *port);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(Port_t *port);
#endif /* FSC_HAVE_DRP */
void SetStateTrySource(Port_t *port);
void SetStateTryWaitSource(Port_t *port);
#ifdef FSC_HAVE_DRP
void SetStateUnattachedSource(Port_t *port);
#endif /* FSC_HAVE_DRP */
#endif /* FSC_HAVE_SRC */

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void SetStateAttachWaitAccessory(Port_t *port);
void SetStateUnsupportedAccessory(Port_t *port);
void SetStatePoweredAccessory(Port_t *port);
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */

void SetStateIllegalCable(Port_t *port);
void StateMachineIllegalCable(Port_t *port);

#ifdef FSC_DTS
void SetStateAttachWaitDebugSink(Port_t *port);
void SetStateAttachedDebugSink(Port_t *port);
void SetStateAttachWaitDebugSource(Port_t *port);
void SetStateAttachedDebugSource(Port_t *port);
void SetStateTryDebugSource(Port_t *port);
void SetStateTryWaitDebugSink(Port_t *port);
void SetStateUnattachedDebugSource(Port_t *port);
#endif /* FSC_DTS */

void updateSourceCurrent(Port_t *port);
void updateSourceMDACHigh(Port_t *port);
void updateSourceMDACLow(Port_t *port);

void ToggleMeasure(Port_t *port);

CCTermType DecodeCCTermination(Port_t *port);
#if defined(FSC_HAVE_SRC) || \
    (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(Port_t *port);
FSC_BOOL IsCCPinRa(Port_t *port);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */
#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(Port_t *port);
#endif /* FSC_HAVE_SNK */

void UpdateSinkCurrent(Port_t *port, CCTermType term);
FSC_BOOL VbusVSafe0V (Port_t *port);

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(Port_t *port);
#endif /* FSC_HAVE_SNK */

FSC_BOOL isVSafe5V(Port_t *port);
FSC_BOOL isVBUSOverVoltage(Port_t *port, FSC_U16 vbus_mv);

void resetDebounceVariables(Port_t *port);
void setDebounceVariables(Port_t *port, CCTermType term);
void setDebounceVariables(Port_t *port, CCTermType term);
void debounceCC(Port_t *port);

#if defined(FSC_HAVE_SRC) || \
    (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void setStateSource(Port_t *port, FSC_BOOL vconn);
void DetectCCPinSource(Port_t *port);
void updateVCONNSource(Port_t *port);
void updateVCONNSource(Port_t *port);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SNK
void setStateSink(Port_t *port);
void DetectCCPinSink(Port_t *port);
void updateVCONNSource(Port_t *port);
void updateVCONNSink(Port_t *port);
#endif /* FSC_HAVE_SNK */

void clearState(Port_t *port);

void UpdateCurrentAdvert(Port_t *port, USBTypeCCurrent Current);

#ifdef FSC_DEBUG
void SetStateDisabled(Port_t *port);

/* Returns local registers as data array */
FSC_BOOL GetLocalRegisters(Port_t *port, FSC_U8 * data, FSC_S32 size);

void setAlternateModes(FSC_U8 mode);
FSC_U8 getAlternateModes(void);
#endif /* FSC_DEBUG */

#endif/* _FSC_TYPEC_H_ */

