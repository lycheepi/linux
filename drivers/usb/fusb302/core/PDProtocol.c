/**
 *******************************************************************************
 * @file        PDProtocol.c
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

#include "platform.h"
#include "fusb30X.h"
#include "TypeC.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "PD_Types.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"
#endif /* FSC_HAVE_VDM */

#define NUM_RETRIES 3;  /* Number of tries (1 + 3 retries) */

/* USB PD Protocol Layer Routines */
void USBPDProtocol(Port_t *port)
{
    if (port->Registers.Status.I_HARDRST ||
        port->Registers.Status.I_HARDSENT)
    {
        ResetProtocolLayer(port, TRUE);
        if (port->PolicyIsSource)
        {
            TimerStart(&port->PolicyStateTimer, tPSHardReset);
            SetPEState(port, peSourceTransitionDefault);
        }
        else
        {
          SetPEState(port, peSinkTransitionDefault);
        }
        
#ifdef FSC_DEBUG
        StoreUSBPDToken(port, FALSE, pdtHardReset);
#endif /* FSC_DEBUG */
    }
    else
    {
        switch (port->ProtocolState)
        {
        case PRLReset:
            /* Sending a hard reset. */
            ProtocolSendHardReset(port);
            port->PDTxStatus = txWait;
            port->ProtocolState = PRLResetWait;
            break;
        case PRLResetWait:
            /* Wait on hard reset signaling */
            ProtocolResetWait(port);
            break;
        case PRLIdle:
            /* Wait on Tx/Rx */
            ProtocolIdle(port);
            break;
        case PRLTxSendingMessage:
            /* Wait on Tx to finish */
            ProtocolSendingMessage(port);
            break;
        case PRLTxVerifyGoodCRC:
            /* Verify returned GoodCRC */
            ProtocolVerifyGoodCRC(port);
            break;
        case PRLManualRetries:
            /* Attempt Tx retries if needed */
            manualRetries(port);
            break;
        case PRLDisabled:
            break;
        default:
            break;
        }
    }
}

void ProtocolIdle(Port_t *port)
{
    if (port->PDTxStatus == txReset)
    {
        /* Need to send a reset? */
        port->ProtocolState = PRLReset;
    }
#ifdef FSC_GSCE_FIX
    else if (port->Registers.Status.I_CRC_CHK)
    {
        /* Using the CRC CHK interrupt as part of the GSCE workaround. */
        if (port->DoTxFlush)
        {
            ProtocolFlushTxFIFO(port);
            port->DoTxFlush = FALSE;
        }
        ProtocolGetRxPacket(port, FALSE);
        port->PDTxStatus = txIdle;
        port->Registers.Status.I_CRC_CHK = 0;
    }
#else
    else if (port->Registers.Status.I_GCRCSENT)
    {
        /* Received a message and sent a GoodCRC in response? */
        if (port->DoTxFlush)
        {
            ProtocolFlushTxFIFO(port);
            port->DoTxFlush = FALSE;
        }
        ProtocolGetRxPacket(port, FALSE);
        port->PDTxStatus = txIdle;
        port->Registers.Status.I_GCRCSENT = 0;
    }
#endif /* FSC_GSCE_FIX */
    else if (port->PDTxStatus == txSend)
    {
        /* Have a message to send? */
        ProtocolTransmitMessage(port);
    }
#ifdef FSC_HAVE_EXT_MSG
    else if (port->ExtTxOrRx != NoXfer && port->ExtWaitTxRx == FALSE)
    {
        port->PDTxStatus = txSend;
    }
#endif /* FSC_HAVE_EXT_MSG */
    else
    {
#ifndef FSC_GSCE_FIX /* Optional - may interfere with GSCE workaround. */
        /* A quickly sent second message can be received
         * into the buffer without triggering an (additional) interrupt.
         */
        DeviceRead(port->I2cAddr, regStatus0, 2,
            &port->Registers.Status.byte[4]);

        if (!port->Registers.Status.ACTIVITY &&
            !port->Registers.Status.RX_EMPTY)
        {
            ProtocolGetRxPacket(port, FALSE);
            port->PDTxStatus = txIdle;
        }
#endif /* !FSC_GSCE_FIX */
    }
}

void ProtocolResetWait(Port_t *port)
{
    if (port->Registers.Status.I_HARDSENT)
    {
        port->ProtocolState = PRLIdle;
        port->PDTxStatus = txSuccess;
    }
}

