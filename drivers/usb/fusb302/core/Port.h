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

#ifndef _PORT_H_
#define _PORT_H_

#include "TypeC_Types.h"
#include "PD_Types.h"
#include "fusb30X.h"
#include "platform.h"
#ifdef FSC_HAVE_VDM
#include "vdm_callbacks_defs.h"
#endif /* FSC_HAVE_VDM */
#ifdef FSC_HAVE_DP
#include "dp.h"
#endif /* FSC_HAVE_DP */
#include "observer.h"
#include "Log.h"
#include "dpm.h"
#include "timer.h"

/* Size of Rx/Tx FIFO protocol buffers */
#define FSC_PROTOCOL_BUFFER_SIZE 64

/**
 * All state variables here for now.
 */
typedef struct Port
{
    DevicePolicy_t*         dpm;
    PortConfig_t            PortConfig;
    FSC_U8                  PortID;                     /* Optional Port ID */
    FSC_U8                  I2cAddr;
    DeviceReg_t             Registers;
    FSC_BOOL                TCIdle;                     /* True: Type-C idle */
    FSC_BOOL                PEIdle;                     /* True: PE idle */

    /* All Type C State Machine variables */
    CCTermType              CCTerm;                     /* Active CC */
    CCTermType              CCTermCCDebounce;           /* Debounced CC */
    CCTermType              CCTermPDDebounce;
    CCTermType              CCTermPDDebouncePrevious;
    CCTermType              VCONNTerm;

    SourceOrSink            sourceOrSink;               /* TypeC Src or Snk */
    USBTypeCCurrent         SinkCurrent;                /* PP Current */
    USBTypeCCurrent         SourceCurrent;              /* Our Current */
    CCOrientation           CCPin;                      /* CC == CC1 or CC2 */
    FSC_BOOL                SMEnabled;               /* TypeC SM Enabled */
    ConnectionState         ConnState;                  /* TypeC State */
    FSC_U8                  TypeCSubState;              /* TypeC Substate */
    FSC_U16                 DetachThreshold;            /* TypeC detach level */
    FSC_U8                  loopCounter;                /* Count and prevent
                                                           attach/detach loop */
    FSC_BOOL                C2ACable;                   /* Possible C-to-A type
                                                           cable detected */
    /* All Policy Engine variables */
    PolicyState_t           PolicyState;                /* PE State */
    FSC_U8                  PolicySubIndex;             /* PE Substate */
    SopType                 PolicyMsgTxSop;             /* Tx to SOP? */
    FSC_BOOL                USBPDActive;                /* PE Active */
    FSC_BOOL                USBPDEnabled;               /* PE Enabled */
    FSC_BOOL                PolicyIsSource;             /* Policy Src/Snk? */
    FSC_BOOL                PolicyIsDFP;                /* Policy DFP/UFP? */
    FSC_BOOL                PolicyHasContract;          /* Have contract */
    FSC_BOOL                isContractValid;            /* PD Contract Valid */
    FSC_BOOL                IsHardReset;                /* HR is occurring */
    FSC_BOOL                IsPRSwap;                   /* PR is occurring */
    FSC_BOOL                IsVCONNSource;              /* VConn state */
    FSC_BOOL                USBPDTxFlag;                /* Have msg to Tx */
    FSC_BOOL                requests_pr_swap_as_src;
    FSC_BOOL                requests_pr_swap_as_snk;
    FSC_U8                  CollisionCounter;           /* Collisions for PE */
    FSC_U8                  HardResetCounter;
    FSC_U8                  CapsCounter;                /* Startup caps tx'd */

    sopMainHeader_t         src_cap_header;
    sopMainHeader_t         snk_cap_header;
    doDataObject_t          src_caps[7];
    doDataObject_t          snk_caps[7];

    FSC_U8                  PdRevContract;              /* Current PD Rev */
    FSC_BOOL                PpsEnabled;                 /* True if PPS mode */

    FSC_BOOL                WaitingOnHR;                /* HR shortcut */

    /* All Protocol State Machine Variables */
    ProtocolState_t         ProtocolState;              /* Protocol State */
    sopMainHeader_t         PolicyRxHeader;             /* Header Rx'ing */
    sopMainHeader_t         PolicyTxHeader;             /* Header Tx'ing */
    sopMainHeader_t         PDTransmitHeader;           /* Header to Tx */
    sopMainHeader_t         CapsHeaderReceived;         /* Recent caps */
    doDataObject_t          PolicyRxDataObj[7];         /* Rx'ing objects */
    doDataObject_t          PolicyTxDataObj[7];         /* Tx'ing objects */
    doDataObject_t          PDTransmitObjects[7];       /* Objects to Tx */
    doDataObject_t          CapsReceived[7];            /* Recent caps header */
    doDataObject_t          USBPDContract;              /* Current PD request */
    doDataObject_t          SinkRequest;                /* Sink request  */
    doDataObject_t          PartnerCaps;                /* PP's Sink Caps */
    doPDO_t                 vendor_info_source[7];      /* Caps def'd by VI */
    doPDO_t                 vendor_info_sink[7];        /* Caps def'd by VI */
    SopType                 ProtocolMsgRxSop;           /* SOP of msg Rx'd */
    SopType                 ProtocolMsgTxSop;           /* SOP of msg Tx'd */
    PDTxStatus_t            PDTxStatus;                 /* Protocol Tx state */
    FSC_BOOL                ProtocolMsgRx;              /* Msg Rx'd Flag */
    FSC_BOOL                ProtocolCheckRxBeforeTx;
    FSC_U8                  ProtocolTxBytes;            /* Bytes to Tx */
    FSC_U8                  ProtocolTxBuffer[FSC_PROTOCOL_BUFFER_SIZE];
    FSC_U8                  ProtocolRxBuffer[FSC_PROTOCOL_BUFFER_SIZE];
    FSC_U8                  ProtocolCRC[4];
    FSC_U8                  MessageIDCounter[SOP_TYPE_NUM]; /* Local ID count */
    FSC_U8                  MessageID[SOP_TYPE_NUM];    /* Local last msg ID */

    FSC_BOOL                DoTxFlush;                  /* Collision -> Flush */
    FSC_U8                  ManualRetriesEnabled;
    FSC_BOOL                DoManualRetry;              /* Retries needed */
    FSC_U8                  ManualRetries;              /* Retry Count */

    /* Timer objects */
    struct TimerObj         PDDebounceTimer;            /* First debounce */
    struct TimerObj         CCDebounceTimer;            /* Second debounce */
    struct TimerObj         StateTimer;                 /* TypeC state timer */
    struct TimerObj         LoopCountTimer;             /* Loop delayed clear */
    struct TimerObj         PolicyStateTimer;           /* PE state timer */
    struct TimerObj         ProtocolTimer;              /* Prtcl state timer */
    struct TimerObj         SwapSourceStartTimer;       /* PR swap delay */
    struct TimerObj         PpsTimer;                   /* PPS timeout timer */
    struct TimerObj         VBusPollTimer;              /* VBus monitor timer */

#ifdef FSC_HAVE_EXT_MSG
    ExtMsgState_t           ExtTxOrRx;                  /* Tx' or Rx'ing  */
    ExtHeader_t             ExtTxHeader;
    ExtHeader_t             ExtRxHeader;
    FSC_BOOL                ExtWaitTxRx;                /* Waiting to Tx/Rx */
    FSC_U16                 ExtChunkOffset;             /* Next chunk offset */
    FSC_U8                  ExtMsgBuffer[260];
    FSC_U8                  ExtChunkNum;                /* Next chunk number */
#endif

#ifdef FSC_GSCE_FIX
    /* Flag for GetSourceCapsExtended hardware bug workaround */
    FSC_BOOL                NextGSCE;
#endif /* FSC_GSCE_FIX */

#ifdef FSC_DEBUG
    FSC_BOOL                SourceCapsUpdated;          /* GUI new caps flag */
#endif /* FSC_DEBUG */

#ifdef FSC_DTS
    FSC_BOOL                DTSMode;                    /* DTS mode flag */
#endif /* FSC_DTS */

#ifdef FSC_HAVE_VDM
    VdmDiscoveryState_t     AutoVdmState;
    VdmManager              vdmm;
    PolicyState_t           vdm_next_ps;
    PolicyState_t           originalPolicyState;
    SvidInfo                core_svid_info;
    SopType                 VdmMsgTxSop;
    doDataObject_t          vdm_msg_obj[7];
    struct TimerObj         VdmTimer;
    FSC_U32                 my_mode;
    FSC_U32                 vdm_msg_length;
    FSC_BOOL                sendingVdmData;
    FSC_BOOL                VdmTimerStarted;
    FSC_BOOL                vdm_timeout;
    FSC_BOOL                vdm_expectingresponse;
    FSC_BOOL                svid_enable;
    FSC_BOOL                mode_enable;
    FSC_BOOL                mode_entered;
    FSC_BOOL                AutoModeEntryEnabled;
    int                     AutoModeEntryObjPos;
    FSC_U16                 auto_mode_disc_tracker;
    FSC_U16                 my_svid;
    FSC_U16                 requestSvid;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_HAVE_DP
    DisplayPortCaps_t       DpModeEntryMask;
    DisplayPortCaps_t       DpModeEntryValue;
    /* DisplayPort Status/Config objects */
    DisplayPortCaps_t       DpCaps;
    DisplayPortStatus_t     DpStatus;
    DisplayPortConfig_t     DpConfig;
    /* Port Partner Status/Config objects */
    DisplayPortStatus_t     DpPpStatus;
    DisplayPortConfig_t     DpPpRequestedConfig;
    DisplayPortConfig_t     DpPpConfig;
    FSC_BOOL                DpEnabled;
    FSC_BOOL                DpAutoModeEntryEnabled;
    FSC_U32                 DpModeEntered;
#endif /* FSC_HAVE_DP */

#ifdef FSC_DEBUG
    StateLog                TypeCStateLog;          /* Log for TypeC states */
    StateLog                PDStateLog;             /* Log for PE states */
    FSC_U8                  USBPDBuf[PDBUFSIZE];    /* Circular PD msg buffer */
    FSC_U8                  USBPDBufStart;          /* PD msg buffer head */
    FSC_U8                  USBPDBufEnd;            /* PD msg buffer tail */
    FSC_BOOL                USBPDBufOverflow;       /* PD Log overflow flag */
#endif /* FSC_DEBUG */
} Port_t;

/**
 * @brief Initializes the port structure and state machine behaviors
 * 
 * Initializes the port structure with default values.
 * Also sets the i2c address of the device and enables the TypeC state machines.
 * 
 * @param port Pointer to the port structure
 * @param i2c_address 8-bit value with bit zero (R/W bit) set to zero.
 * @return None
 */
void PortInit(Port_t *port, FSC_U8 i2cAddr);

/**
 * @brief Set the next Type-C state.
 *
 * Also clears substate and logs the new state value.
 *
 * @param port Pointer to the port structure
 * @param state Next Type-C state
 * @return None
 */
void SetTypeCState(Port_t *port, ConnectionState state);

/**
 * @brief Set the next Policy Engine state.
 *
 * Also clears substate and logs the new state value.
 *
 * @param port Pointer to the port structure
 * @param state Next Type-C state
 * @return None
 */
void SetPEState(Port_t *port, PolicyState_t state);

/**
 * @brief Revert the ports current setting to the configured value.
 *
 * @param port Pointer to the port structure
 * @return None
 */
void SetConfiguredCurrent(Port_t *port);

#endif /* _PORT_H_ */
