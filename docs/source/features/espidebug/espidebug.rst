.. _espidebug:

eSPI Diags Test Interface
***************
This document describes the EC eSPI diags test interface architecture for the system function verification. 
A system firmware feature test interface impacts multiple firmware domain. 
The document will provide a detailed description of the how the firmware architecture for feature will be implemented and verified. 
This document also describes the interaction between domains.

Definitions
================================
-  x86 - Main processors executing the x86 Instruction Set Architecture
-  PSP - Platform Security Processor
-  PSP FW - Security firmware executed by the PSP
-  FCH - Fusion Controller Hub
-  EC - Embedded Controller 
-  eSPI - Enhanced Serial Peripheral Interface
-  OOB - Out Of Band Channel Interface
-  MAFS - General and Controller-Attached
-  CAFS - General and Controller-Attached
-  SAFS - Target Attached Flash Sharing Interface
-  TAFS - Target Attached Flash Sharing Interface
-  EC_SC - Embedded Controller Status Command

Document Reference
================================
Add any references that will help to understand the content of this document.

   - MEC172x+Keyboard+and+Embedded+Controller+for+Notebook+PC+-+Data+Sheet.pdf
   - eSPI Block 1.5 export.pdf
   - EC_OOB_Interface_Spec.doc
   - ACPI_5_Errata_A.pdf
   - 327432 espi_base_specification R1-5.pdf
   - PSP_Enabled_EC_eSPI_Access_Design_Specification.pdf
   - EC - eSPI Mini FAD v1.0

Getting started with the interface.
================================
There are planes be exported into ACPI EC space for the purpose of debugging and tuning on-the-fly. The interface is leveraging standard EC ACPI commands (RD_EC 0x80/WR_EC 0x81) as addressed by ACPI specification section 12.

Users can access the space by following the standard operation sequence of RD_EC/WR_EC commands. Or by a 3rd part, friendly, free charge tool, called RWEverything. The latest version of this tool is v1.7.0. Builds for x64 can be downloaded here. For other versions please check out by this link http://rweverything.com/download/.

On SUT, uncompressing the package and double click Rw.exe for running. Click 'Access' -> 'Embedded Controller'. If you are asked for EC_SC and EC_DATA by a popup dialog, input 666 and 662 respectively. Click 'OK' once finished.

   .. figure:: ec_access.png
      :width: 300px
      :name: ec_access

The ACPI EC space will be shown by the next window. 256 bytes in total. See below screenshot.

   .. figure:: ec_access2.png
      :width: 600px
      :name: ec_access2

In the following of this document, ECRAMxYY is used for indicating one cell of the space. YY can be from 0x00 to 0xFF. So ECRAMx40 holds the value 0xEA and ECRAMx41 is 0x0B from the above screenshot.

Debug interfaces are disabled by default. User will need to program ECRAMx31 to one of valid plane ID as below before using.

   .. figure:: ec_ram.PNG
      :width: 600px
      :name: ec_ram

To write a byte by RWEverything, double clicking the cell and type the new value in the popup window. Then click 'Done' to get it takes effect. Below screenshot is intended to set ECRAMx31 to 0xDB.

   .. figure:: ec_ram2.png
      :width: 600px
      :name: ec_ram2

How to access the EC RAM by scripts.
================================
Considering some of tests are performed automatically by scripts through Kysy, below is a demo in C for how the ECRAM can be accessed. The test framework needs to provide two basic IO access functions.

.. code-block:: c
 
   uint8_t readIo8 (uint16_t Address);
   void writeIo8 (uint16_t Address, uint8_t data);

After that, the upper test routines can access an ECRAM cell by calling read_ec_ram / write_ec_ram. For example, to set ECRAMxB0 bit6 to 1,

.. code-block:: c
 
   uint8_t temp = read_ec_ram (0xB0); // read current value of ECRAMxB0
   temp |= 0x40;                      // set bit 6
   write_ec_ram(0xB0, temp);          // write the updated value back to ECRAMxB0

