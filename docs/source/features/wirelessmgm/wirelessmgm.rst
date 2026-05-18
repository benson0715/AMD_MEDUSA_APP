.. _wirelessmgm:

Wireless Manageability
***************
This document describes the firmware architecture for Wireless Manageability in Zephyr ECFW for Strix Halo program. 
The document will provide a detailed description of the how the firmware architecture for Wireless Manageability will be implemented and verified. 
This document also describes the interaction between the Wireless Manageability and other firmware domains.

Definitions
================================
-  x86 - Main processors executing the x86 Instruction Set Architecture
-  PMFW - System Management firmware responsible for Power Management
-  PMFW - System Management Unit processor that executes PMFW
-  PSP - Platform Security Processor
-  PSP FW - Security firmware executed by the PSP
-  FCH - Fusion Controller Hub
-  SOC - System On Chip
-  DF - Data Fabric 
-  CCX - Core complex
-  AGESA - AMD Generic Encapsulated Software Architecture is AMD reference code resistible for initializing the AMD SOC
-  DXIO - Interconnect firmware responsible for initializing interconnect links (e.g PCIe)
-  MP2 - Microprocessor that executes the sensor firmware
-  MP2 FW - This is the firmware executed on the MP2 to program the sensor fusion controller
-  ABL - AGESA BootLoader - AGESA SOC initialization code executed by the PSP
-  UMC - Unified Memory Controller responsible for routing data to and from the system memory
-  DDR - Double Data Rate channel to access the system memory
-  DDR Phy - responsible for controlling the signaling on the DDR channel
-  MEM - AGESA firmware responsible for programming the UMC and DDR Phy
-  PMU - Phy Microcontroller Unit - responsible for training the DDR channel
-  AMF - AMD Management Firmware - The FW that runs in MPM
-  DASH - Desktop and Mobile Architecture for System Hardware
-  KVM - Keyboard, Video and Mouse
-  MC - Management Controller
-  MPM - Microprocessor Manageability - Core in SoC hosting the Manageability Engine
-  PLDM - Platform Level Data Model
-  AIM-T - AMD Integrated Management Technology
-  ASF - Alert Standard Format

================================
Feature Description
================================
Wireless manageability is a feature where enterprise administrators can monitor, control, and manage a multitude of remote devices through wireless interface without having to be physically near these devices. 
Several IP components like BIOS, PSP, MPM, EC, Windows OS, etc. are involved in supporting this feature (see Figure 1). 
The major role of EC is to transition the SoC from S5 to S0 for wireless manageability OS (mOS) boot, given wireless manageability conditions are met.

   .. figure:: mos_arch.png
      :width: 600px
      :name: mos_arch

Wireless manageability feature in EC is enabled/disabled by toggling a bit (0/1- disabled/enabled) in ECRAM via ACPI interface.

The wireless manageability feature can work in x86 out-of-band mode where x86 cores are not released. The SoC and EC handshake the MPM event through MPM_EVENT#. 
This pin has an on-board pull-up, and both the SoC and EC drive this pin as open drain. 
This is treated as bi- direction GPIO pins for both sides.

The EC operates this pin, adhering to the following rules:

   - To assert this pin, the EC drives it to GND.
   - To de-assert this pin, the EC does not drive it to GND.
   - Thus, after the EC de-asserts this pin, the pin state is LOW if the SoC has asserted it or HIGH if not.
   - To sample the pin, de-assert the pin first, wait for 1 µs so the pin can stabilize at the drive status of other side, then sample the pin.

The SoC drives this pin to GND when it shuts down from MPM where x86 cores were not released. 
SoC transit from S0 to S5 with MPM_EVENT# asserted means MPM is not expecting the EC to power on the SoC to S0 for MPM to boot mOS again. 
In this case, the MPM may set a timer before transit to S5 so EC keeps the S5 power domain for the FCH timer to work.
The EC cannot identify whether the system is shutting down from the host OS (typically Windows 10 run by x86) or from mOS (an embedded OS run by MPM) before the MPM_EVENT# signal is introduced. 
The information is modulated onto this signal so the EC can have different behavior in S5 depending on which OS is running.

Specifically, the EC should do the following:

- When the EC senses an S0 to S4/S5 transition:

   -  With AC not connected (DC-only), the EC should put the SoC in G3 for power saving.

   -  With AC connected, the EC should sample MPM_EVENT# status.
 
      - If the EC samples MPM_EVENT# at HIGH status, the EC (after ~6 seconds expiration) asserts this pin to GND and transfers the SoC to S0 for mOS boot by generating a 20 ms negative pulse on GPIO0/PWR_BTN_L.
      - If the EC samples MPM_EVENT# at LOW status, do nothing.

- When the EC senses the AC plug-in event where the SoC is in G3, and the end-user does not click the power button, the EC asserts MPM_EVENT# to GND, then transfers the SoC to S0 for mOS boot.

Initial Feature Program
================================
Strix Halo program is the first to support wireless manageability feature in Zephyr EC.

