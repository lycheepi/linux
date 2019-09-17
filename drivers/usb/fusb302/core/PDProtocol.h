/**
 *******************************************************************************
 * @file        PDProtocol.h
 * @author      ON Semiconductor USB-PD Firmware Team
 * @brief       Defines PD protocol handling functionality.
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

#ifndef _PDPROTOCOL_H_
#define	_PDPROTOCOL_H_

/////////////////////////////////////////////////////////////////////////////
//                              Required headers
/////////////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "Port.h"

#ifdef FSC_DEBUG
#include "Log.h"
#endif /* FSC_DEBUG */

/* USB PD Protocol Layer Routines */
void USBPDProtocol(Port_t *port);
void ProtocolIdle(Port_t *port);
void ProtocolResetWait(Port_t *port);
void ProtocolRxWait(void);
void ProtocolGetRxPacket(Port_t *port, FSC_BOOL HeaderReceived);
void ProtocolTransmitMessage(Port_t *port);
void ProtocolSendingMessage(Port_t *port);
void ProtocolWaitForPHYResponse(void);
void ProtocolVerifyGoodCRC(Port_t *port);
void ProtocolSendGoodCRC(Port_t *port, SopType sop);
void ProtocolLoadSOP(Port_t *port, SopType sop);
void ProtocolLoadEOP(Port_t *port);
void ProtocolSendHardReset(Port_t *port);
void ProtocolFlushRxFIFO(Port_t *port);
void ProtocolFlushTxFIFO(Port_t *port);
void ResetProtocolLayer(Port_t *port, FSC_BOOL ResetPDLogic);
void manualRetries(Port_t *port);

#ifdef FSC_DEBUG
/* Logging and debug functions */
FSC_BOOL StoreUSBPDToken(Port_t *port, FSC_BOOL transmitter,
                         USBPD_BufferTokens_t token);
FSC_BOOL StoreUSBPDMessage(Port_t *port, sopMainHeader_t Header,
                           doDataObject_t* DataObject,
                           FSC_BOOL transmitter, FSC_U8 SOPType);
FSC_U8 GetNextUSBPDMessageSize(Port_t *port);
FSC_U8 GetUSBPDBufferNumBytes(Port_t *port);
FSC_BOOL ClaimBufferSpace(Port_t *port, FSC_S32 intReqSize);
FSC_U8 ReadUSBPDBuffer(Port_t *port, FSC_U8* pData, FSC_U8 bytesAvail);

void SendUSBPDHardReset(Port_t *port);
void setManualRetries(Port_t *port, FSC_U8 mode);
FSC_U8 getManualRetries(Port_t *port);

#endif /* FSC_DEBUG */

#endif	/* _PDPROTOCOL_H_ */

