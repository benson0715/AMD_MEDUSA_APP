/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <errno.h>
#ifdef CONFIG_PINCTRL
#include <zephyr/drivers/pinctrl.h>
#else
#include <zephyr/drivers/pinmux.h>
#endif
#include "system.h"
#include "app_pseq.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "npcx4mnx_pin.h"
#include "board_config.h"
#include "app_acpi.h"
#include "gpioAutoGen.h"


/** @brief EC FW app owned gpios list.
 *
 * This list is not exhaustive, it do not include driver-owned pins,
 * the initialization is done as part of corresponding Zephyr pinmux driver.
 * BSP drivers are responsible to control gpios in soc power transitions and
 * system transitions.
 *
 * Note: Pins not assigned to any app function are identified with their
 * original pin number instead of signal
 *
 */

/* APP-owned gpios */
struct gpio_ec_config AMDNPCX4_CFG[] =  {
/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * PSL_IN2#/PIO       GPIO00             EXT_PURST#         ------------       ------------       ------------       PSL_IN2#          
 * GPIO01/PIO         PSL_IN3#           ------------       ------------       ------------       ------------       ------------      
 * GPIO02/PIO         PSL_IN4#           ------------       ------------       ------------       ------------       ------------      
 * KSO16/PIO          GPIO03             ------------       ------------       ------------       ------------       KSO16             
 * KSO13/PIO          GPIO04             ------------       ------------       ------------       ------------       KSO13             
 * KSO12/PIO          GPIO05             ------------       ------------       ------------       ------------       KSO12             
 * KSO11/PIO          P80_DAT            GPIO06             ------------       ------------       ------------       KSO11             
 * KSO10/PIO          P80_CLK            GPIO07             ------------       ------------       ------------       KSO10             
 ***************************************************************************************************/
    {HW_PWR_BTN_N,       GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO00      
    {LID_CLOSE_N_3V3,    GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO01      
    {GPIO02,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO02      
    {GPIO03,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_OFF         },    // GPIO03      
    {SMSC_KSO_13,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO13       
    {SMSC_KSO_12,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO12       
    {SMSC_KSO_11,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO11       
    {SMSC_KSO_10,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO10       


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * KSO09/PIO          GPIO10             CR_SIN1            ------------       ------------       ------------       KSO09             
 * KSO08/PIO          GPIO11             CR_SOUT1           ------------       ------------       ------------       KSO08             
 * KSO07/PIO          GPIO12             ------------       ------------       ------------       ------------       KSO07             
 * KSO06/PIO          GPIO13             ------------       ------------       ------------       ------------       KSO06             
 * KSO05/PIO          GPIO14             ------------       ------------       ------------       ------------       KSO05             
 * KSO04/O2ma         GPIO15             XNOR               ------------       ------------       ------------       KSO04/O2ma-High   
 * KSO03/PIO          GPIO16             JTAG_TDO0_SWO0     ------------       ------------       ------------       KSO03             
 * KSO02/PIO          GPIO17             JTAG_TDI0          ------------       ------------       ------------       KSO02/O2ma-Low    
 ***************************************************************************************************/
    {SMSC_KSO_9,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO09       
    {SMSC_KSO_8,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO08       
    {SMSC_KSO_7,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO07       
    {SMSC_KSO_6,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO06       
    {SMSC_KSO_5,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO05       
    {SMSC_KSO_4,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO04       
    {SMSC_KSO_3,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO03       
    {SMSC_KSO_2,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO02       


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * KSO01/PIO          GPIO20             JTAG_TMS0_SWIO0    ------------       ------------       ------------       KSO01/O2ma-Low    
 * KSO00/PWR          GPIO21             JTAG_TCK0_SWCLK0   ------------       ------------       ------------       KSO00             
 * KSI7/PIO           GPIO22             AD20               S_SBUA             ------------       ------------       KSI7              
 * KSI6/PIO           GPIO23             AD21               S_SBUB             ------------       ------------       KSI6              
 * KSI5/PIO           GPIO24             AD12               GP_MISO            ------------       ------------       KSI5              
 * KSI4/PIO           GPIO25             AD24               TRACECLK           GP_SCLK            ------------       KSI4              
 * KSI3/PIO           GPIO26             AD13               TRACEDATA0         ------------       ------------       KSI3              
 * KSI2/PIO           GPIO27             TRACEDATA21        AD14               ------------       ------------       KSI2/O2ma-Low     
 ***************************************************************************************************/
    {SMSC_KSO_1,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO01       
    {SMSC_KSO_0,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO00       
    {SMSC_KSI_7,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI7        
    {SMSC_KSI_6,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI6        
    {SMSC_KSI_5,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI5        
    {SMSC_KSI_4,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI4        
    {SMSC_KSI_3,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI3        
    {SMSC_KSI_2,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI2        


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * KSI1/ANA           GPIO30             AD25               TRACEDATA2         GP_CS#             ------------       KSI1              
 * KSI0/PIO           GPIO31             AD15               TRACEDATA3         GP_MOSI            ------------       KSI0              
 * GPIO32/PIO         TRIS#              ------------       ------------       ------------       ------------       ------------      
 * GPIO33/PIO         I2C5_SCL0          CTS#               ------------       ------------       ------------       ------------      
 * GPIO34/PIO         PS2_DAT2           AD6                ------------       ------------       ------------       ------------      
 * GPIO35/PIO         CR_SOUT4           TEST#              ------------       ------------       ------------       ------------      
 * GPIO36/PIO         I2C5_SDA0          RTS#               ------------       ------------       ------------       ------------      
 * GPIO37/PIO         GPIO37             AD5                ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {SMSC_KSI_1,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI1        
    {SMSC_KSI_0,         GPIO_INPUT | GPIO_INT_EDGE_FALLING | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSI0        
    {GPIO32,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO32      
    {SYS_RST_N,          GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIO33      
    {EC_F4_LED,          GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO34      
    {GPIO35,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO35      
    {EXT_TALERT_N,       GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO36      
    {EC_KB_BL_LED,       GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO37      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO40/PIO         TA1                ------------       ------------       ------------       ------------       ------------      
 * GPIO41/PIO         AD4                ------------       ------------       ------------       ------------       ------------      
 * GPIO42/PIO         AD3                RI#                ------------       ------------       ------------       ------------      
 * GPIO43/I           AD2                ------------       ------------       ------------       ------------       ------------      
 * GPIO44/PIO         AD1                ------------       ------------       ------------       ------------       ------------      
 * GPIO45/PIO         AD0                ------------       ------------       ------------       ------------       ------------      
 * LAD0/PIO           eSPI_IO0           GPIO46             ------------       ------------       ------------       LAD0              
 * LAD1/PIO           eSPI_IO1           GPIO47             ------------       ------------       ------------       LAD1              
 ***************************************************************************************************/
    /* GPIO40 - Not configured, reserved */
    {EC_CHG_ORG_LED,     GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO41      
    {WLAN_IO33_N_IO18_SEL, GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO42      
    {EC_CHG_GREEN_LED,   GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO43      
    {EC_CHG_RED_LED,     GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO44      
    {EC_RTCRST_ON,       GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO45      
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_IO0    
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_IO1    


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO50/PWR         I3C2_SDA           ------------       ------------       ------------       ------------       ------------      
 * LAD2/PIO           eSPI_IO2           GPIO51             ------------       ------------       ------------       LAD2              
 * LAD3/PIO           eSPI_IO3           GPIO52             ------------       ------------       ------------       LAD3              
 * LFRAME#/PIO        eSPI_CS#           GPIO53             SHI_CS#            ------------       ------------       LFRAME#           
 * LRESET#/PIO        eSPI_RST#          GPIO54             ------------       ------------       ------------       LRESET#           
 * PCI_CLK/PIO        eSPI_CLK           GPIO55             SHI_SCLK           ------------       ------------       PCI_CLK           
 * GPIO56/PIO         CLKRUN#            I3C2_SCL           ------------       ------------       ------------       ------------      
 * SER_IRQ/PIO        eSPI_ALERT#        GPIO57             ------------       ------------       ------------       SER_IRQ           
 ***************************************************************************************************/
    {SPI_APU_EC_ROM_GNT, GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_OFF         },    // GPIO50      
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_IO2    
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_IO3    
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_CS#    
    {ESPI_EC_RESET_N,    GPIO_INPUT,                                                 GPIO_CTRL_PWRG_OFF         },    // eSPI_RST#   
    {ESPI_EC_CLK,        GPIO_INPUT,                                                 GPIO_CTRL_PWRG_OFF         },    // eSPI_CLK    
    {SPI_EC_APU_ROM_REQ, GPIO_INPUT | GPIO_PULL_DOWN,                                GPIO_CTRL_PWRG_VTR_IO      },    // GPIO56      
    {ESPI_EC_ALERT_N,    GPIO_INPUT | GPIO_PULL_UP,                                  GPIO_CTRL_PWRG_OFF         },    // eSPI_ALERT# 


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO60/PWR         PWM7               ------------       ------------       ------------       ------------       ------------      
 * GPIO61/PIO         PWROFF#            ------------       ------------       ------------       ------------       ------------      
 * GPIO62/PIO         PS2_CLK1           AD16               ------------       ------------       ------------       ------------      
 * GPIO63/PIO         PS2_DAT1           AD17               INTRUDER2#         ------------       ------------       ------------      
 * GPIO64/PIO         CR_SIN1            ------------       ------------       ------------       ------------       ------------      
 * GPIO65/PIO         CR_SOUT1           FLPRG1#            ------------       ------------       ------------       ------------      
 * GPIO66/PIO         ------------       ------------       ------------       ------------       ------------       ------------      
 * GPIO67/PIO         PS2_CLK0           AD18               ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {FPR_EC_PWRBTN_EVENT_MASK_N, GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO60      
    {EC_SLP_S5_N,        GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO61      
    {CAM_DET_USB2_N_EV1, GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO62      
    {EC_WLAN_AUX_RST_N,  GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_OFF         },    // GPIO63      
    /* GPIO64 - Not configured, reserved */
    /* GPIO65 - Not configured, reserved */
    {EC_ARM,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO66      
    {EC_USB32_RD_EN,     GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO67      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO70/PWR         PS2_DAT0           AD19               ------------       ------------       ------------       ------------      
 * PWRGD/PIO          GPIO72             ------------       ------------       ------------       ------------       PWRGD             
 * GPIO73/PIO         TA2                ------------       ------------       ------------       ------------       ------------      
 * GPIO74/PIO         AD23               ------------       ------------       ------------       ------------       ------------      
 * GPIO75/PIO         32KHZ_OUT          I2C4_SCL0#         RXD                CR_SIN2            ------------       ------------      
 * GPIO76/PIO         EC_SCI#            ------------       ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {EC_USB32_RD_RST_N,  GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN,                          GPIO_CTRL_PWRG_VTR_IO      },    // GPIO70      
    {BAT_PRSNT_N,        GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO72      
    {MPM_EVENT_N,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_OFF         },    // GPIO73      
    {EC_SLP_S3_S0A3_N,   GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO74      
    /* GPIO75 - Not configured, reserved */
    {EC_APU_SCI_N,       GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIO76      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO80/PIO         PWM3               ------------       ------------       ------------       ------------       ------------      
 * PECI_DATA/PIO      GPIO81             ------------       ------------       ------------       ------------       PECI_DATA         
 * KSO14/I            GPIO82             ------------       ------------       ------------       ------------       KSO14             
 * KSO15/PIO          GPIO83             ------------       ------------       ------------       ------------       KSO15             
 * GPIO86/PIO         TXD                CR_SOUT2           ------------       ------------       ------------       ------------      
 * GPIO87/PIO         I2C1_SDA0          ------------       ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {APU_THERMTRIP_N,    GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_OFF         },    // GPIO80      
    {EC_TPAD_I3C_I2C_N_SEL, GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIO81      
    {SMSC_KSO_14,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO14       
    {SMSC_KSO_15,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN | GPIO_PULL_UP,          GPIO_CTRL_PWRG_VTR_IO      },    // KSO15       
    /* GPIO86 - Not configured, reserved */
    {EC_I2C1_SDA_3V3_S5, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C1_SDA0   


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIO90/PIO         I2C1_SCL0          ------------       ------------       ------------       ------------       ------------      
 * GPIO91/PIO         I2C2_SDA0          ------------       ------------       ------------       ------------       ------------      
 * GPIO92/PIO         I2C2_SCL0          ------------       ------------       ------------       ------------       ------------      
 * GPIO93/PIO         TA1                F_DIO2             FLM_DI2            ------------       ------------       ------------      
 * GPIO94/PIO         ------------       ------------       ------------       ------------       ------------       ------------      
 * SPIP_MISO/PIO      GPIO95             ------------       ------------       ------------       ------------       SPIP_MISO         
 * F_DIO1/PWR         FLM_DI1            GPIO96             ------------       ------------       ------------       F_DIO1            
 * GPIO97/PIO         ------------       ------------       ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {EC_I2C1_SCL_3V3_S5, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C1_SCL0   
    {EC_I2C2_SDA_1V8_S5, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C2_SDA0   
    {EC_I2C2_SCL_1V8_S5, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C2_SCL0   
    {GPIO93,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIO93      
    {RSMRST_N,           GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO94      
    {EC_CODEC_AUX_MODE_EN_N, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIO95      
    {SPI_EC_ROM_D1,      GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // FLM_DI1     
    {APU_PWROK,          GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIO97      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * F_CS0#/PIO         FLM_CSIO#          GPIOA0             ------------       ------------       ------------       F_CS0#            
 * SPIP_SCLK/PIO      GPIOA1             ------------       ------------       ------------       ------------       SPIP_SCLK         
 * F_SCLK/PWR         FLM_SCLK           GPIOA2             ------------       ------------       ------------       F_SCLK            
 * SPIP_MOS/PIO       GPIOA3             ------------       ------------       ------------       ------------       SPIP_MOS          
 * F_DIO0/PIO         FLM_DI0            GPIOA4             TB1                ------------       ------------       F_DIO0            
 * GPIOA5/PIO         ------------       ------------       ------------       ------------       ------------       ------------      
 * GPIOA6/PWR         PS2_CLK3           TA2                F_CS1#             ------------       ------------       ------------      
 * GPIOA7/PIO         PS2_DAT3           TB2                F_DIO3             FLM_DI3            ------------       ------------      
 ***************************************************************************************************/
    {SPI_EC_ROM_CS0_N,   GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // FLM_CSIO#   
    {GPIOA1,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOA1      
    {SPI_EC_ROM_CLK,     GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // FLM_SCLK    
    {GPIOA3,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOA3      
    {SPI_EC_ROM_D0,      GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // FLM_DI0     
    {EC_PWR_BTN_N,       GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIOA5      
    {EC_CODEC_MIC_HW_MUTE_N, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIOA6      
    {GPIOA7,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOA7      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIOB0/PIO         ------------       ------------       ------------       ------------       ------------       ------------      
 * KSO17/PIO          GPIOB1             CR_SIN4            ------------       ------------       ------------       KSO17             
 * GPIOB2/PIO         I2C7_SDA0          DSR#               ------------       ------------       ------------       ------------      
 * GPIOB3/PIO         I2C7_SCL0          DCD#               ------------       ------------       ------------       ------------      
 * GPIOB4/PIO         I2C0_SDA0          ------------       ------------       ------------       ------------       ------------      
 * GPIOB5/PIO         I2C_SCL0           ------------       ------------       ------------       ------------       ------------      
 * GPIOB6/PIO         PWM4               GP_SEL#            ------------       ------------       ------------       ------------      
 * GPIOB7/PIO         PWM5               I2C7_SDA1          S_SBUB             ------------       ------------       ------------      
 ***************************************************************************************************/
    {EC_TPNL_I3C_I2C_N_SEL, GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB0      
    {TV_EN,              GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB1      
    {SLP_S5_N,           GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB2      
    {APU_EC_ALERT_N,     GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB3      
    {EC_I2C0_SDA_3V3_S0, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C0_SDA0   
    {EC_I2C0_SCL_3V3_S0, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C0_SCL0   
    {EC_BLINK_N,         GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB6      
    {SLP_S3_S0A3_N,      GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOB7      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIOC0/PIO         PWM6               I2C7_SCL1          S_SBUA             ------------       ------------       ------------      
 * GPIOC1/PIO         I2C6_SDA0          AD22               ------------       ------------       ------------       ------------      
 * GPIOC2/PIO         PWM1               I2C6_SCL0          ------------       ------------       ------------       ------------      
 * GPIOC3/PWR         PWM0               ------------       ------------       ------------       ------------       ------------      
 * GPIOC4/PIO         PWM2               ------------       ------------       ------------       ------------       ------------      
 * GPIOC5/PIO         KBRST#             ------------       ------------       ------------       ------------       ------------      
 * GPIOC6/PWR         SMI#               ------------       ------------       ------------       ------------       ------------      
 * GPIOC7/PWR         DTR_BOUT           AD11               ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {EC_FPR_OP_BOOT_LOGIN_N, GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC0      
    {EC_APU_PROCHOT_N,   GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC1      
    {EC_CAP_LED,         GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC2      
    {EC_PWM0,            GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN,                          GPIO_CTRL_PWRG_VTR_IO      },    // PWM0        
    {EC_PWM1,            GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN,                          GPIO_CTRL_PWRG_VTR_IO      },    // PWM2        
    {SYSTEM_S5_PG,       GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC5      
    {EC_FPR_RST_N,       GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC6      
    {GPIOC7,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOC7      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIOD0/PWR         I2C3_SDA0          ------------       ------------       ------------       ------------       ------------      
 * GPIOD1/PWR         I2C3_SCL0          ------------       ------------       ------------       ------------       ------------      
 * PSL_IN1#/PIO       GPIOD2             ------------       ------------       ------------       ------------       PSL_IN1#          
 * GPIOD3/PIO         TB1                ------------       ------------       ------------       ------------       ------------      
 * GPIOD4/PIO         CR_SIN3            JTAG_TDO1_SWO1     ------------       ------------       ------------       ------------      
 * GPIOD5/PIO         INTRUDER1#         JTAG_TCK1_SWCLK1   ------------       ------------       ------------       ------------      
 * GPIOD6/PIO         CR_SOUT3           FLM_CSI#           ------------       ------------       ------------       ------------      
 * PSL_GPO/PIO        GPIOD7             ------------       ------------       ------------       ------------       PSL_GPO           
 ***************************************************************************************************/
    {EC_I2C3_SDA_3V3_EC, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C3_SDA0   
    {EC_I2C3_SCL_3V3_EC, GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,                         GPIO_CTRL_PWRG_VTR_IO      },    // I2C3_SCL0   
    {CHG_ACOK,           GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOD2      
    /* GPIOD3 - Not configured, reserved */
    {EC_JTAG_TDO_SWO,    GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOD4      
    {EC_JTAG_TCK_SWCLK,  GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOD5      
    {DBG_CARD_PRSNT_N,   GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOD6      
    {GPIOD7,             GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOD7      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIOE0/PIO         AD10               ------------       ------------       ------------       ------------       ------------      
 * GPIOE1/PIO         AD7                ------------       ------------       ------------       ------------       ------------      
 * GPIOE2/PIO         JTAG_TDI1          ------------       ------------       ------------       ------------       ------------      
 * GPIOE3/PIO         I2C6_SDA1          I3C1_SDA           ------------       ------------       ------------       ------------      
 * GPIOE4/PIO         I2C6_SCL1          I3C1_SCL           ------------       ------------       ------------       ------------      
 * GPIOE5/PIO         JTAG_TMS1_SWIO1    ------------       ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {CHG_EC_PROCHOT_N,   GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOE0      
    {EC_ADC_RSVD,        GPIO_INPUT,                                                 GPIO_CTRL_PWRG_OFF         },    // GPIOE1      
    {EC_JTAG_TDI,        GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOE2      
    {EC_S5_PWREN,        GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOE3      
    {EC_1V8_S5_EN,       GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIOE4      
    {EC_JTAG_TMS_SWIO,   GPIO_INPUT,                                                 GPIO_CTRL_PWRG_VTR_IO      },    // GPIOE5      


/***************************************************************************************************
 * FUN/PAD                                                                                                            default     
 * GPIOF0/PIO         AD9                ------------       ------------       ------------       ------------       ------------      
 * GPIOF1/I           AD8                ------------       ------------       ------------       ------------       ------------      
 * GPIOF2/PIO         I2C4_SDA1          ------------       ------------       ------------       ------------       ------------      
 * GPIOF3/PIO         I2C4_SCL1          ------------       ------------       ------------       ------------       ------------      
 * GPIOF4/VBAT        I2C5_SDA1          I3C3_SDA           ------------       ------------       ------------       ------------      
 * GPIOF5/PIO         I2C5_SCL1          I3C3_SCL           ------------       ------------       ------------       ------------      
 ***************************************************************************************************/
    {PD_PRSNT_N,         GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF0      
    {EC_S0_LED,          GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF1      
    {EC_IOT_3V3_SENSE_EN, GPIO_OUTPUT_LOW,                                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF2      
    {PD0_EC_USBC_INT_N,  GPIO_INPUT | GPIO_INT_EDGE_BOTH,                            GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF3      
    {APU_EC_RESET_N,     GPIO_INPUT | GPIO_INT_EDGE_BOTH | GPIO_PULL_UP,             GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF4      
    {KBRST_N,            GPIO_OUTPUT_HIGH,                                           GPIO_CTRL_PWRG_VTR_IO      },    // GPIOF5      

};

/**
 * @brief Initialize EC GPIO configuration
 *
 * Configures all application-owned GPIO pins defined in AMDNPCX4_CFG array.
 *
 * @return 0 on success, negative errno on failure
 */
int __autoGen_initECGPIO (void )
{
    int ret = 0;
	ret = gpio_configure_array(AMDNPCX4_CFG, ARRAY_SIZE(AMDNPCX4_CFG));
    return ret;
}
/***********************************************************************
 *                                                                     *
 * Runtime GPIO settings                                               *
 *                                                                     *
 ***********************************************************************/

struct gpio_ec_config _s_AMDNPCX4_cfg_rtG3[] = {
    GPIO_RUNTIME_SWITCH_G3
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS5S4[] = {
    GPIO_RUNTIME_SWITCH_S5S4
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS3[] = {
    GPIO_RUNTIME_SWITCH_S3
};
struct gpio_ec_config _s_AMDNPCX4_cfg_rtS0[] = {
    GPIO_RUNTIME_SWITCH_S0
};

/**
 * @brief Switch GPIO configuration based on system power state
 *
 * Reconfigures GPIO pins according to the target power state.
 *
 * @param pwr_state Target system power state
 */
void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state ) {
    switch (pwr_state) {
        case SYSTEM_G3_STATE:   
			gpio_configure_array(_s_AMDNPCX4_cfg_rtG3, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtG3));   
			break;
        case SYSTEM_S4_STATE:
        case SYSTEM_S5_STATE:
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS5S4, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS5S4)); 
			break;
        case SYSTEM_S3_STATE:   
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS3, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS3));   
			break;
        case SYSTEM_S0_STATE:
			gpio_configure_array(_s_AMDNPCX4_cfg_rtS0, ARRAY_SIZE(_s_AMDNPCX4_cfg_rtS0));
			break;  
        default:                                                   
			break;
    }
}

/***********************************************************************
 *                                                                     *
 * EC ACPI Space                                                       *
 *                                                                     *
 ***********************************************************************/

/**
 * @brief ACPI handler for GPIO bank 0
 *
 * Handles read/write operations for GPIO bank 0 ACPI register.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void __autogen_AcpiHandler_GIO0 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint8_t retVal = 0;
    if (ui8Idx != ACPI_GPIO_BASE + 3) return;

    if (isRead) {
        retVal |= gpio_read_pin( WLAN_IO33_N_IO18_SEL ) ? 0x01 : 0x00;	// GPIO42/AD3/RI#
        retVal |= gpio_read_pin( DBG_CARD_PRSNT_N )     ? 0x04 : 0x00;	// GPIOD6/CR_SOUT3/FLM_CSI#
        retVal |= gpio_read_pin( PD_PRSNT_N )           ? 0x10 : 0x00;	// GPIOF0/AD9
        retVal |= gpio_read_pin( CHG_EC_PROCHOT_N )     ? 0x20 : 0x00;	// GPIOE0/AD10
        retVal |= gpio_read_pin( APU_EC_ALERT_N )       ? 0x40 : 0x00;	// GPIOB3/I2C7_SCL0/DCD#

        if (pui8Data) *pui8Data = retVal;
    } else if (!pui8Data){
        return;
    } else {
        retVal = *pui8Data;

        gpio_write_pin( WLAN_IO33_N_IO18_SEL ,(retVal & 0x01) ? 1 : 0 );	// GPIO42/AD3/RI#
        gpio_write_pin( DBG_CARD_PRSNT_N ,    (retVal & 0x04) ? 1 : 0 );	// GPIOD6/CR_SOUT3/FLM_CSI#
        gpio_write_pin( PD_PRSNT_N ,          (retVal & 0x10) ? 1 : 0 );	// GPIOF0/AD9
        gpio_write_pin( CHG_EC_PROCHOT_N ,    (retVal & 0x20) ? 1 : 0 );	// GPIOE0/AD10
        gpio_write_pin( APU_EC_ALERT_N ,      (retVal & 0x40) ? 1 : 0 );	// GPIOB3/I2C7_SCL0/DCD#
    }
}

/**
 * @brief ACPI handler for GPIO bank 1
 *
 * Handles read/write operations for GPIO bank 1 ACPI register.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void __autogen_AcpiHandler_GIO1 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint8_t retVal = 0;
    if (ui8Idx != ACPI_GPIO_BASE + 4) return;

    if (isRead) {
        retVal |= gpio_read_pin( SYSTEM_S5_PG )         ? 0x01 : 0x00;	// GPIOC5/KBRST#
        retVal |= gpio_read_pin( TV_EN )                ? 0x02 : 0x00;	// KSO17/GPIOB1/CR_SIN4
        retVal |= gpio_read_pin( EC_TPNL_I3C_I2C_N_SEL ) ? 0x08 : 0x00;	// GPIOB0
        retVal |= gpio_read_pin( EC_TPAD_I3C_I2C_N_SEL ) ? 0x10 : 0x00;	// PECI_DATA/GPIO81

        if (pui8Data) *pui8Data = retVal;
    } else if (!pui8Data){
        return;
    } else {
        retVal = *pui8Data;

        gpio_write_pin( SYSTEM_S5_PG ,        (retVal & 0x01) ? 1 : 0 );	// GPIOC5/KBRST#
        gpio_write_pin( TV_EN ,               (retVal & 0x02) ? 1 : 0 );	// KSO17/GPIOB1/CR_SIN4
        gpio_write_pin( EC_TPNL_I3C_I2C_N_SEL ,(retVal & 0x08) ? 1 : 0 );	// GPIOB0
        gpio_write_pin( EC_TPAD_I3C_I2C_N_SEL ,(retVal & 0x10) ? 1 : 0 );	// PECI_DATA/GPIO81
    }
}

/**
 * @brief ACPI handler for GPIO bank 2
 *
 * Handles read/write operations for GPIO bank 2 ACPI register.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void __autogen_AcpiHandler_GIO2 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint8_t retVal = 0;
    if (ui8Idx != ACPI_GPIO_BASE + 5) return;

    if (isRead) {
        retVal |= gpio_read_pin( EC_FPR_RST_N )         ? 0x04 : 0x00;	// GPIOC6/SMI#
        retVal |= gpio_read_pin( EC_KB_BL_LED )         ? 0x20 : 0x00;	// GPIO37/PS2_CLK2/AD5
        retVal |= gpio_read_pin( EC_F4_LED )            ? 0x40 : 0x00;	// GPIO34/PS2_DAT2/AD6

        if (pui8Data) *pui8Data = retVal;
    } else if (!pui8Data){
        return;
    } else {
        retVal = *pui8Data;

        gpio_write_pin( EC_FPR_RST_N ,        (retVal & 0x04) ? 1 : 0 );	// GPIOC6/SMI#
        gpio_write_pin( EC_KB_BL_LED ,        (retVal & 0x20) ? 1 : 0 );	// GPIO37/PS2_CLK2/AD5
        gpio_write_pin( EC_F4_LED ,           (retVal & 0x40) ? 1 : 0 );	// GPIO34/PS2_DAT2/AD6
    }
}

/**
 * @brief ACPI handler for GPIO bank 3
 *
 * Handles read/write operations for GPIO bank 3 ACPI register.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void __autogen_AcpiHandler_GIO3 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint8_t retVal = 0;
    if (ui8Idx != ACPI_GPIO_BASE + 6) return;

    if (isRead) {
        retVal |= gpio_read_pin( FPR_EC_PWRBTN_EVENT_MASK_N ) ? 0x04 : 0x00;	// GPIO60/PWM7
        retVal |= gpio_read_pin( EC_IOT_3V3_SENSE_EN )  ? 0x08 : 0x00;	// GPIOF2/I2C4_SDA1
        retVal |= gpio_read_pin( CAM_DET_USB2_N_EV1 )   ? 0x10 : 0x00;	// GPIO62/PS2_CLK1/AD16
        retVal |= gpio_read_pin( EC_WLAN_AUX_RST_N )    ? 0x20 : 0x00;	// GPIO63/PS2_DAT1/AD17/INTRUDER2#
        retVal |= gpio_read_pin( EC_CODEC_MIC_HW_MUTE_N ) ? 0x80 : 0x00;	// GPIOA6/PS2_CLK3/TA2/F_CS1#

        if (pui8Data) *pui8Data = retVal;
    } else if (!pui8Data){
        return;
    } else {
        retVal = *pui8Data;

        gpio_write_pin( FPR_EC_PWRBTN_EVENT_MASK_N ,(retVal & 0x04) ? 1 : 0 );	// GPIO60/PWM7
        gpio_write_pin( EC_IOT_3V3_SENSE_EN , (retVal & 0x08) ? 1 : 0 );	// GPIOF2/I2C4_SDA1
        gpio_write_pin( CAM_DET_USB2_N_EV1 ,  (retVal & 0x10) ? 1 : 0 );	// GPIO62/PS2_CLK1/AD16
        gpio_write_pin( EC_WLAN_AUX_RST_N ,   (retVal & 0x20) ? 1 : 0 );	// GPIO63/PS2_DAT1/AD17/INTRUDER2#
        gpio_write_pin( EC_CODEC_MIC_HW_MUTE_N ,(retVal & 0x80) ? 1 : 0 );	// GPIOA6/PS2_CLK3/TA2/F_CS1#
    }
}

/**
 * @brief ACPI handler for GPIO bank 4
 *
 * Handles read/write operations for GPIO bank 4 ACPI register.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void __autogen_AcpiHandler_GIO4 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint8_t retVal = 0;
    if (ui8Idx != ACPI_GPIO_BASE + 7) return;

    if (isRead) {
        retVal |= gpio_read_pin( CHG_ACOK )             ? 0x01 : 0x00;	// PSL_IN1#/GPIOD2
        retVal |= gpio_read_pin( EC_CODEC_AUX_MODE_EN_N ) ? 0x02 : 0x00;	// SPIP_MISO/GPIO95
        retVal |= gpio_read_pin( LID_CLOSE_N_3V3 )      ? 0x04 : 0x00;	// GPIO01/PSL_IN3#
        retVal |= gpio_read_pin( BAT_PRSNT_N )          ? 0x08 : 0x00;	// PWRGD/GPIO72
        retVal |= gpio_read_pin( EC_FPR_OP_BOOT_LOGIN_N ) ? 0x10 : 0x00;	// GPIOC0/PWM6/I2C7_SCL1/S_SBUA
        retVal |= gpio_read_pin( EC_CAP_LED )           ? 0x80 : 0x00;	// GPIOC2/PWM1/I2C6_SCL0

        if (pui8Data) *pui8Data = retVal;
    } else if (!pui8Data){
        return;
    } else {
        retVal = *pui8Data;

        gpio_write_pin( CHG_ACOK ,            (retVal & 0x01) ? 1 : 0 );	// PSL_IN1#/GPIOD2
        gpio_write_pin( EC_CODEC_AUX_MODE_EN_N ,(retVal & 0x02) ? 1 : 0 );	// SPIP_MISO/GPIO95
        gpio_write_pin( LID_CLOSE_N_3V3 ,     (retVal & 0x04) ? 1 : 0 );	// GPIO01/PSL_IN3#
        gpio_write_pin( BAT_PRSNT_N ,         (retVal & 0x08) ? 1 : 0 );	// PWRGD/GPIO72
        gpio_write_pin( EC_FPR_OP_BOOT_LOGIN_N ,(retVal & 0x10) ? 1 : 0 );	// GPIOC0/PWM6/I2C7_SCL1/S_SBUA
        gpio_write_pin( EC_CAP_LED ,          (retVal & 0x80) ? 1 : 0 );	// GPIOC2/PWM1/I2C6_SCL0
    }
}


/**
 * @brief Main ACPI GPIO handler dispatcher
 *
 * Routes ACPI GPIO read/write requests to appropriate bank handlers.
 *
 * @param isRead Non-zero for read operation, zero for write
 * @param ui8Idx ACPI register index
 * @param pui8Data Pointer to data buffer
 */
void Board_Gpio_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	switch (ui8Idx)
	{
		case ACPI_GPIO_BASE:

			break;
		case ACPI_GPIO_BASE+1:

			break;
		case ACPI_GPIO_BASE+2:

			break;
		case ACPI_GPIO_BASE+3:
			__autogen_AcpiHandler_GIO0(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+4:
			__autogen_AcpiHandler_GIO1(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+5:
			__autogen_AcpiHandler_GIO2(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+6:
 			__autogen_AcpiHandler_GIO3(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+7:
			__autogen_AcpiHandler_GIO4(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+8:
			board_ioexp_AcpiHandler_IOExp10(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+9:
			board_ioexp_AcpiHandler_IOExp11(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+10:
			board_ioexp_AcpiHandler_IOExp0(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+11:
			board_ioexp_AcpiHandler_IOExp1(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+12:
			board_ioexp_AcpiHandler_IOExp2(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+13:
			board_ioexp_AcpiHandler_IOExp3(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+14:
			board_ioexp_AcpiHandler_IOExp4(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+15:
			board_ioexp_AcpiHandler_IOExp5(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+16:
			board_ioexp_AcpiHandler_IOExp6(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+17:
			board_ioexp_AcpiHandler_IOExp7(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+18:
			board_ioexp_AcpiHandler_IOExp8(isRead,ui8Idx,pui8Data);
			break;
		case ACPI_GPIO_BASE+19:
			board_ioexp_AcpiHandler_IOExp9(isRead,ui8Idx,pui8Data);
			break;
		}

}