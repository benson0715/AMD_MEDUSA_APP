.. _syspwr:

System Power
***************

Definitions
================================
- x86 - Main processors executing the x86 Instruction Set Architecture
- PMFW - System Management firmware responsible for Power Management
- PMFW - System Management Unit processor that executes PMFW
- PSP - Platform Security Processor
- PSP FW - Security firmware executed by the PSP
- FCH - Fusion Controller Hub
- SOC - System On Chip
- DF - Data Fabric 
- CCX - Core complex
- AGESA - AMD Generic Encapsulated Software Architecture is AMD reference code resistible for initializing the AMD SOC
- DXIO - Interconnect firmware responsible for initializing interconnect links (e.g PCIe)
- MP2 - Microprocessor that executes the sensor firmware
- MP2 FW - This is the firmware executed on the MP2 to program the sensor fusion controller
- ABL - AGESA BootLoader - AGESA SOC initialization code executed by the PSP
- UMC - Unified Memory Controller responsible for routing data to and from the system memory
- DDR - Double Data Rate channel to access the system memory
- DDR Phy - responsible for controlling the signaling on the DDR channel
- MEM - AGESA firmware responsible for programming the UMC and DDR Phy
- PMU - Phy Microcontroller Unit - responsible for training the DDR channel

================================

Introduction
================================
This document describes the firmware architecture of EC that support system power feature. System power (PSYS) is the power draw as detected by the charger and delivered as an analog signal to the SVI3 regulator. 

PSYS output from the charger needs to be configured and this is done by the EC FW. 

The VR config table to set SVI3 Voltage Regulator is provided by power team and need to be initialized by EC FW.

P3T - Peak Package Power Tracking, is a power limiter that ensures the instantaneous peak package power doesn't exceed the set P3T Limit. With P3T enabled, the peak package power is expected to be throttled down below the p3t limit within 5us. 

Feature Description
================================
System power (PSYS) is the power draw as detected by the charger 
and delivered as an analog signal to the SVI3 regulator. 
The ADC in the regulator outputs a digital value which is available 
on the regulator's telemetry interface. 
PMFW can read system power from this telemetry interface 
and use this information in its control loops that determine frequency 
and voltage settings for the various IPs in the SOC.

   .. figure:: syspwr_arch.png
      :width: 600px
      :name: syspwr_arch


================================

Feature Execution Flow
================================
1.	Battery Charger

PSYS output from the charger needs to be enabled and this is done by the EC FW. 
On the CRB, there are two chargers - one for the brick and the other for the USB-PD. 
This is different from an OEM system where there is only one charger IC. 
The EC FW has to therefore properly enable the PSYS output from the appropriate charger IC on the CRB.

2.	SVI3 Voltage Regulator

The default voltage regulator on a Birman CRB is MPS 2845 which does not support PSYS. 
So, this regulator is replaced with MPS 2835, a test chip which supports PSYS. 
The DRMOS on the platform is pin compatible with MPS 2835, so no updates are required to the DRMOS. 
On Birman+ CRB, the regulator used is MPS 2825 which is a common footprint part, and it supports PSYS. 

   .. figure:: psys_reg.png
      :width: 600px
      :name: psys_reg

   .. figure:: vr_init.png
      :width: 600px
      :name: vr_init

The MP2835 voltage regulator from Monolithic Power Systems has implemented PSYS with the following control register:

3.	Smart P3T 

P3T - Peak Package Power Tracking, is a power limiter that ensures the instantaneous peak package power doesn't exceed the set P3T Limit. With P3T enabled, the peak package power is expected to be throttled down below the p3t limit within 5us. 

P3T, Peak package power will progressively reduce a power throttler as battery is discharged in a DC setting. 

P3T will reduce the number of prochot assertions which should allow for a significant uplift in performance at high power limits. In addition to this it will provide an alternative safety function for battery current limit. 

   .. figure:: p3t.png
      :width: 600px
      :name: p3t

P3t prevents the current level from reaching its maximum limit by preventing the current from reaching to a level where prochot starts engaging.

Evidence/Scenario:

   The chart below shows the comparison of 2 cases:

      1.	When p3t is not engaged (i.e. p3t is set fix to 216W)
      2.	When p3t is engaged (i.e. p3t limit is determined on the basis of battery level)

Workloads: In the following analysis gaming workloads are being considered as they show an interesting behavior due to its fluctuating nature.

The following chart depicts that there is a significant increase in the score when p3t is implemented as compared to the system without p3t.

Graph description: 
- Blue bar is the score when p3t is at 216W
- Orange bar is the score when p3t is implemented in the system
- The grey bar shows increase in score (in %)

   .. figure:: workloads.png
      :width: 600px
      :name: p3t