.. note::

EC RAM can be accessed in 8-bit width in a time. As thus the upper software needs to do a read-modify-write for any fields less than a byte.

.. code-block:: c
 
   /*******************************************************
   * This is a demo on how to access EC RAM by ACPI defined
   * standard EC command. (0x80/0x81)
   *******************************************************/
   
   #define EC_SC   0x666   // CRB EC allocates IO pair 666h/662h
   #define EC_DATA 0x662   // CRB EC allocates IO pair 666h/662h
   
   #define EC_OBF  1
   #define EC_IBF  2

   //
   // External IO access functions
   //
   external uint8_t readIo8 (uint16_t Address);             // read 1 byte from IO port
   external void writeIo8 (uint16_t Address, uint8_t data); // write 1 byte to IO port
   
   void wait4Ibe()
   {
   sleep (200);   // sleep at least 200 microseconds

   while( readIo8(EC_SC) & EC_IBF ) {
   sleep (200); // sleep at least 200 microseconds
   }
   }
   
   void wait4Obf()
   {
   sleep (200);   // sleep at least 200 microseconds

   while(!(readIo8(EC_SC) & EC_OBF)) {
   sleep (200); // sleep at least 200 microseconds
   }
   }
   
   uint8_t read_ec_ram (uint8_t index) {
   wait4Ibe ();
   writeIo8 (EC_SC, 0x80);
   wait4Ibe ();
   writeIo8 (EC_DATA, index);
   wait4Obf ();
   return readIo8 (EC_DATA);
   }
   
   void write_ec_ram (uint8_t index, uint8_t data) {
   wait4Ibe ();
   writeIo8 (EC_SC, 0x81);
   wait4Ibe ();
   writeIo8 (EC_DATA, index);
   wait4Ibe ();
   writeIo8 (EC_DATA, data);
   }

Atomic access sequence 
================================
As ACPI-EC interface allows only 1-byte access each time. For WORD/DWORD access, upper software needs to perform atomic access sequence to ensure the separated byte accesses are for one number. Unless otherwise specified, software should do the following,

   - To atomic write a multi-byte word, write the lowest byte first. Write the highest byte will case EC to retrieve and apply the whole word into its inner buffer.
   - To atomic read a multi-byte word, read the highest byte first. EC will lock the whole word until next time the highest byte is read.
   - Accessing of other ECRAM space is not allowed during an atomic read/write sequence.

Bytes order
================================
Unless otherwise specified, the debug interface uses Little-Endian for multi-byte word. Take BmHwStatus as example, it is a 32-bit word which be mapped to Plane 0xE1, page 0 or 2, offset 0x04, 0x05, 0x06 and 0x07. Hence in this page, 

   - BmHwStatus [7:0] is mapped to ECRAMx0x04.
   - BmHwStatus [15:8] is mapped to ECRAMx0x05.
   - BmHwStatus [23:16] is mapped to ECRAMx0x06.
   - BmHwStatus [31:24] is mapped to ECRAMx0x07.

eSPI slave capabilities
================================
Part of slave eSPI capabilities are list as below. For test purpose the eSPI slave can report different capabilities so as to influence the master behavior.

Plane ID (ECRAMx31) = 0xE0

Please NOTE that the options in below take can take effect until an EC Watch Dog Timer reset, See General EC register access.

   .. figure:: ESPI_CAP.PNG
      :width: 600px
      :name: ESPI_CAP

Please NOTE the platform may need a full power cycle reset before some of the settings can take effect. See General EC register access for how to perform an EC reset.

