.. _ucsi:

USCI Device support
***************
This document describes the firmware architecture EC to support UCSI driver. 
UCSI is one Microsoft drvier and we can find in the the device manager if UCSI support is enabled. 
This driver allows OS to access PD controller to understand more information about what happens in type-c port. 
For example, if there is device attached, what is the type of the device and what is the capability of the device. 
Then OS may react based on the event happens in type-c port for example, trigger data role swap or power role swap. 

To matter this requirement, OS defines the communication protocol between Host and EC and to deliver the command/response for UCSI message. 
It needs EC to implement corresponding code as bridge between OS/Bios and PD controller.

The USB Type-C Connector System Software Interface describes the registers and data structures used to interface with the USB Type-C connector on the system. 
The system software component is referred to as the OS policy Manager (OPM) in this features.

The combination of hardware and firmware and any vendor-provided OS software that provides the interfrace to all the USB Type-C connectors on the platofmr is referreed to as the platform policy manager (PPM) in this feature. 
In addition, there may be individual policy manager for each USB Type-C connector on the platform. 
The individual policy manager are referred to as Local Policy Manager (LPM) in this feature.

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
- Connector - A USB Type-C connector that is attached to the platform
- LPM - Local Policy Manager. Hardware/Firmware that manages an individual USB Type-C connector.
- OPM - OS Policy Manager. Operating Software that interfaces with the PPM
- PPM - Platform Policy Manager. Hardware/Firmware that manages all the USB Type-C connectors on the platform.


Document Reference
================================
- Add any references that will help to understand the content of this document:
- USB Type-C Connector System Software Interface [UCSI] V2.1
- Bios Implementation of UCSI V0.1

Introduction
================================
This document describes the firmware architecture EC to support UCSI driver. 
UCSI is one Microsoft drvier and we can find in the the device manager if UCSI support is enabled. 
This driver allows OS to access PD controller to understand more information about what happens in type-c port. 
For example, if there is device attached, what is the type of the device and what is the capability of the device. 
Then OS may react based on the event happens in type-c port for example, trigger data role swap or power role swap. 

To matter this requirement, OS defines the communication protocol between Host and EC and to deliver the command/response for UCSI message. 
It needs EC to implement corresponding code as bridge between OS/Bios and PD controller.

The USB Type-C Connector System Software Interface describes the registers and data structures used to interface with the USB Type-C connector on the system. 
The system software component is referred to as the OS policy Manager (OPM) in this features.

The combination of hardware and firmware and any vendor-provided OS software that provides the interfrace to all the USB Type-C connectors on the platofmr is referreed to as the platform policy manager (PPM) in this feature. 
In addition, there may be individual policy manager for each USB Type-C connector on the platform. 
The individual policy manager are referred to as Local Policy Manager (LPM) in this feature.


Feature Description
================================
This feature describes the Embedded Controller (EC) implementation for the UCSI feature to cover the PPM part. 
OS will act as OPM and PD controller will act as LPM.

Firmware Requirements Document Reference
================================

Feature Execution Flow
================================
1.	Logical blocks on a Type-C Host:

   .. figure:: logical_block.png
      :width: 600px
      :name: logical_block

Above diagram shows the OPM/PPM/LPM role definition. And EC should cover the PPM as bridge between OPM and LPM.

2.	Details blocks on a Type-C Host:
Below diagram shows the detail information about UCSI structures. 
OPM is OS and it will run the application and the UCSI driver itself. 
PPM is EC and it will delivery or decode the UCSI message protocol from OS and translate the the protocol which can be understood by LPM (PD controller). 
As different PD controller may have different EC to PD protocol, EC should act as the middle translation layer between OS and PD. 
When PD response for the command, EC also needs to change the PD protocol to the regular UCSI response protocol to host. 
LPM is PD controller and it is the real machine to control the type-c port and the source of all UCSI related messages.

   .. figure:: details_blocks.png
      :width: 600px
      :name: details_blocks

3.	Windows UCSI architecture:

   .. figure:: windows_ucsi_arch.png
      :width: 600px
      :name: windows_ucsi_arch

This diagram shows how windows OS operate UCSI driver. 
It support inbox UCSI driver and customized UcmCx client driver. 
If we choose to use UCSI inbox driver, EC needs to follow the same UCSI generic procotol. 
If we use UcmCx driver which may provide by PD vendors or third part suppilers, \
we can support custimzed proctol based on the driver. 
In AMD CRB we are using the inbox driver.

   .. figure:: ucsi_flow.png
      :width: 600px
      :name: ucsi_flow

Need to write a driver for the connector if your USB Type-C system does not implement PD state machine or it implements state machine 
but does not support UCSI over non-ACPI transport.

4.	UCSI Hardware architecture:

   .. figure:: ucsi_hw.png
      :width: 600px
      :name: ucsi_hw

   - PD controller task includes:

      - Implement PD policy engine, protocol layer, physical layer.
      - Implement default platform policy for the USB-C ports on the system.
      - Provide control query I2C interface hooks for EC.

   - Embedded controller task includes:
 
      - Implement I2C interface to PD controller
      - Implement UCSI update EC's mailbox
      - Implement SCI, update EC's mailbox
      - Communicate status change, provide role swap hooks, etc

   - Bios ACPI task includes:

      - Report USB-C connector to the OS
      - Implement UCSI mail box paradigm
      - Initialization of EC's mailbox
      - _DSM method that notifies EC to read the OS message

5.	Bios bring up for UCSI:

   .. figure:: bios_ucsi.png
      :width: 600px
      :name: bios_ucsi

   .. figure:: bios_ucsi2.png
      :width: 600px
      :name: bios_ucsi2

   .. figure:: bios_ucsi3.png
      :width: 600px
      :name: bios_ucsi3

