/**
 *******************************************************************************
 * @file        vendor_info.c
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

#include "vendor_info.h"
#include "PD_Types.h"

void VIF_InitializeSrcCaps(doDataObject_t *src_caps)
{
    FSC_U8 i;
    doDataObject_t gSrc_caps[7] =
    {
        /* macro expects index starting at 1 and type */
        CREATE_SUPPLY_PDO_FIRST(1),
        CREATE_SUPPLY_PDO(2, Src_PDO_Supply_Type2),
        CREATE_SUPPLY_PDO(3, Src_PDO_Supply_Type3),
        CREATE_SUPPLY_PDO(4, Src_PDO_Supply_Type4),
        CREATE_SUPPLY_PDO(5, Src_PDO_Supply_Type5),
        CREATE_SUPPLY_PDO(6, Src_PDO_Supply_Type6),
        CREATE_SUPPLY_PDO(7, Src_PDO_Supply_Type7),
    };

    for(i = 0; i < 7; ++i) {src_caps[i].object = gSrc_caps[i].object;}
}
void VIF_InitializeSnkCaps(doDataObject_t *snk_caps)
{
    FSC_U8 i;
    doDataObject_t gSnk_caps[7] =
    {
        /* macro expects index start at 1 and type */
        CREATE_SINK_PDO(1, Snk_PDO_Supply_Type1),
        CREATE_SINK_PDO(2, Snk_PDO_Supply_Type2),
        CREATE_SINK_PDO(3, Snk_PDO_Supply_Type3),
        CREATE_SINK_PDO(4, Snk_PDO_Supply_Type4),
        CREATE_SINK_PDO(5, Snk_PDO_Supply_Type5),
        CREATE_SINK_PDO(6, Snk_PDO_Supply_Type6),
        CREATE_SINK_PDO(7, Snk_PDO_Supply_Type7),
    };

    for(i = 0; i < 7; ++i) {snk_caps[i].object = gSnk_caps[i].object;}

}

#ifdef FSC_HAVE_EXT_MSG
FSC_U8 gCountry_codes[6] =
{
        2, 0, /* 2-byte Number of country codes */

        /* country codes follow */
        'U','S','C','N'
};
#endif