void ProtocolGetRxPacket(Port_t *port, FSC_BOOL HeaderReceived)
{
#ifdef FSC_DEBUG
    sopMainHeader_t logHeader;
#endif /* FSC_DEBUG */
    FSC_U32 i = 0, j = 0;
    FSC_U8 data[3];

    if (port->Registers.Status.RX_EMPTY == 1)
    {
        /* Nothing to see here... */
        return;
    }

    if (HeaderReceived == FALSE)
    {
        /* Read the Rx token and two header bytes */
        DeviceRead(port->I2cAddr, regFIFO, 3, &data[0]);
        port->PolicyRxHeader.byte[0] = data[1];
        port->PolicyRxHeader.byte[1] = data[2];

        /* Figure out what SOP* the data came in on */
        port->ProtocolMsgRxSop = TokenToSopType(data[0]);
    }
    else
    {
        /* PolicyRxHeader, ProtocolMsgRxSop already set */
    }

#ifdef FSC_GSCE_FIX
    /* NOTE - This is the GSCE bug workaround! */
    if (port->PolicyRxHeader.NumDataObjects == 0 &&
        port->PolicyRxHeader.MessageType == CMTGetSourceCapExt)
    {
        if (!port->NextGSCE)
        {
            port->NextGSCE = TRUE;

            /* Read remainder of CRC bytes */
            DeviceRead(port->I2cAddr, regFIFO, 4, &port->ProtocolCRC[0]);

            /* Pre-load manual GoodCRC */
            port->PolicyTxHeader.word = 0;
            port->PolicyTxHeader.MessageType = CMTGoodCRC;
            port->PolicyTxHeader.PortDataRole = port->PolicyIsDFP;
            port->PolicyTxHeader.PortPowerRole = port->PolicyIsSource;
            port->PolicyTxHeader.SpecRevision = USBPDSPECREV2p0;
            port->PolicyTxHeader.MessageID = port->PolicyRxHeader.MessageID;
            ProtocolSendGoodCRC(port, SOP_TYPE_SOP);
            return;
        }
        else
        {
            port->NextGSCE = FALSE;
        }
    }
    else
    {
        /* For normal usage, wait on the GCRCSENT interrupt */
        /* I_GCRCSENT can be missed (rare), so the 15-count should be long
         * enough to get passed the GoodCRC transmission.
         */
        while (!port->Registers.Status.I_GCRCSENT && ++i < 15)
        {
            DeviceRead(port->I2cAddr, regInterruptb, 1,
                       &port->Registers.Status.byte[3]);
        }
        port->Registers.Status.I_GCRCSENT = 0;
    }
#endif /* FSC_GSCE_FIX */

#ifdef FSC_DEBUG
    /* Log the auto goodcrc */
    logHeader.word = 0;
    logHeader.NumDataObjects = 0;
    logHeader.MessageType = CMTGoodCRC;
    logHeader.PortDataRole = port->PolicyIsDFP;
    logHeader.PortPowerRole = port->PolicyIsSource;
    logHeader.SpecRevision = port->PdRevContract;
    logHeader.MessageID = port->PolicyRxHeader.MessageID;
#endif /* FSC_DEBUG */

    if (port->ProtocolMsgRxSop == SOP_TYPE_ERROR)
    {
        /* SOP result can be corrupted in rare cases, or possibly if
         * the FIFO reads get out of sync.
         * TODO - Flush?
         */
        return;
    }

    if ((port->PolicyRxHeader.NumDataObjects == 0) &&
        (port->PolicyRxHeader.MessageType == CMTSoftReset))
    {
        /* For a soft reset, reset ID's, etc. */
        port->MessageIDCounter[port->ProtocolMsgRxSop] = 0;
        port->MessageID[port->ProtocolMsgRxSop] = 0xFF;
        port->ProtocolMsgRx = TRUE;
#ifdef FSC_DEBUG
        port->SourceCapsUpdated = TRUE;
#endif /* FSC_DEBUG */
    }
    else if (port->PolicyRxHeader.MessageID !=
             port->MessageID[port->ProtocolMsgRxSop])
    {
        port->MessageID[port->ProtocolMsgRxSop] =
            port->PolicyRxHeader.MessageID;
        port->ProtocolMsgRx = TRUE;
    }

    if ((port->PolicyRxHeader.NumDataObjects == 0) &&
        (port->PolicyRxHeader.MessageType == CMTGoodCRC))
    {
        /* Rare cases may result in the next GoodCRC being processed before
         * the expected current message.  Handle and continue on to next msg.
         */

        /* Read out the 4 CRC bytes to move the address to the next packet */
        DeviceRead(port->I2cAddr, regFIFO, 4, &port->ProtocolCRC[0]);

        port->ProtocolState = PRLIdle;
        port->PDTxStatus = txSuccess;

        ProtocolGetRxPacket(port, FALSE);

        return;
    }
    else if (port->PolicyRxHeader.NumDataObjects > 0)
    {
        /* Data message - Grab the data from the FIFO, including 4 byte CRC */
        DeviceRead(port->I2cAddr, regFIFO,
                   ((port->PolicyRxHeader.NumDataObjects << 2) + 4),
                   &port->ProtocolRxBuffer[0]);

#ifdef FSC_HAVE_EXT_MSG
        if (port->PolicyRxHeader.Extended == 1)
        {
            /* Copy ext header first */
            port->ExtRxHeader.byte[0] = port->ProtocolRxBuffer[0];
            port->ExtRxHeader.byte[1] = port->ProtocolRxBuffer[1];

            if (port->ExtRxHeader.ReqChunk == 1)
            {
                /* Received request for another chunk. Continue sending....*/
                port->ExtWaitTxRx = FALSE;
                if (port->ExtRxHeader.ChunkNum < port->ExtChunkNum)
                {
                    /* Resend the previous chunk */
                    port->ExtChunkNum = port->ExtRxHeader.ChunkNum;
                    port->ExtChunkOffset =
                        port->ExtChunkNum * EXT_MAX_MSG_LEGACY_LEN;
                }
            }
            else
            {
                if (port->ExtRxHeader.DataSize > EXT_MAX_MSG_LEGACY_LEN)
                {
                    if (port->ExtRxHeader.DataSize > 260)
                    {
                        port->ExtRxHeader.DataSize = 260;
                    }
                    if (port->ExtRxHeader.ChunkNum == 0)
                    {
                        port->ExtChunkOffset = 0;   /* First message */
                        port->ExtChunkNum = 1;      /* Next chunk number */
                    }
                }
            }
        }
#endif /* FSC_HAVE_EXT_MSG */
        for (i = 0; i < port->PolicyRxHeader.NumDataObjects; i++)
        {
            for (j = 0; j < 4; j++)
            {
#ifdef FSC_HAVE_EXT_MSG
                if (port->PolicyRxHeader.Extended == 1)
                {
                    /* Skip ext header */
                    if (i == 0 && (j == 0 || j == 1)){continue;}
                    
                    if (port->ExtRxHeader.ReqChunk == 0)
                    {
                        port->ExtMsgBuffer[port->ExtChunkOffset++] =
                                port->ProtocolRxBuffer[j + (i << 2)];
                    }
                }
                else
#endif /* FSC_HAVE_EXT_MSG */
                {
                    port->PolicyRxDataObj[i].byte[j] =
                            port->ProtocolRxBuffer[j + (i << 2)];
                }
            }
       }

#ifdef FSC_HAVE_EXT_MSG
        if (port->PolicyRxHeader.Extended == 1)
        {
            if (port->ExtRxHeader.ReqChunk == 0)
            {
                if (port->ExtChunkOffset < port->ExtRxHeader.DataSize)
                {
                    /* more message left. continue receiving */
                    port->ExtTxOrRx = RXing;
                    port->ProtocolMsgRx = FALSE;
                    port->ExtWaitTxRx = FALSE;
                }
                else
                {
                    port->ExtTxOrRx = NoXfer;
                    port->ProtocolMsgRx = TRUE;
                }
            }
            else if (port->ExtRxHeader.ReqChunk == 1 &&
                     port->ExtChunkOffset < port->ExtTxHeader.DataSize)
            {
                port->ExtWaitTxRx = FALSE;
            }
            else
            {
                /* Last message received */
                port->ExtTxOrRx = NoXfer;
            }
        }
#endif /* FSC_HAVE_EXT_MSG */
    }
    else
    {
        /* Command message */
        /* Read out the 4 CRC bytes to move the address to the next packet */
        DeviceRead(port->I2cAddr, regFIFO, 4, &port->ProtocolCRC[0]);
    }

#ifdef FSC_DEBUG
    /* Store the received PD message for the device policy manager */
    StoreUSBPDMessage(port, port->PolicyRxHeader,
                      (doDataObject_t*)port->ProtocolRxBuffer, FALSE, port->ProtocolMsgRxSop);
    
    /* Store the GoodCRC message that we have sent (SOP) */
    StoreUSBPDMessage(port, logHeader, &port->PolicyTxDataObj[0], TRUE,
                      port->ProtocolMsgRxSop);

    /*
     * Special debug case where PD state log will provide the time elapsed
     * in this function, and the number of I2C bytes read during this period.
     */
    WriteStateLog(&port->PDStateLog, dbgGetRxPacket, platform_get_log_time());
#endif /* FSC_DEBUG */

    /* Special VDM use case where a second message appears too quickly */
    if ((port->PolicyRxHeader.NumDataObjects != 0) &&
        (port->PolicyRxHeader.MessageType == DMTVenderDefined) &&
        (port->PolicyRxDataObj[0].SVDM.CommandType == 0)) /* Initiator */
    {
        /* Delay and check if a new mesage has been received */
        platform_delay_10us(100); /* 1ms */

        DeviceRead(port->I2cAddr, regInterruptb, 3,
                   &port->Registers.Status.byte[3]);

        if (port->Registers.Status.I_GCRCSENT &&
            !port->Registers.Status.RX_EMPTY)
        {
            /* Get the next message - overwriting the current message */
            ProtocolGetRxPacket(port, FALSE);
        }
    }
}

