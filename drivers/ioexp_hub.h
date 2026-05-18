/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __IOEXP_HUB_H__
#define __IOEXP_HUB_H__

#include <stdint.h>
#include <stdbool.h>
#include "ioexp_hub.h"
#include "dev_pca9535.h"
#include "board_ioexpTable.h"
#include "board_config.h"
/*
 * Below MACROs taking effect only for IO Expander pins
 */


/*
 * IOEXP Pin number encoding
 *
 * [31:25] - SN             7-bit
 * [24:21] - I2C port       4-bit
 * [20:18] - I2C speed      3-bit
 *           0 - 1M; 1 - 400K; 2 - 100K; 3 - 85K; 4 - 75K; 5 - 65K; 6 - 50K
 * [17]    - reserved
 * [16]    - POR setting  (HIGH/LOW)
 * [15:14] - Additional pin type (Native/Dummy/Null/IoExp)
 * [13:7]  - I2C slave addr 7-bit
 * [6:5]   - 2'b00 Input; 
 *           2'b01 Input+INT_EN;
 *           2'b10 Output
 *           2'b11 Open Drain
 * [4:0]   - PIN index      5-bit
 */

#ifndef IOEXP_SETBIT
#define IOEXP_SETBIT            dev_pca9535_setBit
#endif
#ifndef IOEXP_SETOUTPUT
#define IOEXP_SETOUTPUT         dev_pca9535_setOutput
#endif
#ifndef IOEXP_SMARTREAD
#define IOEXP_SMARTREAD         dev_pca9535_smartRead
#endif

#define IOEXP_I2cPort(pin)          (((pin) >> 21) & 0x0F)
#define IOEXP_I2cSlvAddr(pin)       (((pin) >>  7) & 0x7F)
#define IOEXP_IdxOfGroup(pin)       ((pin) & 0x1F)
#define IOEXP_BitMask(pin)          (1ul << IOEXP_IdxOfGroup(pin))
#define IOEXP_IsInputPin(pin)       (!(((pin) >> 6) & 0x01))       // Not INPUT means the drive type is either OD or Pp
#define IOEXP_IsOpenDrainPin(pin)   ((((pin) >> 5) & 0x03) == 3)   // OD Output
#define IOEXP_IsPushPullPin(pin)    ((((pin) >> 5) & 0x03) == 2)   // PP Output
#define IOEXP_PORSeting(pin)        (((pin) >> 16) & 0x01)
#define IOEXP_IsPorSetLow(pin)      (!(IOEXP_PORSeting(pin)))

#define IOEXP_PinName(pin)          _dbg_NET_NAME(pin)

#define IOEXP_BatchStart            do {uint8_t _bs_slv = 0xFF; uint8_t _bs_port = 0xFF; uint32_t _bs_mask = 0; uint32_t _bs_val = 0
#define IOEXP_BatchSet(pin, val)    do {                                                         \
                                      if (0xFF == _bs_slv)                                       \
                                        _bs_slv = IOEXP_I2cSlvAddr(pin);                         \
                                      else if (_bs_slv != IOEXP_I2cSlvAddr(pin))                 \
                                        assert(0); /* BatchSet can take effect on same group */  \
																			if (0xFF == _bs_port)                                      \
                                        _bs_port = IOEXP_I2cPort(pin);                           \
                                      else if (_bs_port != IOEXP_I2cPort(pin))                   \
                                        assert(0); /* BatchSet can take effect on same group */  \
                                      _bs_mask |= IOEXP_BitMask(pin);                            \
                                      _bs_val |= ((val) ? IOEXP_BitMask(pin) : 0);               \
                                    } while (0)

#define IOEXP_BatchEnd              if (_bs_slv != 0xFF && _bs_port != 0xFF)                     \
                                    {IOEXP_SETOUTPUT(_bs_slv, _bs_port, _bs_mask, _bs_val);}     \
                                    } while (0)
/* ioexp pin type */
typedef enum {
	IOEXP_GPIO_INPUT = 0,
	IOEXP_GPIO_UNKNOWN = (1 << 8),
	IOEXP_GPIO_OUTPUT = (2 << 8),
	IOEXP_GPIO_OPEN_DRAIN = (3 << 8),
} IOEXP_GPIO_TYPE;

/* ioexp port init */
void ioexp_por_init(uint32_t pin);
/* ioexp reset config */
void ioexp_reset(uint32_t pin);
/* ioexp get pin status */
uint32_t ioexp_get(uint32_t pin);
/* ioexp set pin status */
void ioexp_set(uint32_t pin, uint32_t val);
/* ioexp flip pin status */
void ioexp_flip(uint32_t pin);
/* ioexp set pin type */
void ioexp_set_type(uint32_t pin, IOEXP_GPIO_TYPE type);

#endif // end of __IOEXP_HUB_H__
