#include "timeline_scheduler.h"
#include <stdio.h>

void vConfigureScheduler(TimelineConfig_t *cfg) {
    printf("Configuring Timeline Scheduler...\r\n");

    /* 1. Loop through the config and create tasks */
    for (uint32_t i = 0; i < cfg->num_tasks; i++) {
        TimelineTaskConfig_t *task = &cfg->tasks[i];
        
        /* Calculate priority AND grant MPU Privileges */
        UBaseType_t task_priority = (task->type == HARD_RT) ? 2 : 1;
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
        vTaskSuspend(task->xHandle);
        task->is_running = 0;
        printf("Created and suspended task: %s\r\n", task->task_name);
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
        vTaskDelayUntil(&xLastWakeTime, 100); 
        current_tick++;

        uint32_t time_in_major = current_tick % cfg->major_frame_ticks;
        uint32_t current_subframe = time_in_major / cfg->sub_frame_ticks;
        uint32_t time_in_sub = time_in_major % cfg->sub_frame_ticks;

        /* Show a small clock so you know the system is alive */
        printf("[%lu].", current_tick);
        if (current_tick % 10 == 0) printf("\n");

        /* --- REQUIREMENT: MAJOR FRAME RESET --- */
        if (time_in_major == 0) {
            printf("\r\n>>> MAJOR FRAME RESET (Tick %lu) <<<\r\n", current_tick);
            for (uint32_t i = 0; i < cfg->num_tasks; i++) {
                TimelineTaskConfig_t *task = &cfg->tasks[i];
                
                /* Ensure correct Priority and MPU Privilege Bit */
                UBaseType_t priority = (task->type == HARD_RT) ? 2 : 1;
                priority |= portPRIVILEGE_BIT;

                xTaskCreate(task->function, task->task_name, 1024, (void*)task, 
                            priority, &task->xHandle);
                
                vTaskSuspend(task->xHandle);
                task->is_running = 0;
            }
        }

        /* --- REQUIREMENT: TASK DISPATCHING --- */
        for (uint32_t i = 0; i < cfg->num_tasks; i++) {
            TimelineTaskConfig_t *task = &cfg->tasks[i];
            
            if (task->type == HARD_RT) {
                /* Start the HRT task */
                if ((task->ulSubframe_id == current_subframe) && (time_in_sub == task->ulStart_time)) {
                    printf("\r\n[%lu] DISPATCHER: Starting %s\r\n", current_tick, task->task_name);
                    task->is_running = 1;
                    vTaskResume(task->xHandle);
                }
                
                /* Deadline enforcement */
                if ((task->ulSubframe_id == current_subframe) && (time_in_sub == task->ulEnd_time)) {
                    if (task->is_running) {
                        printf("\r\n[%lu] !!! DEADLINE VIOLATION: Killing %s !!!\r\n", current_tick, task->task_name);
                        vTaskDelete(task->xHandle);
                        task->xHandle = NULL;
                        task->is_running = 0;
                    }
                }
            } else if (task->type == SOFT_RT && time_in_major == 1) {
                /* Run Soft RT tasks during idle gaps */
                vTaskResume(task->xHandle);
            }
        }
    }
}