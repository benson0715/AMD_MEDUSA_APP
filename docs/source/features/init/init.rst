.. _init:

EC Init
***************
EC Initialization code takes the embedded controller from the reset state to a state where the RTOS can run. 
It usually configures the basic registers and initializes some devices. 
Initial hardware configuration involves setting up the target platform so it can work as required and communicate with system. 

Definitions
================================
-  x86 - Main processors executing the x85 Instruction Set Architecture
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

Feature Description
================================
-	EC code is a real-time operating system (RTOS) that is fast and responsive, manage limited resources and schedule tasks, so they complete on time, and ensure functions are isolated and free of interference from other functions.
-	This section describes EC init. 

Feature Execution Flow
================================
- Main()

   - VCI init to enable VCI Power Button and VCI Charge ACOK
   - Enable  UART for debug if needed
   - Enable SysTick
   - Suspend all tasks except the Main Task

Feature Implementation Details
================================
- Main Task

   - Set the default value of service Init ID and system state for EC function configuration

      .. code-block:: c
      
            DWORD ServiceSource = (1<<Service_Init_ID);     // 32b Tasks Active Flag, 1=Active
            volatile OSPM_Type SystemState =PLATFORM_POWER_ON_RESET; // init SystemState = POR

   - OemGPIO Init

      .. code-block:: c
      
         static uint32_t _s_autogen_PorInitTable[] = {
            POR_INIT_MISC,
            POR_INIT_DBG,
            POR_INIT_GPIO,
            POR_INIT_I2C,
            POR_INIT_JTAG,
            POR_INIT_Keyboard,
            POR_INIT_SYS_BUS,
            POR_INIT_THREMAL,
            GPIO_PIN_NULL
            };

   - Chip Init
 
      - GIRQ_Reset 		// Disable  GIRQ8..GIRQ26 all individual Interrupts.
      - HTMTimer_Init	// Reset(clr) all the counters which base on Hiber Timers.
 
   - OEM Initialization
 
      - Timer init		// Enable HW Timer and set to active status
      - Event init		// Set Event to active status
      - Config I2C port	// Config I2C channels and set their clock speed
      - Read board ID	// Load board ID for other components may depend on it 
      - Init IoExp0		// Init IO expander0, so all pins of this chip will be input
      - Init Espi		
      - Set capabilities 
      - Active Espi logic device
      - Setup all supported VWs      
      - Enable necessary eSPI INTRs 
      - Init flash channel
      - Register OOB package handler
      - Power Sequence init	// Set power sequence nest step to POWER_INIT

         .. code-block:: c
         
            void powerSeqInit ()
            {
               u_pwrSt_init(powerSeq_statusChangeHandler);

            // TODO: Ensure GPIO163(AC_JACK_INn) is input
               // TODO: Ensure PD related GPIOs are input
               powerSeq_nextStep(POWER_INIT, 1);
            }


      - Check PD		         // Check PD FW version whether to update
      - ACPI Interface init	// Config ECRAM interface for ACPI communication
      - Init other tasks and enable task switch	//initProc of each tasks should be done before this point
      - Create handler of one millisecond timer 
      - Power sequence monitor and state machine 
      - Power button monitor

Feature of one millisecond handler
================================
- Power sequence for system power sequence state change
- Button monitor