void ProtocolTransmitMessage(Port_t *port)
{
    FSC_U32 i, j;
    sopMainHeader_t temp_PolicyTxHeader = { 0 };

    port->ManualRetries = NUM_RETRIES;
    port->DoManualRetry = FALSE;

    port->DoTxFlush = FALSE;

    /* Note: Power needs to be set a bit before we write TX_START to update */
    ProtocolLoadSOP(port, port->ProtocolMsgTxSop);

#ifdef FSC_HAVE_EXT_MSG
    if (port->ExtTxOrRx == RXing)
    {
        /* Set up chunk request */
        temp_PolicyTxHeader.word = port->PolicyRxHeader.word;
        temp_PolicyTxHeader.PortPowerRole = port->PolicyIsSource;
        temp_PolicyTxHeader.PortDataRole = port->PolicyIsDFP;
    }
    else
#endif /* FSC_HAVE_EXT_MSG */
    {
        temp_PolicyTxHeader.word = port->PolicyTxHeader.word;
    }

    if ((temp_PolicyTxHeader.NumDataObjects == 0) &&
        (temp_PolicyTxHeader.MessageType == CMTSoftReset))
    {
        port->MessageIDCounter[port->ProtocolMsgTxSop] = 0;
        port->MessageID[port->ProtocolMsgTxSop] = 0xFF;
#ifdef FSC_DEBUG
        port->SourceCapsUpdated = TRUE;
#endif /* FSC_DEBUG */
    }

#ifdef FSC_HAVE_EXT_MSG
    if (temp_PolicyTxHeader.Extended == 1)
    {
        if (port->ExtTxOrRx == TXing)
        {
            /* Remaining bytes */
            i = port->ExtTxHeader.DataSize - port->ExtChunkOffset;

            if (i > EXT_MAX_MSG_LEGACY_LEN)
            {
                temp_PolicyTxHeader.NumDataObjects = 7;
            }
            else
            {
                /* Round up to 4 byte boundary.
                 * Two extra byte is for the extended header.
                 */
                temp_PolicyTxHeader.NumDataObjects = (i + 4) / 4;
            }
            port->PolicyTxHeader.NumDataObjects =
                    temp_PolicyTxHeader.NumDataObjects;
            port->ExtTxHeader.ChunkNum = port->ExtChunkNum;
        }
        else if (port->ExtTxOrRx == RXing)
        {
            temp_PolicyTxHeader.NumDataObjects = 1;
        }
        port->ExtWaitTxRx = TRUE;
    }
#endif /* FSC_HAVE_EXT_MSG */

    temp_PolicyTxHeader.MessageID =
            port->MessageIDCounter[port->ProtocolMsgTxSop];
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
            PACKSYM | (2 + (temp_PolicyTxHeader.NumDataObjects << 2));
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
            temp_PolicyTxHeader.byte[0];
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
            temp_PolicyTxHeader.byte[1];

    /* If this is a data object... */
    if (temp_PolicyTxHeader.NumDataObjects > 0)
    {
#ifdef FSC_HAVE_EXT_MSG
        if (temp_PolicyTxHeader.Extended == 1)
        {
            if (port->ExtTxOrRx == RXing)
            {
                port->ExtTxHeader.ChunkNum = port->ExtChunkNum;
                port->ExtTxHeader.DataSize = 0;
                port->ExtTxHeader.Chunked = 1;
                port->ExtTxHeader.ReqChunk = 1;
            }
            else if (port->ExtTxOrRx == TXing)
            {
                port->ExtTxHeader.ChunkNum = port->ExtChunkNum;
            }

            /* Copy the two byte extended header. */
            port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
                    port->ExtTxHeader.byte[0];
            port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
                    port->ExtTxHeader.byte[1];
        }
#endif /* FSC_HAVE_EXT_MSG */
        for (i = 0; i < temp_PolicyTxHeader.NumDataObjects; i++)
        {
            for (j = 0; j < 4; j++)
            {
#ifdef FSC_HAVE_EXT_MSG
                if (temp_PolicyTxHeader.Extended == 1)
                {
                    /* Skip extended header */
                    if (i == 0 && (j == 0 || j == 1)) { continue; }

                    if (port->ExtChunkOffset < port->ExtTxHeader.DataSize)
                    {
                        port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
                                port->ExtMsgBuffer[port->ExtChunkOffset++];
                    }
                    else
                    {
                        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = 0;
                    }
                }
                else
#endif /* FSC_HAVE_EXT_MSG */
                {
                    /* Load the actual bytes */
                    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
                            port->PolicyTxDataObj[i].byte[j];
                }
            }
        }
    }

    /* Load the CRC, EOP and stop sequence */
    ProtocolLoadEOP(port);

    /* sometimes it's important to check for a rec'd message before sending */
    if (port->ProtocolCheckRxBeforeTx)
    {
        /* self-clear - one-time deal */
        port->ProtocolCheckRxBeforeTx = FALSE;
        DeviceRead(port->I2cAddr, regInterruptb,1,
            &port->Registers.Status.byte[3]);
        if (port->Registers.Status.I_GCRCSENT)
        {
            /* if a message was received, bail */
            port->Registers.Status.I_GCRCSENT = 0;
            port->PDTxStatus = txError;
#ifdef FSC_HAVE_EXT_MSG
            port->ExtTxOrRx = NoXfer;
#endif /* FSC_HAVE_EXT_MSG */
            ProtocolGetRxPacket(port, FALSE);
            return;
        }
    }

    /* Commit the FIFO to the device */
    if (DeviceWrite(port->I2cAddr, regFIFO, port->ProtocolTxBytes,
                    &port->ProtocolTxBuffer[0]) == FALSE)
    {
        /* If a FIFO write happens while a GoodCRC is being transmitted,
         * the transaction will NAK and will need to be discarded.
         */
        port->DoTxFlush = TRUE;
        port->PDTxStatus = txError;
        return;
    }

    /* Start the transmission */
    port->Registers.Control.TX_START = 1;
    DeviceWrite(port->I2cAddr, regControl0, 1,&port->Registers.Control.byte[0]);
    port->Registers.Control.TX_START = 0;

    /* 3ms */
    TimerStart(&port->ProtocolTimer, tRetry);

    /* Set the transmitter status to busy */
    port->PDTxStatus = txBusy;

    if (port->ManualRetriesEnabled)
    {
        port->ProtocolState = PRLManualRetries;
    }
    else
    {
        port->ProtocolState = PRLTxSendingMessage;
    }

#ifdef FSC_DEBUG
    /* Store all messages that we attempt to send for debugging (SOP) */
    StoreUSBPDMessage(port, temp_PolicyTxHeader,
                      (doDataObject_t*)&port->ProtocolTxBuffer[7],
                      TRUE, port->ProtocolMsgTxSop);
    WriteStateLog(&port->PDStateLog, dbgSendTxPacket, platform_get_log_time());
#endif /* FSC_DEBUG */
}