For example, to change the default settings espi_mmio_test_bar to 0x10203040, upper software needs to switch Plane ID to 0xE0, i.e. set ECRAMx31 to 0xE0. Then write the field [7:0] first:

   1.	Write ECRAMx31 = 0xE0  // switch Plane to 0xE0
   2.	Write ECRAMx0C = 0x40
   3.	Write ECRAMx0D = 0x30
   4.	Write ECRAMx0E = 0x20
   5.	Write ECRAMx0F = 0x10
   6.	sleep 3 seconds
   7.	Write ECRAMx31 = 0xDB // switch Plane to 0xDB, see General EC register access for details
   8.	Write ECRAMx30 = 0x44 // trigger EC WDT reset so the new applied setting can take effect

eSPI peripheral channel
================================
Plane ID (ECRAMx31) = 0xE1

Before upper software can do bus master test with this interface, it should ensure the bus master enable bit (bit 2) of eSPI global configuration register 0x10 has set to 1. 

For upstream bus master accessing, upper software can change the test parameters as described in table below for different test purpose. However, it has two group of default parameters for quick upstream test. i.e. 

In ch0, Page = 0, set bit 0 of ECRAMx00 to 1 triggers EC to copy its inner buffer (ch0 buffer or MMIO space of espi_mmio_test_bar start from 0x10) to host address 0x04000000

In ch1, Page = 2, set bit 0 of ECRAMx00 to 1 triggers EC to copy host address 0x04000000 to its inner buffer (ch1 buffer or MMIO space of espi_mmio_test_bar start from 0x90)

The length field specify how many bytes will be handled in the bus master transmission.

In addition to the debug interface being exported at Plane ID 0xE1, there is another MMIO space (default at memory 0xFEEC1000, configurable by espi_mmio_test_bar) used as an alias of the EC inner buffer. This MMIO space can be used as downstream MMIO access test.

   .. figure:: peripheral_ch.PNG
      :width: 600px
      :name: peripheral_ch

   .. figure:: peripheral_ch2.PNG
      :width: 600px
      :name: peripheral_ch2

eSPI virtual wires channel
================================
Plane ID (ECRAMx31) = 0xE2

For virtual wires controlling, the whole EC registers of virtual wires are exported into this plane. Upper software can check the VW status or trigger S-to-M VWs by it. There are two kinds of register: M-to-S and S-to-M.

EC has totally 11 M-to-S registers and 7 S-to-M registers, in other word, the EC hardware can monitor at most 11 groups of M-to-S VWs. For example, according to eSPI spec, the PLTRST# signal is bit 1 of index 0x03. “M2S VW Group 1” is assigned for this index. As thus, upper software can check bit 72 (SRC1) of “M2S VW Group 1” to know the current signal status. By checking the mapping table below, bit 72 (SRC1) of “M2S VW Group 1” is at 
Page = 0, Offset = 0x15 - [79:72] of M2S VW Group 1

So upper software should set ECRAMx30 (page selector) to 0 then read ECRAMx15. Bit 0 of the byte read from ECRAMx15 is the status of PLTRST#. Pseudo-code as below

.. code-block:: c
 
   void switch_debug_interface_plane (uint8_t planeId) {
      static uint8_t currentPlane = 0xFF;
      if (planeId != currentPlane) {
         write_ec_ram (0x31, planeId);
         assert (read_ec_ram (0x31) == planeId);
      }
   }

   void switch_page_within_a_plane (uint8_t pageId) {
      if (read_ec_ram (0x30) != pageId) {
         write_ec_ram (0x30, pageId);
      }
   }


   uint32_t read_VW_PLTRSTn (void) {
      switch_debug_interface_plane (0xE2);
      switch_page_within_a_plane (0);

      return (read_ec_ram (0x15) & 0x01) ? 1ul : 0ul;
   }

So read_VW_PLTRSTn() returns 1 means PLTRST# has de-asserted (at HIGH status), read_VW_PLTRSTn() returns 0 means PLTRST# has asserted (at LOW status).

M-to-S VW registers to ECRAM mapping:

   .. figure:: m2s_mapping1.PNG
      :width: 600px
      :name: m2s_mapping1

   .. figure:: m2s_mapping2.PNG
      :width: 600px
      :name: m2s_mapping2

   .. figure:: m2s_mapping3.PNG
      :width: 600px
      :name: m2s_mapping3

