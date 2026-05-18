/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "debug_info.h"
#include "dbgLogFifo.h"
#include <dev_utility.h>
#include "zephyr/kernel_structs.h"

uint32_t g_info_statistics_buf[INFO_NUMBER] = {0};
// static uint32_t s_thread_last_record[INFO_RECORD_THREAD_NUMBER] = {0};
static uint32_t s_thread_last_idx = 0;

/**
 * @brief Thread iteration callback for collecting thread information
 * @param thread Pointer to the thread structure
 * @param user_data User data passed to the callback (unused)
 */
static void _info_thread(const struct k_thread *thread, void *user_data)
{	
	/* TODO: comment out temporarily because zephyr kernel upgrade conflict*/
	
	// uint32_t duration = 0;
	// uint32_t last = k_ticks_to_ms_floor32(thread->rt_stats.last_switched_in);
	// uint32_t execution = k_ticks_to_ms_floor32(thread->rt_stats.stats.execution_cycles);

	// if (s_thread_last_idx++ < INFO_RECORD_THREAD_NUMBER) {
	// 	duration = execution - s_thread_last_record[s_thread_last_idx];
	// }
	// dbgLog_printf("%s: %d %d %d\n", thread->name, thread->base.thread_state, last, duration);
	// s_thread_last_record[s_thread_last_idx] = execution;
}

/**
 * @brief Collect and log thread runtime statistics
 */
void info_thread_collect(void)
{
	static uint32_t s_total_last = 0;

	uint32_t total = 0;
	uint32_t duration = 0;
	k_thread_runtime_stats_t st = {0};
	
	dbgLog_printf("Thread!!!\n");
	s_thread_last_idx = 0;
	k_thread_foreach(_info_thread, NULL);
	k_thread_runtime_stats_all_get(&st);
	total = k_ticks_to_ms_floor32(st.execution_cycles);
	
	duration = total - s_total_last;
	dbgLog_printf("Cur: %s, duration: %d, total: %d, isr: %d\n", _current->name, duration, total,  k_is_in_isr());
	s_total_last = total;
}

/**
 * @brief Increase a statistics counter by a specified value
 * @param id Statistics counter ID
 * @param value Value to add to the counter
 */
void info_value_increase(uint8_t id, uint8_t value)
{
	if (id < INFO_NUMBER) {
		g_info_statistics_buf[id] += value;
	}
}

/**
 * @brief Collect and log all statistics values, then reset counters
 */
void info_value_collect(void)
{
	dbgLog_printf("Statistics!!!\n");
	uint8_t i = INFO_NUMBER - 1;
	do {
		dbgLog_printf("ID%d: %d\n", i, g_info_statistics_buf[i]);
		g_info_statistics_buf[i] = 0;
	} while(i--);
}