void ProtocolSendingMessage(Port_t *port)
{
    /* Waiting on result/status of transmission */
    if (port->Registers.Status.I_TXSENT)
    {
        port->Registers.Status.I_TXSENT = 0;
        ProtocolVerifyGoodCRC(port);
    }
    else if (port->Registers.Status.I_COLLISION)
    {
        port->Registers.Status.I_COLLISION = 0;
        port->PDTxStatus = txCollision;
        port->ProtocolState = PRLIdle;
    }
    else if (port->Registers.Status.I_RETRYFAIL)
    {
        port->Registers.Status.I_RETRYFAIL = 0;
        port->PDTxStatus = txError;
        port->ProtocolState = PRLIdle;
    }
    else if (port->Registers.Status.I_GCRCSENT)
    {
        /* Interruption */
        port->PDTxStatus = txError;
        ProtocolGetRxPacket(port, FALSE);
        port->ProtocolState = PRLIdle;
    }
    else if (port->Registers.Status.I_CRC_CHK)
    {
        /* Rare case - TXSENT interrupt skipped, may see I_CRC_CHK instead. */
        port->Registers.Status.I_CRC_CHK = 0;
        ProtocolVerifyGoodCRC(port);
    }
}

void ProtocolVerifyGoodCRC(Port_t *port)
{
    FSC_U8 data[3];

    /* Read the Rx token and two header bytes */
    DeviceRead(port->I2cAddr, regFIFO, 3, &data[0]);
    port->PolicyRxHeader.byte[0] = data[1];
    port->PolicyRxHeader.byte[1] = data[2];

    /* Figure out what SOP* the data came in on */
    port->ProtocolMsgRxSop = TokenToSopType(data[0]);

    if ((port->PolicyRxHeader.NumDataObjects == 0) &&
        (port->PolicyRxHeader.MessageType == CMTGoodCRC))
    {
        FSC_U8 MIDcompare;
        if (port->ProtocolMsgRxSop == SOP_TYPE_ERROR)
            MIDcompare = 0xFF;
        else
            MIDcompare = port->MessageIDCounter[port->ProtocolMsgRxSop];

#ifdef FSC_DEBUG
        /* Store the received PD message for the DPM (GUI) */
        StoreUSBPDMessage(port, port->PolicyRxHeader, &port->PolicyRxDataObj[0],
                          FALSE, port->ProtocolMsgRxSop);
#endif /* FSC_DEBUG */

        if (port->PolicyRxHeader.MessageID != MIDcompare)
        {
            /* Read out the 4 CRC bytes to move the addr to the next packet */
            DeviceRead(port->I2cAddr, regFIFO, 4, &port->ProtocolCRC[0]);
#ifdef FSC_DEBUG
            /* Store that there was a bad message ID received in the buffer */
            StoreUSBPDToken(port, FALSE, pdtBadMessageID);
#endif /* FSC_DEBUG */
            port->PDTxStatus = txError;
            port->ProtocolState = PRLIdle;
        }
        else
        {
            if (port->ProtocolMsgRxSop != SOP_TYPE_ERROR)
            {
                /* Increment and roll over */
                port->MessageIDCounter[port->ProtocolMsgRxSop]++;
                port->MessageIDCounter[port->ProtocolMsgRxSop] &= 0x07;
#ifdef FSC_HAVE_EXT_MSG
                if (port->ExtTxOrRx != NoXfer)
                {
                    if (port->ExtChunkOffset >= port->ExtTxHeader.DataSize)
                    {
                        /* All data has been sent */
                        port->ExtTxOrRx = NoXfer;
                    }
                    port->ExtChunkNum++;
                }
#endif /* FSC_HAVE_EXT_MSG */
            }
            port->ProtocolState = PRLIdle;
            port->PDTxStatus = txSuccess;
            
            /* Read out the 4 CRC bytes to move the addr to the next packet */
            DeviceRead(port->I2cAddr, regFIFO, 4, &port->ProtocolCRC[0]);
        }
    }
    else
    {
        port->ProtocolState = PRLIdle;
        port->PDTxStatus = txError;

        /* Rare case, next received message preempts GoodCRC */
        ProtocolGetRxPacket(port, TRUE);
    }
}