Feature Implementation Details
================================
EC Wireless Manageability Pseudo-Code:

   1.	ACPI handler - enable/disable the wireless manageability feature, including powering on/off the MPM_EVENT# signal. If the feature is disabled, none of the steps below are executed.
   2.	Introduce a one-shot timer, in its expiration callback - if the system is in S5 and MPM_EVENT# is HIGH, assert MPM_EVENT# for mOS boot and trigger the S5 to S0 power sequence.
   3.	Enter S5 hook (called from power sequencing module) - enable the timer with ~6 seconds expiration if MPM_EVENT# is HIGH, else do nothing.
   4.	Exit S5 hook - disable the timer.
   5.	Enter S0 hook - restart MPM_EVENT# debounce if MPM_EVENT# is HIGH, else do nothing.
   6.	Exit S0 hook - do nothing.
   7.	Power button hook (called from power button handler) - stop MPM_EVENT# debounce and de-assert MPM_EVENT#.
   8.	APU_RST rising edge hook (called from APU reset trigger handler) - de-assert MPM_EVENT# and stop MPM_EVENT# debounce.
   9.	Power source change hook - if current board power source is battery (DC-only), stop MPM_EVENT# debounce and de-assert MPM_EVENT#.

Customer Impact
================================

The feature is only supported on Pro SKUs. A hardware check ensures that the solution does not function on non-pro SKUs.

This feature requires the following: 

   - Dedicated flash space required to include MPM firmware images.
   - The wireless LAN device must be connected to the S5 AC power rails. Note: When a system is connected to the external AC power source, the wireless LAN must always be powered on.
   - WAKE# must be connected to an AGPIO that can wake the system from S5 to S0.
   - All LEDs should be connected through EC. This ensures that they can be turned off when in Manageability mode.
   To disable this feature, OEMs can do the following:
   - Remove the Manageability images from flash.
   - The BIOS has a UI element (setup option) to enable/disable this feature.

Feature Risk
================================
The firmware implementation risk associated with this feature is high. EC plays a major role in transitioning SoC from S5 to S0 and indicating the mOS boot via MPM_EVENT# signal.

Feature Verification Environment
================================
The environment needed to test this feature requires the following:

- Client system with WLAN device connected (QCOM NFA765 or MediaTek Z616 on WLAN/BT M.2 interface), the default MPM firmware image in BIOS is for Mediatek

- Enable MPM BIOS options

   - AMD CBS/SOC Miscellaneous Control / AIM-T / Moselle Options / AIM-T / Moselle support/AIM-T Enabled
   - AMD CBS/SOC Miscellaneous Control / AIM-T / Moselle Options / Wireless Manageability/Enabled
   - SOC Miscellaneous Control / AIM-T / Moselle Options / KVM for Wireless Manageability/Enabled

- Install MPM MailBox drivers in client system's Windows OS

   - AIM-T_Manageability_Service-2.0.0.546 (1).exe

- Generate the provisioning package

   - Install the generation tool on any Windows host system

      - Provisioning_Console_setup-2.0.0.259-AMD.exe

   - Follow Appendix-A step by step in the following document 

      - AMD AIM-T User Guide (Windows) 2022.07.31.docx
   
   - Copy the provisioning package to client system, run it as administrator: AIM-TProvisioningApp.exe -i XXXX_oMt
   - Reboot the client system
- Install Dashcli tool in host system and make sure the host system and client system are connected to the same wifi router

   - AMD DASH CLI Setup_4.5.0.1394.exe

- Execute dash command in host system to make sure client system can be discovered 

   - dashcli.exe -h <192.168.1.13> -u <xxx> -P <xxx> -S https -C -p 664 discover
   - If the client can be discovered, then try to shut down the client system by dash command: dashcli.exe -h <192.168.1.13> -S https -C -p 664 -u <xxx> -P <xxx> -t computersystem[0] power shutdown
   - Note: the username and password <xxx> above are the ones which you fill in when generating the provisioning package.
   - When the client system is shutdown with AIM-T enabled and provisioned, then system will auto wakeup to X86NRC case, that is expected.

Feature Verification Unit Test Plan
================================
For AIM-T EC specific unit test, the client system should auto wake up when it is shutdown remotely (EC will wake up the system), 
but system will not boot to host OS (Windows), instead, it will boot to mOS (X86NRC), that is, ABL/PSP will run but not release X86 core. 
After MPM get IP information, system should shutdown again automatically if there is no dash command or AC/DC event. 
In X86NRC case, if user plug in/out battery, system should wake up and boot to X86 NRC again.

Feature Verification Dependencies
================================
Verification of this feature has the following dependencies:

   - BIOS image with AIM-T capabilities
   - Client system with WLAN device
   - BIOS image with MPM firmware images built in
   - Windows MPM Mailbox drivers for the client system
   - Provisioning package for the client system
   - DASHCLI tool
   - Wifi router where both the client and host systems are connected to
   - AMF and PSP FW with AIM-T capabilities

Key Assumptions
================================
GPIO for the MPM_EVENT# signal and the ACPI interface specifics may change per program.

Deliverables
================================
Zephyr EC with wireless manageability feature will be delivered for the Strix Halo program. 

Risks
================================
The risks associated with this feature is QA not having the complete verification environment. 
That is, QA not setup to test this feature partially or wholly. 
Partial mitigation is for the developer to test the EC functionality associated with this feature.