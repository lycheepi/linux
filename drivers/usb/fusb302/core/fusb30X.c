/**
 *******************************************************************************
 * @file        fusb30X.c
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       This file provides register map definitions and
 *              associated values.
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

#include "fusb30X.h"
#include "platform.h"

FSC_BOOL DeviceWrite(FSC_U8 i2cAddr, FSC_U8 regAddr,
                     FSC_U8 length, FSC_U8* data)
{
    return platform_i2c_write(i2cAddr, FUSB300AddrLength, length,
                              length, FUSB300IncSize, regAddr, data);
}

FSC_BOOL DeviceRead(FSC_U8 i2cAddr, FSC_U8 regAddr,
                    FSC_U8 length, FSC_U8* data)
{
    return platform_i2c_read(i2cAddr, FUSB300AddrLength, length,
                             length, FUSB300IncSize, regAddr, data);
}
