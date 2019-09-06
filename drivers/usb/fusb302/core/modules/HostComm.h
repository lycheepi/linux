/***************************************//*************************************
 * FileName:        HostComm.h
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
#ifndef _HOSTCOMM_H
#define _HOSTCOMM_H

#include "platform.h"
#include "hcmd.h"
#include "Port.h"

#ifdef FSC_DEBUG
/**
 * @brief Intialize the host com
 * @param[in] port starting address of the port structures
 * @param[in] num number of ports defined
 */
void HCom_Init(Port_t *port, int num);

/**
 * @brief Function to process the hostCom messages. Called when the input buffer
 * has a valid message.
 */
void HCom_Process(void);

/**
 * @brief Returns pointer to input buffer of length HCOM_MSG_LENGTH
 */
FSC_U8* HCom_InBuf(void);

/**
 * @brief Returns pointer to output buffer of length HCOM_MSG_LENGTH
 */
FSC_U8* HCom_OutBuf(void);

#endif /* FSC_DEBUG */

#endif	/* HOSTCOMM_H */
