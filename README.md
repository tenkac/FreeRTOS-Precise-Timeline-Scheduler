# 🕒 Precise Timeline Scheduler for FreeRTOS

A deterministic, timeline-driven scheduling architecture built on top of FreeRTOS. This project replaces the default dynamic priority-based model with a strict, time-triggered paradigm, ensuring highly predictable and repeatable real-time execution.

## 🧭 Overview

In mission-critical embedded systems, knowing *exactly* when a task will run is often more important than running it as fast as possible. This project introduces a Master Dispatcher that enforces a rigid timeline divided into a **Major Frame** and smaller **Sub-frames**. 

Tasks are spawned and terminated at exact millisecond boundaries defined by a static configuration. If a task violates its timing constraints, the scheduler ruthlessly enforces the deadline to protect system stability.

## ✨ Key Features

* **Strict Determinism:** Zero timing drift. Tasks execute at precise tick markers, repeatable across tens of thousands of frames.
* **Master Dispatcher:** A high-priority central controller that handles task creation, resumption, and termination without modifying the underlying FreeRTOS kernel.
* **Memory Safe Resets:** Flawless Major Frame resetting logic that destroys and recreates the environment without heap exhaustion or dangling pointers.
* **Thread Safety:** Implements FreeRTOS critical sections to protect shared task-state variables from race conditions.
* **Automated Static Testing:** Pre-flight configuration validation that catches mathematical errors and illegal task overlaps before the OS boots.
* **High-Speed Trace Module:** A "flight data recorder" that logs execution events to memory with zero performance penalty, printing a structured post-mortem summary at the end of each frame.

---

## ⚙️ Architecture & Task Model

The scheduler is governed by a `TimelineConfig_t` structure that defines the Major Frame (e.g., 100 ms) and its Sub-frames (e.g., 10 frames of 10 ms). Tasks are strictly categorized into two types:

### 🧱 Hard Real-Time (HRT) Tasks
* **Strict Timing:** Assigned a specific start time and deadline within a specific sub-frame.
* **Non-preemptive Execution:** Runs to completion within its designated slot.
* **Deadline Enforcement:** If an HRT task fails to self-terminate by its deadline tick, the Dispatcher kills it instantly and logs a `TRACE_DEADLINE_MISS`.

### 🌿 Soft Real-Time (SRT) Tasks
* **Background Execution:** Spawns at the beginning of the Major Frame and runs *only* during the idle CPU time left behind by HRT tasks.
* **Preemptible:** Naturally preempted by HRT tasks using FreeRTOS priority management.
* **No Guarantees:** SRT tasks are not guaranteed to finish within a single frame and do not trigger deadline violations.

---

## 🛠️ Configuration Example

The entire timeline is defined statically at compile time.

```c
TimelineTaskConfig_t my_tasks[] = {
    {
        .task_name = "Task_A",
        .function = Task_A,
        .type = HARD_RT,
        .ulSubframe_id = 0,   /* Sub-frame 0 */
        .ulStart_time = 2,    /* Starts at Tick 2 */
        .ulEnd_time = 8       /* Deadline at Tick 8 */
    },
    {
        .task_name = "Task_B",
        .function = Task_B,
        .type = SOFT_RT,      /* SRT fills idle time */
    }
};

TimelineConfig_t my_timeline_config = {
    .major_frame_ticks = 100,
    .sub_frame_ticks = 10,
    .num_subframes = 10,
    .tasks = my_tasks,
    .num_tasks = 2
};
```

---

## 📊 Trace Module (The Flight Recorder)

To prove determinism without causing "Heisenbugs" (timing issues caused by slow `printf` calls), the scheduler utilizes a high-speed memory trace array. 

Events (`TRACE_TASK_START`, `TRACE_DEADLINE_MISS`, `TRACE_FRAME_RESET`) are written to memory instantly. At the exact boundary of the Major Frame reset, the Dispatcher dumps the deferred log to the console:

```text
>>> MAJOR FRAME RESET (Tick 100) <<<

--- SCHEDULER TRACE SUMMARY ---
Tick: 2 | Task: Task_A | Event: 0 (START)
Tick: 45 | Task: Task_C | Event: 0 (START)
Tick: 100 | Task: SYSTEM | Event: 3 (RESET)
-------------------------------
```

---

## 🚀 Building and Running

This project is built to run on the **QEMU Cortex-M3 (MPS2)** emulator.

1. **Clean the build directory:**
   ```bash
   make clean
   ```
2. **Compile the OS:**
   ```bash
   make
   ```
3. **Launch the Emulator:**
   ```bash
   make run
   ```

*Note: To exit QEMU without closing your terminal, press `Ctrl-A` followed by `X`.*

---

## 🧪 Automated Testing

The system includes a pre-flight automated test suite that runs immediately before `vConfigureScheduler()`. It verifies:
1. **Invalid Config Math:** Ensures `(sub_frames * sub_frame_ticks) == major_frame_ticks`.
2. **Overlapping HRT Tasks:** Iterates through the task array to ensure no two HRT tasks occupy the exact same time window, preventing priority inversions.
3. **Trace Validations:** Post-boot trace analysis verifies deadline enforcements and major frame resets.