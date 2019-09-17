/***************************************//*************************************
 * FileName:        dpm.h
 * Company:         ON Semiconductor
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
 *
 *****************************************************************************/
#ifndef MODULES_DPM_H_
#define MODULES_DPM_H_

#include "platform.h"
#include "PD_Types.h"
#include "TypeC_Types.h"

/*****************************************************************************
 * Forward declarations
 *****************************************************************************/
typedef struct devicePolicy_t DevicePolicy_t;
typedef DevicePolicy_t* DevicePolicyPtr_t;

typedef struct Port Port_t;

/**
 * Configuration structure for port. Port looks at the structure to decide
 * policy settings. This is shared variables by both Port and DPM.
 */
typedef struct
{
    USBTypeCPort PortType;                  /* Snk/Src/DRP */
    FSC_BOOL     SrcPreferred;              /* Source preferred (DRP) */
    FSC_BOOL     SnkPreferred;              /* Sink preferred (DRP) */
    FSC_BOOL     SinkGotoMinCompatible;     /* Sink GotoMin supported. */
    FSC_BOOL     SinkUSBSuspendOperation;   /* USB suspend capable */
    FSC_BOOL     SinkUSBCommCapable;        /* USB communications capable */
    FSC_U32      SinkRequestMaxVoltage;     /* Sink Maximum voltage */
    FSC_U32      SinkRequestMaxPower;       /* Sink Maximum power */
    FSC_U32      SinkRequestOpPower;        /* Sink Operating power */
    FSC_BOOL     audioAccSupport;           /* Audio Acc support */
    FSC_BOOL     poweredAccSupport;         /* Powered Acc support */
    FSC_U8       RpVal;                     /* Pull up value to use */
} PortConfig_t;

/**
 * @brief Initializes the DPM object pointer
 * @param[in] DPM pointer type object
 */
void DPM_Init(DevicePolicyPtr_t *dpm);

/**
 * @brief Adds port to the list of ports managed by dpm
 * @param[in] dpm object to which the port is added
 * @param[in] port object which is added to DPM list
 */
void DPM_AddPort(DevicePolicyPtr_t dpm, Port_t *port);

/**
 * @brief Get source cap header for the port object.
 * @param[in] dpm pointer to device policy object
 * @param[in] port requesting source cap header
 */
sopMainHeader_t* DPM_GetSourceCapHeader(DevicePolicyPtr_t dpm, Port_t *port);

/**
 * @brief Get sink cap header for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting sink cap header
 */
sopMainHeader_t* DPM_GetSinkCapHeader(DevicePolicyPtr_t dpm, Port_t *port);

/**
 * @brief Get the source cap for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting source caps
 */
doDataObject_t* DPM_GetSourceCap(DevicePolicyPtr_t dpm, Port_t *port);

/**
 * @brief Get sink cap for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting sink cap
 */
doDataObject_t* DPM_GetSinkCap(DevicePolicyPtr_t dpm, Port_t *port);

/**
 * @brief Called by the usb PD/TypeC core to ask device policy to transition
 * to the capability advertised specified by port and index.
 * @param[in] dpm pointer to the device policy object
 * @param[in] port advertising the source capability
 * @param[in] index of the source capability object
 */
FSC_BOOL DPM_TransitionSource(DevicePolicyPtr_t dpm, Port_t *port, int index);

/**
 * @brief Called by usb PD/TypeC core to ask device policy if the source
 * is ready after the transition. It returns true if the source transition has
 * completed and successful.
 * @param[in] dpm pointer to the device policy object
 * @param[in] port advertising the source capability
 * @param[in] index of the source capability object
 */
FSC_BOOL DPM_IsSourceCapEnabled(DevicePolicyPtr_t dpm, Port_t *port, int index);

#endif /* MODULES_DPM_H_ */
