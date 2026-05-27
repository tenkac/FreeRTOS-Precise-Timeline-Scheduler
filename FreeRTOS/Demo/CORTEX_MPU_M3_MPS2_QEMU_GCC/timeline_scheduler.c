#include "timeline_scheduler.h"
#include <stdio.h>

#define MAX_TRACE_LOGS 100
TraceLog_t system_trace[MAX_TRACE_LOGS];
uint32_t trace_index = 0;

void vLogTraceEvent(const char* task_name, TraceEventType_t event) {
    taskENTER_CRITICAL();
    if (trace_index < MAX_TRACE_LOGS) {
        system_trace[trace_index].tick = xTaskGetTickCount();
        system_trace[trace_index].task_name = task_name;
        system_trace[trace_index].event_type = event;
        trace_index++;
    }
    taskEXIT_CRITICAL();
}

void vPrintTraceSummary(void) {
    printf("\r\n--- SCHEDULER TRACE SUMMARY ---\r\n");
    for (uint32_t i = 0; i < trace_index; i++) {
        printf("Tick: %lu | Task: %s | Event: %d\r\n", 
            system_trace[i].tick, 
            system_trace[i].task_name, 
            system_trace[i].event_type);
    }
    printf("-------------------------------\r\n");
}

void vConfigureScheduler(TimelineConfig_t *cfg) {
    printf("Validating Timeline Configuration...\r\n");

    /* 1. Validate Math */
    if (cfg->major_frame_ticks != (cfg->sub_frame_ticks * cfg->num_subframes)) {
        printf("FATAL ERROR: Major frame ticks do not equal total sub-frame ticks!\r\n");
        for(;;); /* Halt system */
    }

    if (!validate_no_overlaps(cfg)) {
        printf("FATAL ERROR: HRT Task overlap detected in configuration!\r\n");
        for(;;);
    }

    /* 2. Validate Task Pointers */
    for (uint32_t i = 0; i < cfg->num_tasks; i++) {
        if (cfg->tasks[i].function == NULL) {
            printf("FATAL ERROR: %s has no assigned function!\r\n", cfg->tasks[i].task_name);
            for(;;); /* Halt system */
        }
    }
    
    printf("Configuration Validated. Configuring Timeline Scheduler...\r\n");

    /* 1. Loop through the config and create tasks */
    for (uint32_t i = 0; i < cfg->num_tasks; i++) {
        TimelineTaskConfig_t *task = &cfg->tasks[i];
        
        /* Calculate priority AND grant MPU Privileges */
        UBaseType_t task_priority = (task->type == HARD_RT) ? configMAX_PRIORITIES - 2  : 1;
        task_priority |= portPRIVILEGE_BIT; 
        
        /* Create the task with a much larger stack (1024 words instead of minimal) */
        BaseType_t xResult = xTaskCreate(
            task->function,
            task->task_name,
            1024, 
            (void *)task,  /* <--- Change NULL to (void *)task */
            task_priority,
            &task->xHandle
        );
        
        /* Check if the system ran out of heap memory! */
        if (xResult != pdPASS) {
            printf("ERROR: Not enough memory to create %s!\r\n", task->task_name);
            for(;;); /* Halt the system */
        }
        
        /* Suspend it so it doesn't run autonomously */
        if (task->type == SOFT_RT)
        {
            vTaskResume(task->xHandle);
            task->is_running = 1;
            printf("Created and resumed SRT task: %s\r\n", task->task_name);
        }
        else
        {
            vTaskSuspend(task->xHandle);
            task->is_running = 0;
            printf("Created and suspended HRT task: %s\r\n", task->task_name);
        }
    }
    
    /* 2. Create the Master Dispatcher Task */
    BaseType_t xDispResult = xTaskCreate(
        vMasterDispatcherTask, 
        "Dispatcher", 
        1024, 
        (void*)cfg, 
        (configMAX_PRIORITIES - 1) | portPRIVILEGE_BIT, 
        NULL
    );
    
    if (xDispResult != pdPASS) {
        printf("ERROR: Not enough memory to create Master Dispatcher!\r\n");
        for(;;);
    }
    
    printf("Master Dispatcher created. Ready to start.\r\n");
}

