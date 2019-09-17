/***************************************//*************************************
 * FileName:        dpm.c
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
#include "vendor_info.h"
#include "dpm.h"
#include "platform.h"
#include "TypeC.h"

typedef enum
{
    dpmSelectSourceCap,
    dpmIdle,
} DpmState_t;

/** Definition of DPM interface
 *
 */
struct devicePolicy_t
{
    Port_t          *ports[NUM_PORTS];     ///< List of port managed
    int              num_ports;
    DpmState_t       dpm_state;
};

static DevicePolicy_t devicePolicyMgr;

void DPM_Init(DevicePolicy_t **dpm)
{
    devicePolicyMgr.num_ports = 0;
    devicePolicyMgr.dpm_state = dpmIdle;

    *dpm = &devicePolicyMgr;
}

void DPM_AddPort(DevicePolicy_t *dpm, Port_t *port)
{
    dpm->ports[dpm->num_ports++] = port;
}

sopMainHeader_t* DPM_GetSourceCapHeader(DevicePolicy_t *dpm, Port_t *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return &(port->src_cap_header);
}

sopMainHeader_t* DPM_GetSinkCapHeader(DevicePolicy_t *dpm, Port_t *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return &(port->snk_cap_header);
}

doDataObject_t* DPM_GetSourceCap(DevicePolicy_t *dpm, Port_t *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return port->src_caps;
}

doDataObject_t* DPM_GetSinkCap(DevicePolicy_t *dpm, Port_t *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return port->snk_caps;
}

FSC_BOOL DPM_TransitionSource(DevicePolicy_t *dpm, Port_t *port, int index)
{
    FSC_BOOL status = TRUE;
    FSC_U32 voltage = 0;

    if (port->src_caps[index].PDO.SupplyType == pdoTypeFixed)
    {
      /* Convert 10mA units to mA for setting supply current */
      platform_set_pps_current(port->PortID,
                               port->PolicyRxDataObj[0].FVRDO.OpCurrent*10);

      if (port->src_caps[index].FPDOSupply.Voltage == 100)
      {
          platform_set_pps_voltage(port->PortID, 250);
          platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_5V, TRUE, TRUE);
      }
      else
      {
          /* Convert 50mV units to 20mV for setting supply voltage */
          voltage = (port->src_caps[index].FPDOSupply.Voltage * 5) / 2;
          platform_set_pps_voltage(port->PortID, voltage);
          platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_HV, TRUE, TRUE);
      }
    }
    else if (port->src_caps[index].PDO.SupplyType == pdoTypeAugmented)
    {
      /* PPS request is already in 20mV units */
      platform_set_pps_voltage(port->PortID,
                               port->PolicyRxDataObj[0].PPSRDO.Voltage);

      /* Convert 50mA units to mA for setting supply current */
      platform_set_pps_current(port->PortID,
                               port->PolicyRxDataObj[0].PPSRDO.OpCurrent * 50);
      platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_HV, TRUE, TRUE);
    }

    return status;
}

FSC_BOOL DPM_IsSourceCapEnabled(DevicePolicy_t *dpm, Port_t *port, int index)
{
    FSC_BOOL status = FALSE;
    FSC_U32 sourceVoltage = 0;

    if (port->src_caps[index].PDO.SupplyType == pdoTypeFixed)
    {
      sourceVoltage = port->src_caps[index].FPDOSupply.Voltage;

      if (!isVBUSOverVoltage(port,
            VBUS_MV_NEW_MAX(VBUS_PD_TO_MV(sourceVoltage))) &&
          isVBUSOverVoltage(port,
            VBUS_MV_NEW_MIN(VBUS_PD_TO_MV(sourceVoltage))))
      {
          status = TRUE;
      }
    }
    else if (port->src_caps[index].PDO.SupplyType == pdoTypeAugmented)
    {
      sourceVoltage = port->USBPDContract.PPSRDO.Voltage;

      if (!isVBUSOverVoltage(port,
            VBUS_MV_NEW_MAX(VBUS_PPS_TO_MV(sourceVoltage))) &&
          isVBUSOverVoltage(port,
            VBUS_MV_NEW_MIN(VBUS_PPS_TO_MV(sourceVoltage))))
      {
          status = TRUE;
      }
    }

    return status;
}
