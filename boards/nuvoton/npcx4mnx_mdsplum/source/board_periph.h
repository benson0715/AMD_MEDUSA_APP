/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#ifndef __BOARD_ERIPH_H__
#define __BOARD_ERIPH_H__

/* board peripheral init */
int board_periph_init(void);
/* on board ioexp interrpt handler to process the pin status dispatcher */
void board_periph_IoExp_IntDispatcher(void);

#endif /* __BOARD_ERIPH_H__ */