SopType DecodeSopFromPdMsg(FSC_U8 msg0, FSC_U8 msg1)
{
    /* this SOP* decoding is based on FUSB302 GUI:
     * SOP   => 0b 0xx1 xxxx xxx0 xxxx
     * SOP'  => 0b 1xx1 xxxx xxx0 xxxx
     * SOP'' => 0b 1xx1 xxxx xxx1 xxxx
     */
    if (((msg1 & 0x90) == 0x10) && ((msg0 & 0x10) == 0x00))
        return SOP_TYPE_SOP;
    else if (((msg1 & 0x90) == 0x90) && ((msg0 & 0x10) == 0x00))
        return SOP_TYPE_SOP1;
    else if (((msg1 & 0x90) == 0x90) && ((msg0 & 0x10) == 0x10))
        return SOP_TYPE_SOP2;
    else
        return SOP_TYPE_SOP;
}

void ProtocolSendGoodCRC(Port_t *port, SopType sop)
{
    ProtocolLoadSOP(port, sop);

    port->ProtocolTxBuffer[port->ProtocolTxBytes++] = PACKSYM | 0x02;
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
            port->PolicyTxHeader.byte[0];
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] =
            port->PolicyTxHeader.byte[1];
    ProtocolLoadEOP(port);
    DeviceWrite(port->I2cAddr, regFIFO, port->ProtocolTxBytes,
                &port->ProtocolTxBuffer[0]);
}

