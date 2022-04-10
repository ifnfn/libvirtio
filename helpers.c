/******************************************************************************
 * Copyright (c) 2020 Hesham Almatary
 * See LICENSE_CHERI for license details.
 *****************************************************************************/

/******************************************************************************
 * Copyright (c) 2007, 2012, 2013 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/
/*
 * All functions concerning interface to slof
 */

#include <stdio.h>
#include <stdlib.h>
#include <core/helpers.h>

#include "helpers.h"

#if 1
// #include <FreeRTOS.h>
// #include "task.h"

// #define pdUS_TO_TICKS( xTimeInUs ) ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInUs ) * ( TickType_t ) configTICK_RATE_HZ ) / ( TickType_t ) 1000000 ) )

struct cma *slof_cma;
#define CMA_SIZE 0x200000ul

void *SLOF_alloc_mem(size_t size)
{
    return malloc(size);
}

void *SLOF_alloc_mem_aligned(size_t size, size_t alignment, uint64_t *pa)
{
    assert(slof_cma);
    return (void*)cma_alloc(slof_cma, size, pa);
    // return aligned_alloc(size, alignment);
}

void SLOF_free_mem(void *addr, long size)
{
    free(addr);
}

void SLOF_free_mem_aligned(void *addr)
{
    assert(slof_cma);
    cma_free(slof_cma, (vaddr_t)addr);
}

long SLOF_dma_map_in(void *virt, long size, int cacheable)
{
	// FIXME Empty as only used if IOMMU and VIRTIO_VERSION1 are supported
	(void) size;
	(void) cacheable;
	return (long) virt;
}

void SLOF_dma_map_out(long phys, void *virt, long size)
{
	// FIXME Empty as only used if IOMMU and VIRTIO_VERSION1 are supported
	(void) size;
	(void) phys;
	(void) virt;
}

/**
 * get msec-timer value
 * access to HW register
 * overrun will occur if boot exceeds 1.193 hours (49 days)
 *
 * @param   -
 * @return  actual timer value in ms as 32bit
 */
uint32_t SLOF_GetTimer(void)
{
    // uint64_t us = portGET_RUN_TIME_COUNTER_VALUE();
    uint64_t us = 0;
    return (uint32_t)(us / 1000);
}

static int lx_sleep(unsigned long delay_us)
{
    static unsigned long timer_ep = 0;
    int err = 0;
    if (timer_ep == 0) {
        err = sys_svc_wait("/dev/timer0", SVC_WAIT_EXACT, &timer_ep);
        if (err){
            return err;
        }
    }
    struct timespec tv = { 0 };
    tv.tv_sec = 0;
    tv.tv_nsec = delay_us * 1000;
    err = sys_timer_sleep(timer_ep, &tv);
    return err;
}

void SLOF_msleep(uint32_t time)
{
    lx_sleep((unsigned long)time*1000);
}

void SLOF_usleep(uint32_t time)
{
    lx_sleep(time);
}

#endif