#ifndef TIMELINE_SCHEDULER_H
#define TIMELINE_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

/* Define the two task categories */
typedef enum { 
    HARD_RT, 
    SOFT_RT 
} TaskType_t;

/* Configuration for a single task */
typedef struct {
    const char* task_name;
    TaskFunction_t function;
    TaskType_t type; 
    uint32_t ulStart_time;     /* Start time relative to the sub-frame (in ticks) */
    uint32_t ulEnd_time;       /* Deadline relative to the sub-frame (in ticks) */
    uint32_t ulSubframe_id;    /* Which sub-frame this task belongs to */
    
    /* Internal OS Tracking (Used by the Dispatcher, not the user) */
    TaskHandle_t xHandle;
    uint8_t is_running;
} TimelineTaskConfig_t;

/* Global configuration for the major frame */
typedef struct {
    uint32_t major_frame_ticks;
    uint32_t sub_frame_ticks;
    uint32_t num_subframes;
    TimelineTaskConfig_t* tasks;
    uint32_t num_tasks;
} TimelineConfig_t;

typedef enum { 
    TRACE_TASK_START, 
    TRACE_TASK_END, 
    TRACE_DEADLINE_MISS,
    TRACE_FRAME_RESET
} TraceEventType_t;

typedef struct {
    TickType_t tick;
    const char* task_name;
    TraceEventType_t event_type;
} TraceLog_t;

void vLogTraceEvent(const char* task_name, TraceEventType_t event);
void vPrintTraceSummary(void);
uint8_t validate_no_overlaps(TimelineConfig_t *cfg);

/* API Functions */
void vConfigureScheduler(TimelineConfig_t *cfg);
void vMasterDispatcherTask(void *pvParameters);

#endif /* TIMELINE_SCHEDULER_H */