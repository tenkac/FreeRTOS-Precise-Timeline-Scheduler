/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */
/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* App includes. */
#include "app_main.h"
#include <stdio.h>

#include "timeline_scheduler.h"

/* 1. Define two simple worker tasks */
void Task_A(void *pvParameters) {
    TimelineTaskConfig_t *my_cfg = (TimelineTaskConfig_t *)pvParameters;

    printf("      -> [Task A] Working...\r\n");
    
    /* Simulate a short task */
    for(volatile int i=0; i<100000; i++); 

    printf("      -> [Task A] Done. Self-Terminating.\r\n");
    
    /* Tell dispatcher we are finished safely */
    my_cfg->is_running = 0;
    vTaskDelete(NULL); 
}

void Task_B(void *pvParameters) {
    while(1) {
        printf("      -> [Task B] Executing Soft Real-Time work during idle time...\r\n");
        
        /* Soft RT work */
        volatile uint32_t delay = 0;
        for(delay = 0; delay < 50000; delay++); 

        /* Wait for the next major frame */
        vTaskSuspend(NULL); 
    }
}
/* 1. Define Task C (New HRT task) */
void Task_C(void *pvParameters) {
    TimelineTaskConfig_t *my_cfg = (TimelineTaskConfig_t *)pvParameters;
    printf("      -> [Task C] Running in a later sub-frame!\r\n");
    my_cfg->is_running = 0;
    vTaskDelete(NULL); 
}
/* 2. Create your task configuration array */
TimelineTaskConfig_t my_tasks[] = {
    {
        .task_name = "Task_A",
        .function = Task_A,
        .type = HARD_RT,
        .ulSubframe_id = 0,   /* Sub-frame 0 (Ticks 0-9) */
        .ulStart_time = 2,    /* Start at the 2nd tick */
        .ulEnd_time = 8       /* Deadline at the 8th tick */
    },
    {
        .task_name = "Task_C",
        .function = Task_C,
        .type = HARD_RT,
        .ulSubframe_id = 4,   /* Sub-frame 4 (Ticks 40-49) */
        .ulStart_time = 5,    /* Start at tick 5 of that sub-frame (Tick 45 total) */
        .ulEnd_time = 9
    },
    {
        .task_name = "Task_B",
        .function = Task_B,
        .type = SOFT_RT,
        /* SRT tasks just need the type; dispatcher handles the rest */
    }
};

/* 3. Create the global timeline configuration */
TimelineConfig_t my_timeline_config = {
    .major_frame_ticks = 100,
    .sub_frame_ticks = 10,
    .num_subframes = 10,
    .tasks = my_tasks,
    .num_tasks = 3  /* Don't forget to update this to 3! */
};

void app_main( void )
{
    printf( "\r\n--- FreeRTOS Timeline Scheduler Starting ---\r\n" );

    /* Call your custom scheduler setup! */
    vConfigureScheduler(&my_timeline_config);

    /* Hand control over to the RTOS. */
    vTaskStartScheduler();

    printf( "Returned from vTaskStartScheduler something bad had happened\n" );
    for( ; ; ) {}
}
/*-----------------------------------------------------------*/
/* KEEP ALL OF THE HOOKS EXACTLY AS THEY ARE BELOW THIS LINE */
/* vApplicationStackOverflowHook, vApplicationMallocFailedHook, etc... */
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName )
{
    /* If configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2 then this
     * function will automatically get called if a task overflows its stack. */
    ( void ) pxTask;
    ( void ) pcTaskName;

    printf( "Stack Overflow Hook called\n" );

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/
void vApplicationMallocFailedHook( void )
{
    /* If configUSE_MALLOC_FAILED_HOOK is set to 1 then this function will
     *  be called automatically if a call to pvPortMalloc() fails.  pvPortMalloc()
     *  is called automatically when a task, queue or semaphore is created. */
    printf( "Application Malloc Failed Hook called\n" );

    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
 * function then they must be declared static - otherwise they will be allocated on
 * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
     * state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 * application must provide an implementation of vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
 * function then they must be declared static - otherwise they will be allocated on
 * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/