S-to-M VW registers to ECRAM mapping:

   .. figure:: s2m_mapping1.PNG
      :width: 600px
      :name: s2m_mapping1

   .. figure:: s2m_mapping2.PNG
      :width: 600px
      :name: s2m_mapping2

   .. figure:: s2m_mapping3.PNG
      :width: 600px
      :name: s2m_mapping3

Upper software can leverage the S-to-M mapping to check current VW status and to change the pin status from slave to master. 
For example, AMD specific VW CPU_TEMP (index 53, bit 0), is assigned to “S2M VW Group 6”, the current VW status is at SRC0, bit32 of S2M VW Group 6, which is mapped to ECRAM page = 0x11, offset 0x24. To generate a pulse on the pin, software can do the following:

.. code-block:: c
      
   void set_VW_CPU_TEMP (uint32_t value) {
   switch_debug_interface_plane (0xE2);
   switch_page_within_a_plane (11);

   uint8_t originalValue = read_ec_ram (0x24);

   if (value) {
      originalValue |= 0x01;   // set the SRCx bit means to set the VW to H
   } else {
      originalValue &= ~ 0x01; // clear the SRCx bit means to set the VW to L
   }

   write_ec_ram (0x24) & 0x01) ? 1ul : 0ul;  // write the byte back
   }
   //
   // Generate a CPU_TEMP pulse
   //
   set_VW_CPU_TEMP (1);  // set CPU_TEMP to high.
   sleep (1000);    // a delay is needed here so the signal can keep in high for a reasonable while.
   set_VW_CPU_TEMP (0);  // set CPU_TEMP to low.

Although the default VW registers assignment can cover all hot VW pins required for RMB, upper software still can change the INDEX field (bit 7:0 of each EC VW registers) to handle VWs from different group. 
Table below lists the FW default register setting on RMB platform.

   .. figure:: vw_pin.PNG
      :width: 600px
      :name: vw_pin

SPI S0 flash sharing
================================
Plane ID (ECRAMx31) = 0xE4

This plane is for runtime SPI flash sharing stress test. 
Although the EC FW is on the system BIOS ROM, but it is loaded before SoC be powered up, 
so there is no actual use case till now for EC to access system BIOS ROM at runtime (SoC in S0). 
As thus this interface is introduced to prove the ROM sharing capability between SoC and EC. 
This stress test is disabled by default and its parameters and status are listed as table below.

   .. figure:: spi_s01.PNG
      :width: 600px
      :name: spi_s01

   .. figure:: spi_s02.PNG
      :width: 600px
      :name: spi_s02

To enable the test, upper software should set SPI address, BlockSize, 
BlockNum with reasonable value, set Misc_Ctrl to enable different test cases. 
Then set TestInterval to a none-zero value to start the test running. 
TestInterval is used as the main factor for different stress strength. 
The test loop will do the Pseudo-code as below.

.. code-block:: c

   while (TestInterval) {         // do the test only if interval is not zero.
      uint32_t startAddr = SPI address;
      uint32_t endAddr = startAddr + BlockSize * BlockNum;

      if (write test is enabled) {
         if not MAFS, Send ATOMIC REQ and wait for GNT;
         Erase the whole region [startAddr, endAddr];
      if not MAFS, Release REQ;

      1ms delay;

      for each block in region [startAddr, endAddr] {
         if not MAFS, Send ATOMIC REQ and wait for GNT;
         Write the block with test pattern; 
         if not MAFS, Release REQ;

   1ms delay;
   }
   }

   for each block in region [startAddr, endAddr] {
      if not MAFS, Send ATOMIC REQ and wait for GNT;
      Read the block test buffer;
      if not MAFS, Release REQ;

      Compare test buffer with test pattern, set Error_Flag if any mismatch; 

   1ms delay;
   }

      // Update statistic data
   TestCycles ++;
   if (Error_Flag)
         ErrorCounter ++;
   }

      TestInterval millisecond delay; // sleep for a while before next cycle
   }

