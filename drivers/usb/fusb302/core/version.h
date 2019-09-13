/**
 *******************************************************************************
 * @file        version.h
 * @author      ON Semiconductor USB-PD Firmware Team
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
#ifndef VERSION_H_
#define VERSION_H_

/**
 * @brief Program Revision constant.  When updating firmware, change this.
 *
 * Format is Upper.Middle.Lower or Major.Minor.Revision
 */
#define FSC_TYPEC_CORE_FW_REV_UPPER     4   /* Major */
#define FSC_TYPEC_CORE_FW_REV_MIDDLE    0   /* Minor */
#define FSC_TYPEC_CORE_FW_REV_LOWER     1   /* Revision */

#endif /* VERSION_H_ */
