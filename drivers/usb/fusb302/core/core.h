/**
 *******************************************************************************
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

#ifndef _FSC_CORE_H
#define _FSC_CORE_H

#include "platform.h"
#include "Port.h"

/**
 * @brief Initializes the core port structure.
 * 
 * Initializes the port structure with default values.
 * Also sets the i2c address of the device and enables the TypeC state machines.
 * 
 * @param port Pointer to the port structure
 * @param i2c_address 8-bit value with bit zero (R/W bit) set to zero.
 * @return None
 */
void core_initialize(Port_t *port, FSC_U8 i2c_address);

/**
 * @brief Enable the TypeC or PD state machines.
 * 
 * Sets whether the state machine functions are executed.
 * Does no initialization or reset of variables.
 * 
 * @param port Pointer to the port structure
 * @param enable TRUE/FALSE
 * @return None
 */
void core_enable_typec(Port_t *port, FSC_BOOL enable);
void core_enable_pd(Port_t *port, FSC_BOOL enable);

/**
 * @brief Core state machine entry point.
 * 
 * Called from a high level while(1) loop or interrupt handler to process
 * port status and control.
 * May block briefly (e.g. for I2C peripheral access) but should otherwise
 * return after a single pass through the state machines.
 * 
 * @param port Pointer to the port structure
 * @return None
 */
void core_state_machine(Port_t *port);

/**
 * @brief Determine the time to the next expiring core timer.
 * 
 * The state machines occasionally need to wait for or to do something.
 * Instead of allowing the state machines to block, use this value to
 * implement a (for example) timer interrupt to delay the next call of the
 * core state machines.
 * This function returns the time to the next event in the resolution set
 * by the Platform (milliseconds * TICK_SCALE_TO_MS).
 * 
 * @param port Pointer to the port structure
 * @return Interval to next timeout event (See brief and platform timer setup)
 */
FSC_U32 core_get_next_timeout(Port_t *port);

/**
 * @brief Core release version
 * 
 * Release version of the firmware in an Upper.Middle.Lower or
 * Major.Minor.Revision format.
 * 
 * @param None
 * @return Version values
 */
FSC_U8 core_get_rev_lower(void);
FSC_U8 core_get_rev_middle(void);
FSC_U8 core_get_rev_upper(void);

/**
 * @brief Send a PD Hard Reset
 * 
 * Puts the PE state machine into a send hard reset state.
 * 
 * @param port Pointer to the port structure
 * @return None
 */
void core_send_hard_reset(Port_t *port);

/**
 * @brief Enable/Disable the manual retries functionality.
 * 
 * @param port Pointer to the port structure
 * @param enable Boolean value to turn the functionality on/off.
 * @return Boolean indicating functionality is on/off.
 */
void core_set_manual_retries(Port_t *port, FSC_BOOL retries);
FSC_BOOL core_get_manual_retries(Port_t *port);
    
/**
 * @brief Detatch and reset the current port.
 * 
 * Instruct the port state machines to detach and reset to their 
 * looking-for-connection state.
 * 
 * @param port Pointer to the port structure
 * @return None
 */
void core_set_state_unattached(Port_t *port);

/**
 * @brief Reset PD state machines.
 * 
 * For the current connection, re-initialize and restart the
 * PE and Protocol state machines.
 * 
 * @param port Pointer to the port structure
 * @return None
 */
void core_reset_pd(Port_t *port);

/**
 * @brief Return advertised source current available to this device.
 * 
 * If this port is the Sink device, return the available current being
 * advertised by the port partner.
 * 
 * @param port Pointer to the port structure
 * @return Current in mA - or 0 if this port is the source.
 */
FSC_U16 core_get_advertised_current(Port_t *port);

/**
 * @brief Set advertised source current for this device.
 *
 * If this port is the Source device, set the currently advertised source
 * current.
 *
 * @param port Pointer to the port structure
 * @param value Source current enum requested
 * @return None
 */
void core_set_advertised_current(Port_t *port, FSC_U8 value);


/**
 * @brief Return current CC orientation
 * 
 *  0 = No CC pin
 *  1 = CC1 is the CC pin
 *  2 = CC2 is the CC pin
 * 
 * @param port Pointer to the port structure
 * @return CC orientation
 */
FSC_U8 core_get_cc_orientation(Port_t *port);

/**
 * @brief High level port configuration
 * 
 * DRP is automatically enabled when setting Try.*
 * Try.* is automatically disabled when setting DRP/SRC/SNK
 * 
 * @param port Pointer to the port structure
 * @return None
 */
void core_set_drp(Port_t *port);
void core_set_try_snk(Port_t *port);
void core_set_try_src(Port_t *port);
void core_set_source(Port_t *port);
void core_set_sink(Port_t *port);

#endif /* _FSC_CORE_H */

