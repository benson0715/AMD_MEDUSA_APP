.. _spi:

SPI
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

Introduction
================================
Serial Peripheral Interface (SPI) is an interface bus commonly used to send data between microcontrollers 
and small peripherals such as shift registers, sensors, and SD cards. 
It uses separate clock and data lines, 
along with a select line to choose the device you wish to talk to.

Feature Description
================================
SPI is a "synchronous" data bus, which means that it uses separate lines for data 
and a "clock" that keeps both sides in perfect sync. 
The clock is an oscillating signal that tells the receiver exactly when to sample the bits on the data line. 
This could be the rising (low to high) or falling (high to low) edge of the clock signal. 

   .. figure:: spi_signal.png
      :width: 400px
      :name: spi_signal

   .. figure:: spi_signal2.png
      :width: 400px
      :name: spi_signal2

Feature Execution Flow
================================
- Devices on SPI; SPI ROM Mux, TPM header and EC 

   .. figure:: spi_device.png
      :width: 600px
      :name: spi_signal2

- ROM sharing between APU and EC 

- STX default uses 1x SPI ROM (1st ROM) and BOM option uses 2x SPI ROMs 

- SPI ROM Mux is daisy chained on the lines from APU to EC 

- Provide strapping option to support MAFS/SAFS on mother board, but feature can only be tested when new EC module firmware is ready. 

   - SAFS need dedicated FCH Boot Strip. 
   - ESPI need to be swapped to SPI0 by reworking 
   - ESPI Alert need to use in-band mode 

- Default strapping option is SPI Sharing ROM/MAFS. 
Master Attached Flash Sharing refer to the scheme where flash components are attached to the eSPI master such as chipset. eSPI slaves are allowed to access to the shared flash component through the Flash Access channel.

   .. figure:: MAF.png
      :width: 600px
      :name: MAF

For AMD platform does not support eSPI when system in G3/S4/S5, EC acts as SPI master to access SPI ROM. The current design is as follows:

   .. figure:: share_rom.png
      :width: 600px
      :name: share_rom

- Master Attached Flash Sharing and Slave Attached Flash Sharing

   .. figure:: SAF.png
      :width: 600px
      :name: SAF

Feature Implementation Details
================================
- AMD PSP Enabled EC eSPI SPI-ROM Access System Workflow
Current AMD Master Attached Flash Sharing (MAFS) scheme involves x86 cores to be constantly interrupted to handle SPI Flash access.  The SPI Flash accesses are mainly Write/Read/Erase. 
The requirement is to provide EC (Embedded Controller) with a design solution which does not interrupt x86 cores but still allows EC to proper access SPI Flash.

- EC places SPI-ROM transactions into eSPI buffer. Depending on transaction types(write/read.

- EC toggles GPIO pin GENINT1_L_AGPIO89_PSP_INTR0 as door-bell signal to PSP. This door-bell signal is interrupt based which generates interrupt to PSP. PSP is responsible to configure the GPIO and FCH for interrupt to be properly generated to PSP. 

- Once PSP receives this door-bell signal, it controls the eSPI controller to interact with EC.  PSP then consumes the eSPI buffer and faithfully conducts the SPI-ROM transaction through Rom Armor V2 Client version.  For detailed PSP-EC communication protocol, please refer to the section “PSP-EC eSPI controller communication protocol”.

Feature Verification Environment
================================
- Because of the high-speed signals, SPI should only be used to send data over short distances (up to a few feet). If you need to send data further than that, lower the clock speed, and consider using specialized driver chips.
- If things aren't working the way you think they should, a logic analyzer is a very helpful tool. Smart analyzers like the Saleae USB Logic Analyzer can even decode the data bytes for a display or logging.
- Read FLASH JEDCE ID

   .. figure:: read_id.png
      :width: 600px
      :name: read_id

MOSI send RDID command“0x9F”, MISO return 1-byte Manufacturer ID “0xC2” and 2-Byte Device ID “0x25 0x15”。FLASH JEDEC ID is “0xC2 0x25 0x15”.

   .. figure:: read_seq.png
      :width: 600px
      :name: read_seq