Please NOTE the test parameters can be changed only if TestInterval is set to 0. Once the test loop is started, all parameters will be read-only. Set TestInterval back to 0 can disable the test. EC will make sure the bus is released before it exits the main test loop.

The Debug script is “Rom Armor V3 Stress Toll” for the purpose of EC and MMIO read/write/erase stress testing, please refer to Confluence Page, and the newer script version are Windows version: v1.1.2.0 and UEFI version:v1.5.1.0.
The basic usage: RAv3Stress.efi /ESPI 29000 3 /STRESS 10 it's to run ESPI FA stress at SPI FLASH [29000, 2BFFF] for 10 minutes.

eSPI flash access channel - quick test
================================
Plane ID (ECRAMx31) = 0xE5

The interface definition is in table below and the default setting is

	Flash address = 0x21000, Length = 0x40, Tag = 0, Ops = READ, timeout = no-timeout

Software can override these parameters for different test purpose before set TestTrigger to 1. 
The data buffer be exported at page 1/2/3 has totally 128 bytes. 
And the initial data pattern in the buffer is a sequence number start for 0x80. 
In WRITE test, Software can change the buffer content to test with different data pattern. 
In READ test, Software can check the buffer to confirm if the payload is transmitted correctly or not.

   .. figure:: fa_quick_test1.PNG
      :width: 600px
      :name: fa_quick_test1

   .. figure:: fa_quick_test2.PNG
      :width: 600px
      :name: fa_quick_test2

In addition to the ECRAM based test interface, EC supports another set of quick commands to trigger test,

- Write IOx668 = 0xF0 - Used for filling SPI ROM region with data 0x0 through eSPI FA interface.

- Write IOx668 = 0xFF - Used for filling SPI ROM region with data 0xFF through eSPI FA interface.

- Write IOx668 = 0xFC - Used for filling SPI ROM region with data 0xCC through eSPI FA interface.

- Write IOx668 = 0x72 - Used for filling SPI ROM region with special meaning data through eSPI FA interface.

   Note: The test flash region through FA interface setting.

- Write IOx668 = 0xDD - Disable FA stress test

- Write IOx668 = 0xDE - Enable FA stress test

- Write IOx668 = 0xBB - 120KB E/W/R benchmark (zero interval between requests from EC)

- Write IOx668 = 0xBE - 500KB E/W/R benchmark (zero interval between requests from EC)

   Note: The test flash region is [1000h, 2F00h] and [21000h, 21000h + [120KB/500KB]] with cycle data [0:255] through FA interface setting.

Note #1 Single package / Stress test / benchmark are exclusive and cannot running at same time.

Note #2 Single package test can be triggered repeatedly.

eSPI VW aggregator
================================
Plane ID (ECRAMx31) = 0xA2

The previous section, eSPI virtual wires channel, provides a verbose register list for host to configurate EC inner eSPI VW registers directly. 
This plane provides another option for whom do not need to control too many details. 
It aggregates all M-to-S and S-to-M virtual wires into one page.

   .. figure:: vw_aggregator1.PNG
      :width: 600px
      :name: vw_aggregator1

   .. figure:: vw_aggregator2.PNG
      :width: 600px
      :name: vw_aggregator2

   .. figure:: vw_aggregator3.PNG
      :width: 600px
      :name: vw_aggregator3

EC statistic data
================================
Plane ID (ECRAMx31) = 0xD2

This Plane exports quite a few of counters.

   .. figure:: statistic_data.PNG
      :width: 600px
      :name: statistic_data

Each of the counters are unidirectional increment after EC power on reset. 
The values will be maintained until EC lost power. 
Host can clear these counters to zero by an EC Watch Dog Timer reset. 
See General EC register access, which also result in a full power cycle of the motherboard.