void vMasterDispatcherTask(void *pvParameters) {
    TimelineConfig_t *cfg = (TimelineConfig_t *)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t current_tick = 0;

    while(1) {
        /* 100ms delay = 10 ticks per second. Fast enough to be fun, slow enough to read. */
        vTaskDelayUntil(&xLastWakeTime, 1); 
        current_tick++;

        uint32_t time_in_major = current_tick % cfg->major_frame_ticks;
        uint32_t current_subframe = time_in_major / cfg->sub_frame_ticks;
        uint32_t time_in_sub = time_in_major % cfg->sub_frame_ticks;

        /* Show a small clock so you know the system is alive */
        printf("[%lu].", current_tick);
        if (current_tick % 10 == 0) printf("\n");

        /* --- REQUIREMENT: MAJOR FRAME RESET (STAGGERED FIX) --- */
        
        /* 1. DELETE on the exact last tick of the Major Frame */
        if (time_in_major == (cfg->major_frame_ticks - 1)) {
            for (uint32_t i = 0; i < cfg->num_tasks; i++) {
                TimelineTaskConfig_t *task = &cfg->tasks[i];
                if (task->xHandle != NULL) {
                    vTaskDelete(task->xHandle);
                    task->xHandle = NULL;
                }
            }
        }

        /* 2. RECREATE on the exact first tick of the next Major Frame */
        /* In timeline_scheduler.c -> vMasterDispatcherTask() -> MAJOR FRAME RESET block */
        if (time_in_major == 0)
        {
            printf("\r\n>>> MAJOR FRAME RESET (Tick %lu) <<<\r\n", current_tick);
            vLogTraceEvent("SYSTEM", TRACE_FRAME_RESET);
            for (uint32_t i = 0; i < cfg->num_tasks; i++)
            {
                TimelineTaskConfig_t *task = &cfg->tasks[i];

                UBaseType_t priority = (task->type == HARD_RT) ? configMAX_PRIORITIES - 2 : 1;
                priority |= portPRIVILEGE_BIT;

                xTaskCreate(task->function, task->task_name, 1024, (void *)task,
                            priority, &task->xHandle);

                task->is_running = 0;

                /* Change: Immediately resume SRT tasks so they are Ready for idle time */
                if (task->type == SOFT_RT)
                {
                    vTaskResume(task->xHandle);
                    task->is_running = 1;
                }
                else
                {
                    vTaskSuspend(task->xHandle);
                }
            }
        }

        /* --- REQUIREMENT: TASK DISPATCHING --- */
        for (uint32_t i = 0; i < cfg->num_tasks; i++) {
            TimelineTaskConfig_t *task = &cfg->tasks[i];
            
            if (task->type == HARD_RT) {
                /* Start the HRT task */
                if ((task->ulSubframe_id == current_subframe) && (time_in_sub == task->ulStart_time)) {
                    printf("\r\n[%lu] DISPATCHER: Starting %s\r\n", current_tick, task->task_name);
                    vLogTraceEvent(task->task_name, TRACE_TASK_START);
                    task->is_running = 1;
                    if (task->xHandle != NULL) {
                        task->is_running = 1;
                        vTaskResume(task->xHandle);
                    }   
                }

                /* Deadline enforcement */
                if ((task->ulSubframe_id == current_subframe) && (time_in_sub == task->ulEnd_time))
                {
                    if (task->is_running && task->xHandle != NULL)
                    { /* <-- Add xHandle check here */
                        printf("\r\n[%lu] !!! DEADLINE VIOLATION: Killing %s !!!\r\n", current_tick, task->task_name);
                        vLogTraceEvent(task->task_name, TRACE_DEADLINE_MISS);
                        vTaskDelete(task->xHandle);
                        task->xHandle = NULL;
                        task->is_running = 0;
                    }
                }
            }
        }
    }
}