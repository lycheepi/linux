/**
 *******************************************************************************
 * @file        vendor_macros.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Macros to generate PDOs and other vendor defined values
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

#ifndef VENDOR_MACROS_H
#define VENDOR_MACROS_H

#include "PD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PORT_SUPPLY_FIRST_FIXED(idx)\
   {.FPDOSupply = {\
                      .MaxCurrent        = Src_PDO_Max_Current##idx,\
                      .Voltage           = Src_PDO_Voltage##idx,\
                      .PeakCurrent       = Src_PDO_Peak_Current##idx,\
                      .DataRoleSwap      = DR_Swap_To_UFP_Supported,\
                      .USBCommCapable    = USB_Comms_Capable,\
                      .ExternallyPowered = Unconstrained_Power,\
                      .USBSuspendSupport = SWAP(USB_Suspend_May_Be_Cleared),\
                      .DualRolePower     = Accepts_PR_Swap_As_Src,\
                      .SupplyType        = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_0(idx)\
   {.FPDOSupply = {\
                      .MaxCurrent        = Src_PDO_Max_Current##idx,\
                      .Voltage           = Src_PDO_Voltage##idx,\
                      .PeakCurrent       = Src_PDO_Peak_Current##idx,\
                      .SupplyType        = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_1(idx)\
    { .BPDO = {\
                 .MaxPower   = Src_PDO_Max_Power##idx,\
                 .MinVoltage = Src_PDO_Min_Voltage##idx,\
                 .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                 .SupplyType = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_2(idx)\
    { .VPDO = {\
                  .MaxCurrent = Src_PDO_Max_Current##idx,\
                  .MinVoltage = Src_PDO_Min_Voltage##idx,\
                  .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                  .SupplyType = Src_PDO_Supply_Type##idx,\
    }}

#define PORT_SUPPLY_TYPE_3(idx)\
    { .PPSAPDO = {\
                     .MaxCurrent = Src_PDO_Max_Current##idx,\
                     .MinVoltage = Src_PDO_Min_Voltage##idx,\
                     .MaxVoltage = Src_PDO_Max_Voltage##idx,\
                     .SupplyType = Src_PDO_Supply_Type##idx,\
     }}

#define PORT_SUPPLY_TYPE_(idx, type)      PORT_SUPPLY_TYPE_##type(idx)
#define CREATE_SUPPLY_PDO(idx, type)      PORT_SUPPLY_TYPE_(idx, type)
#define CREATE_SUPPLY_PDO_FIRST(idx)      PORT_SUPPLY_FIRST_FIXED(idx)

#define PORT_SINK_TYPE_0(idx)\
    { .FPDOSink = {\
                       .OperationalCurrent = Snk_PDO_Op_Current##idx,\
                       .Voltage            = Snk_PDO_Voltage##idx,\
                       .DataRoleSwap       = DR_Swap_To_DFP_Supported,\
                       .USBCommCapable     = USB_Comms_Capable,\
                       .ExternallyPowered  = Unconstrained_Power,\
                       .HigherCapability   = Higher_Capability_Set,\
                       .DualRolePower      = Accepts_PR_Swap_As_Snk,\
                       .SupplyType         = pdoTypeFixed,\
    }}

#define PORT_SINK_TYPE_1(idx)\
    { .BPDO = {\
                 .MaxPower   = Snk_PDO_Op_Power##idx,\
                 .MinVoltage = Snk_PDO_Min_Voltage##idx,\
                 .MaxVoltage = Snk_PDO_Max_Voltage##idx,\
                 .SupplyType = Snk_PDO_Supply_Type3##idx,\
    }}

#define PORT_SINK_TYPE_2(idx)\
    { .VPDO = {\
                  .MaxCurrent = Snk_PDO_Op_Current##idx,\
                  .MinVoltage = Snk_PDO_Min_Voltage##idx,\
                  .MaxVoltage = Snk_PDO_Max_Voltage##idx,\
                  .SupplyType = Snk_PDO_Supply_Type##idx,\
    }}

#define PORT_SINK_TYPE_3(idx)\
    { .PPSAPDO = {\
                     .MaxCurrent          = Snk_PDO_Op_Current##idx,\
                     .MinVoltage          = Snk_PDO_Min_Voltage##idx,\
                     .MaxVoltage          = Snk_PDO_Max_Voltage##idx,\
                     .SupplyType          = Snk_PDO_Supply_Type##idx,\
     }}

#define PORT_SINK_TYPE_(idx, type)      PORT_SINK_TYPE_##type(idx)
#define CREATE_SINK_PDO(idx, type)      PORT_SINK_TYPE_(idx, type)

/******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************/
extern FSC_U8 gCountry_codes[];

void VIF_InitializeSrcCaps(doDataObject_t *src_caps);
void VIF_InitializeSnkCaps(doDataObject_t *snk_caps);

/** Helpers **/   
#define YES 1
#define NO 0
#define SWAP(X) ((X) ? 0 : 1)

#ifdef __cplusplus
}
#endif

#endif /* VENDOR_MACROS_H */