void ProtocolLoadSOP(Port_t *port, SopType sop)
{
    /* Clear the Tx byte counter */
    port->ProtocolTxBytes = 0;

    switch (sop)
    {
    case SOP_TYPE_SOP1:
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC3_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC3_TOKEN;
        break;
    case SOP_TYPE_SOP2:
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC3_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC3_TOKEN;
        break;
    case SOP_TYPE_SOP:
    default:
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC1_TOKEN;
        port->ProtocolTxBuffer[port->ProtocolTxBytes++] = SYNC2_TOKEN;
        break;
    }
}

void ProtocolLoadEOP(Port_t *port)
{
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] = JAM_CRC;
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] = EOP;
    port->ProtocolTxBuffer[port->ProtocolTxBytes++] = TXOFF;
}

void ProtocolSendHardReset(Port_t *port)
{
    FSC_U8 data = port->Registers.Control.byte[3] | 0x40;  /* Hard Reset bit */

    /* If the shortcut flag is set, we've already sent the HR command */
    if (port->WaitingOnHR)
    {
        port->WaitingOnHR = FALSE;
    }
    else
    {
        DeviceWrite(port->I2cAddr, regControl3, 1, &data);
    }

#ifdef FSC_DEBUG
    StoreUSBPDToken(port, TRUE, pdtHardReset);
#endif // FSC_DEBUG
}