Feature Implementation Details
================================
- UCSI implementation on the IFX PD controller:

      .. figure:: ifx_pd.png
         :width: 600px
         :name: ifx_pd

   - CCGx Implements UCSI & HPI on the I2C interface between EC & CCGx devices
   - UCSI interface will be exposed within the HPI Space at offset 0xF000
   - CCGx supports the pass through mode which means it can decode the original UCSI protocol.
   - There is no need for EC to translate the UCSI message to PD protocol.

      .. figure:: ifx_pd2.png
         :width: 600px
         :name: ifx_pd2

   - UCSI memory region in CCGx starts at offset 0xF000 & end at 0xF02F
   - The UCSI memory region is split into read-only and write-only regions (from the OS and EC perspective. Refer to UCSI Specification for more details on the field)
   - Memory map definition is the same as used in the OS-BIOS-EC interface 

      .. figure:: memory_map.png
         :width: 600px
         :name: memory_map

   - UCSI commands need to support system:

      .. figure:: ucsi_cmd.png
         :width: 600px
         :name: ucsi_cmd

      .. figure:: ucsi_cmd2.png
         :width: 600px
         :name: ucsi_cmd2

      a.	UCSI driver issues a UCSI command, by writing into CONTROL and MESSAGE_OUT region in the shared memory and  invokes the _DSM ACPI method to trigger the BIOS to handle the write 
      b.	BIOS _DSM code will copy the write-only regions from shared memory to the EC via the 0x62/0x66 port invoking a custom EC command. The interface between the EC and BIOS can be something custom as well. 
      c.	EC takes the data written to it and sends it to CCGX by writing to the respective write-only regions. 
      d.	CCGX raises an interrupt by making the INT# line low
      e.	EC reads the read-only regions from CCGX and issues an SCI which alerts the BIOS
      f.	BIOS's SCI handler will read this data from the EC and issues an ACPI Notify to the OS
      g.	OS driver will read the read-only regions (MESSAGE_IN & CCI) and handle the response as needed

   - This example demonstrates the OS issued UCSI Command - Get Capability

      .. figure:: ucsi_cmd3.png
         :width: 600px
         :name: ucsi_cmd3

   - UCSI Command - Get Capability Continuation

      .. figure:: ucsi_cmd4.png
         :width: 600px
         :name: ucsi_cmd4

      .. figure:: ucsi_cmd5.png
         :width: 600px
         :name: ucsi_cmd5

      .. figure:: ucsi_cmd6.png
         :width: 600px
         :name: ucsi_cmd6

      a. CCGX raises an interrupt by making the INT# line low
      b. EC reads from the MESSAGE_IN & CCI registers in CCGX and issues an SCI which alerts the BIOS
      c. BIOS's SCI handler will read this data from the EC and issues an ACPI Notify to the OS
      d. OS driver will read the read-only regions (MESSAGE_IN & CCI) and handle the response as needed

   - This example demonstrates the UCSI asynchronous notification flow when a device is connected. [Note: OS should have issued the SET_NOTIFICATION_ENABLE with Connect Change bit set to receive this event]

      .. figure:: ucsi_cmd7.png
         :width: 600px
         :name: ucsi_cmd6

      .. figure:: ucsi_cmd8.png
         :width: 600px
         :name: ucsi_cmd6

- UCSI implementation on the TI PD controller:

      .. figure:: ucsi_cmd9.png
         :width: 600px
         :name: ucsi_cmd9

      .. figure:: ucsi_cmd10.png
         :width: 600px
         :name: ucsi_cmd10

For example, TI cannot support PPM_Reset command. 
When EC receives this command from Bios, it needs to send command to TI to reset PD controller. 
EC also needs to reset the internal data to default value. 
Then send command complete response to Bios.

Feature Firmware Domain Interactions
================================
Support Windows driver

Firmware Dependency
================================
- SBIOS to EC firmware interface definition
- ESPI EC support on platform
- EC depends on eSPI bus being initialized prior to use
- ESPI initialization sequence documented in PPR
- CPM to SBIOS interface in UEFI definition
- DSDT/SSDT ACPI tables for EC definition
- MMIO/IO APCB token decode ranges definition
- APCB binary with MMIO/IO tokens
- EC FW binary

Feature Verification Environment
================================
- Windows OS
- HLK

Feature Verification Test Plan details 
================================
- USB-C Host systems required to pass Windows HLK test for Windows hardware compatibility certification

- USB-C Controller + EC required to implement all UCSI features as per the UCSI specification
 
   - Windows 10 also requires several UCSI features that are marked as “Optional” in the UCSI Specification

- USB-C/PD HLK tests includes:

   - USC-C ACPI Validation Test
   - UCSI Command Tests
   - Power Role Swap Tests
   - Data Role Swap Tests
   - Battery Charging Tests


Feature Test Plan Types
================================
- Unit
- Integration
- Test Vehicle (TV) board

Feature Verification Unit Test Plan
================================
- HLK Test Server

- UCSI 1.x compliant Windows desktop systems.

- Super MUTT Hardware
 
   - Power adapter

- USB Type-C PD capable cable.

- USB 2.0 and USB 3.0 device

- A switch to connect Server, SUT and Partner into same net. Highly suggest to use wire to set-up net environment.

      .. figure:: ucsi_test.png
         :width: 600px
         :name: ucsi_cmd10

Feature Verification Dependencies
================================
- ESPI EC support in the environment
- PD controller FW
- Bios implementation
