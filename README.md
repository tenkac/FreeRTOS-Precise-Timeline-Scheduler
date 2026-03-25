# 🕒 FreeRTOS Precise Timeline Scheduler

## 🧭 Overview
This project implements a deterministic, timeline-driven scheduler for FreeRTOS, replacing the standard priority-based preemptive model with a strict **Time-Triggered Architecture (TTA)**. 

Designed for precise real-time environments, this scheduler enforces task execution based on a mathematically rigorous Major and Sub-frame structure. It ensures that Hard Real-Time (HRT) tasks execute within guaranteed, isolated time windows, while Soft Real-Time (SRT) tasks utilize the remaining idle CPU cycles.

## ⚙️ Key Features
* **Deterministic Major/Sub-Frame Structure:** The global timeline is divided into a 100-tick Major Frame containing ten 10-tick Sub-frames, ensuring 100% predictable and repeatable execution.
* **Temporal Isolation:** Hard Real-Time (HRT) tasks are strictly assigned to specific sub-frames with explicit start times and deadlines. 
* **Hard Deadline Enforcement (The "Kill Switch"):** The Master Dispatcher monitors HRT task execution. If a task exceeds its assigned window, the Dispatcher forcefully terminates it (`vTaskDelete`) to protect the timeline.
* **Soft Real-Time (SRT) Yielding:** SRT tasks operate at a lower priority, safely running during the idle gaps left by HRT tasks without risking preemption of critical operations.
* **Dynamic Reinitialization:** At the end of every Major Frame, the system state is entirely reset, and all tasks are re-created to prevent long-term state drift.

## 🏗️ System Architecture & Modified Files
This project modifies the standard FreeRTOS Cortex-M3 (MPS2/QEMU) port. The following core files drive the timeline logic:

* **`timeline_scheduler.c` / `.h` (The Custom Kernel):** Contains the high-priority Master Dispatcher (`vMasterDispatcherTask`) and the `vConfigureScheduler()` API. It calculates the modulo math for the current sub-frame, manages MPU privilege elevation, dynamically resumes HRT tasks at their exact start ticks, and acts as the system watchdog for deadline violations.
* **`app_main.c` (The Application Space):** Defines the worker tasks (`Task_A`, `Task_B`, `Task_C`) as single-shot functions that execute and self-terminate. It also houses the `TimelineTaskConfig_t` matrix, which maps out the temporal isolation and assigns tasks to their specific time slots.
* **`FreeRTOSConfig.h` (System Settings):** The OS memory pool (`configTOTAL_HEAP_SIZE`) was explicitly tuned to 24KB to accommodate the larger 1024-word stacks required for safe `printf` execution within the MPU boundaries, preventing starvation without overflowing the simulated board's RAM.
* **`Makefile` (Build Automation):** Updated to compile the custom scheduler logic, retain emergency hardware fault handlers (`vHandleMemoryFault`), and includes a custom `make run` target for seamless QEMU deployment.

## 🛠️ Configuration Interface
The schedule is defined at compile-time using the custom `TimelineTaskConfig_t` structure. This matrix is passed directly into the OS initialization:

```c
TimelineTaskConfig_t my_tasks[] = {
    {
        .task_name = "Task_A",
        .function = Task_A,
        .type = HARD_RT,
        .ulSubframe_id = 0,   /* Sub-frame 0 */
        .ulStart_time = 2,    /* Start at local tick 2 */
        .ulEnd_time = 8       /* Deadline at local tick 8 */
    },
    // Additional tasks follow...
}
```

## 🚀 Building and Running
This project requires an ARM cross-compiler (`arm-none-eabi-gcc`) and the QEMU emulator.

1. **Clean previous builds:**
   ```bash
   make clean
   ```
2. **Compile and launch the emulator:**
   ```bash
   make run
   ```

## Expected Output
Upon launching, the terminal will display a sequential, scaled-down timeline (100ms per tick for human readability). You will observe:

   * The passage of time represented by tick markers ([1].[2].).

   * HRT tasks dispatching at precise ticks (e.g., [102] DISPATCHER: Starting Task_A).

   * Tasks successfully self-terminating.

   * Acomplete system reset every 100 ticks (>>> MAJOR FRAME RESET <<<).