void ProtocolFlushRxFIFO(Port_t *port)
{
    FSC_U8 data = port->Registers.Control.byte[1] | 0x04;  /* RX_FLUSH bit */
    DeviceWrite(port->I2cAddr, regControl1, 1, &data);
}

void ProtocolFlushTxFIFO(Port_t *port)
{
    FSC_U8 data = port->Registers.Control.byte[0] | 0x40;  /* TX_FLUSH bit */
    DeviceWrite(port->I2cAddr, regControl0, 1, &data);
}

void ResetProtocolLayer(Port_t *port, FSC_BOOL ResetPDLogic)
{
    FSC_U32 i;
    FSC_U8 data = 0x02; /* PD_RESET bit */

    if (ResetPDLogic)
    {
        DeviceWrite(port->I2cAddr, regReset, 1, &data);
    }

    port->ProtocolState = PRLIdle;
    port->PDTxStatus = txIdle;

    port->WaitingOnHR = FALSE;

#ifdef FSC_GSCE_FIX
    port->NextGSCE = FALSE;
#endif /* FSC_GSCE_FIX */

#ifdef FSC_HAVE_VDM
    TimerDisable(&port->VdmTimer);
    port->VdmTimerStarted = FALSE;
#endif /* FSC_HAVE_VDM */

    port->ProtocolTxBytes = 0;

    for (i = 0; i < SOP_TYPE_NUM; i++)
    {
        port->MessageIDCounter[i] = 0;
        port->MessageID[i] = 0xFF;
    }

    port->ProtocolMsgRx = FALSE;
    port->ProtocolMsgRxSop = SOP_TYPE_SOP;
    port->USBPDTxFlag = FALSE;
    port->PolicyHasContract = FALSE;
    port->USBPDContract.object = 0;

#ifdef FSC_DEBUG
    port->SourceCapsUpdated = TRUE;
#endif // FSC_DEBUG

    port->CapsHeaderReceived.word = 0;
    for (i = 0; i < 7; i++)
    {
        port->CapsReceived[i].object = 0;
    }

#ifdef FSC_HAVE_EXT_MSG
    port->ExtWaitTxRx = FALSE;
    port->ExtChunkNum = 0;
    port->ExtTxOrRx = NoXfer;
    port->ExtChunkOffset = 0;
#endif /* FSC_HAVE_EXT_MSG */

}

#ifdef FSC_DEBUG
/* USB PD Debug Buffer Routines */
FSC_BOOL StoreUSBPDToken(Port_t *port, FSC_BOOL transmitter,
                         USBPD_BufferTokens_t token)
{
    FSC_U8 header1 = 1;

    if (ClaimBufferSpace(port, 2) == FALSE)
    {
        return FALSE;
    }

    if (transmitter)
    {
        header1 |= 0x40;
    }

    port->USBPDBuf[port->USBPDBufEnd++] = header1;
    port->USBPDBufEnd %= PDBUFSIZE;
    token &= 0x0F;
    port->USBPDBuf[port->USBPDBufEnd++] = token;
    port->USBPDBufEnd %= PDBUFSIZE;

    return TRUE;
}

FSC_BOOL StoreUSBPDMessage(Port_t *port, sopMainHeader_t Header,
                           doDataObject_t* DataObject,
                           FSC_BOOL transmitter, FSC_U8 SOPToken)
{
    FSC_U32 i, j, required;
    FSC_U8 header1;

    required = Header.NumDataObjects * 4 + 2 + 2;

    if (ClaimBufferSpace(port, required) == FALSE)
    {
        return FALSE;
    }

    header1 = (0x1F & (required - 1)) | 0x80;

    if (transmitter)
    {
        header1 |= 0x40;
    }

    port->USBPDBuf[port->USBPDBufEnd++] = header1;
    port->USBPDBufEnd %= PDBUFSIZE;
    port->USBPDBuf[port->USBPDBufEnd++] = SOPToken;
    port->USBPDBufEnd %= PDBUFSIZE;
    port->USBPDBuf[port->USBPDBufEnd++] = Header.byte[0];
    port->USBPDBufEnd %= PDBUFSIZE;
    port->USBPDBuf[port->USBPDBufEnd++] = Header.byte[1];
    port->USBPDBufEnd %= PDBUFSIZE;

    for (i = 0; i < Header.NumDataObjects; i++)
    {
        for (j = 0; j < 4; j++)
        {
            port->USBPDBuf[port->USBPDBufEnd++] = DataObject[i].byte[j];
            port->USBPDBufEnd %= PDBUFSIZE;
        }
    }

    return TRUE;
}

FSC_U8 GetNextUSBPDMessageSize(Port_t *port)
{
    FSC_U8 numBytes = 0;

    if (port->USBPDBufStart == port->USBPDBufEnd)
    {
       numBytes = 0;
    }
    else
    {
        numBytes = (port->USBPDBuf[port->USBPDBufStart] & 0x1F) + 1;
    }

    return numBytes;
}

FSC_U8 GetUSBPDBufferNumBytes(Port_t *port)
{
    FSC_U8 bytes = 0;

    if (port->USBPDBufStart == port->USBPDBufEnd)
    {
        bytes = 0;
    }
    else if (port->USBPDBufEnd > port->USBPDBufStart)
    {
        bytes = port->USBPDBufEnd - port->USBPDBufStart;
    }
    else
    {
        bytes = port->USBPDBufEnd + (PDBUFSIZE - port->USBPDBufStart);
    }

    return bytes;
}

FSC_BOOL ClaimBufferSpace(Port_t *port, FSC_S32 intReqSize)
{
    FSC_S32 available;
    FSC_U8 numBytes;

    if (intReqSize >= PDBUFSIZE)
    {
        return FALSE;
    }

    if (port->USBPDBufStart == port->USBPDBufEnd)
    {
        available = PDBUFSIZE;
    }
    else if (port->USBPDBufStart > port->USBPDBufEnd)
    {
        available = port->USBPDBufStart - port->USBPDBufEnd;
    }
    else
    {
        available = PDBUFSIZE - (port->USBPDBufEnd - port->USBPDBufStart);
    }

    do
    {
        /* Overwrite entries until space is adequate */
        if (intReqSize >= available)
        {
            port->USBPDBufOverflow = TRUE;
            numBytes = GetNextUSBPDMessageSize(port);
            if (numBytes == 0)
            {
                return FALSE;
            }
            available += numBytes;
            port->USBPDBufStart += numBytes;
            port->USBPDBufStart %= PDBUFSIZE;
        }
        else
        {
            break;
        }
    } while (1);

    return TRUE;
}

/* USB HID Commmunication Routines */
FSC_U8 ReadUSBPDBuffer(Port_t *port, FSC_U8* pData, FSC_U8 bytesAvail)
{
    FSC_U8 i, msgSize, bytesRead;
    bytesRead = 0;
    msgSize = GetNextUSBPDMessageSize(port);

    while (msgSize != 0 && msgSize < bytesAvail)
    {
        for (i = 0; i < msgSize; i++)
        {
            *pData++ = port->USBPDBuf[port->USBPDBufStart++];
            port->USBPDBufStart %= PDBUFSIZE;
        }
        bytesAvail -= msgSize;
        bytesRead += msgSize;
        msgSize = GetNextUSBPDMessageSize(port);
    }

    return bytesRead;
}

void setManualRetries(Port_t *port, FSC_U8 mode)
{
    port->ManualRetriesEnabled = mode;
}

FSC_U8 getManualRetries(Port_t *port)
{
    return port->ManualRetriesEnabled;
}

#endif /* FSC_DEBUG */

void manualRetries(Port_t *port)
{
    if (port->Registers.Status.I_COLLISION)
    {
        port->DoTxFlush = TRUE;
    }

    if (port->Registers.Status.I_RETRYFAIL)
    {
        port->DoManualRetry = TRUE;
    }

    if (port->Registers.Status.I_TXSENT)
    {
        /* Success! */
        port->Registers.Status.I_TXSENT = 0;
        ProtocolVerifyGoodCRC(port);
    }

    if (port->Registers.Status.I_GCRCSENT)
    {
        ProtocolGetRxPacket(port, FALSE);
        port->ProtocolState = PRLIdle;
    }

    /* Also set on collision */
    if (port->DoManualRetry)
    {
        port->Registers.Status.I_RETRYFAIL = 0;

        if (!port->Registers.Status.RX_EMPTY)
        {
            ProtocolFlushRxFIFO(port);
        }
        if (port->Registers.Status.I_GCRCSENT)
        {
            port->PDTxStatus = txError;
        }
        else
        {
            if (port->ManualRetries > 0)
            {
                /* Failure :( */
                port->ProtocolState = PRLIdle;
                port->PDTxStatus = txError;
            }
            else
            {
                if (port->DoTxFlush == FALSE)
                {
                    /* Reload FIFO */
                    DeviceWrite(port->I2cAddr, regFIFO, port->ProtocolTxBytes,
                                &port->ProtocolTxBuffer[0]);
                }

                /* Resend */
                port->Registers.Control.TX_START = 1;
                DeviceWrite(port->I2cAddr, regControl0, 1,
                        &port->Registers.Control.byte[0]);
                port->Registers.Control.TX_START = 0;
                TimerStart(&port->ProtocolTimer, tRetry); /* 3ms */

                /* Decrement Retries if there is no collision */
                port->ManualRetries -= 1;
            }

            port->DoManualRetry = FALSE;
            port->DoTxFlush = FALSE;
        }
    }
}

