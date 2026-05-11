/****************************************************************************
* MInimal Real-time Operating System (MIROS)
* version 0.26 — priority scheduler + event flags
*
* Originally based on Miro Samek's MiROS lesson 25 teaching aid.
* Extended for the SKYRTOS migration: explicit priority per thread,
* event-flag wait/signal API (task + ISR safe).
****************************************************************************/
#ifndef MIROS_H
#define MIROS_H

#include <stdint.h>

/* Thread Control Block (TCB) */
typedef struct {
    void    *sp;            /* stack pointer */
    uint32_t timeout;       /* timeout delay down-counter */
    uint32_t evtFlags;      /* events that have been signalled */
    uint32_t evtWaitMask;   /* events the thread is currently waiting on (0 = not waiting) */
    uint8_t  prio;          /* 1..31 (0 reserved for idle) */
} OSThread;

typedef void (*OSThreadHandler)();

void OS_init(void *stkSto, uint32_t stkSize);

/* callback to handle the idle condition */
void OS_onIdle(void);

/* this function must be called with interrupts DISABLED */
void OS_sched(void);

/* transfer control to the RTOS to run the threads */
void OS_run(void);

/* blocking delay (timer-based wakeup) */
void OS_delay(uint32_t ticks);

/* process all timeouts; call from SysTick */
void OS_tick(void);

/* callback to configure and start interrupts */
void OS_onStartup(void);

/* Register a thread with the kernel.
 * prio: 1..31, must be unique. Higher number = higher priority.
 */
void OSThread_start(
    OSThread *me,
    uint8_t prio,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize);

/* ---- Event flag API ----------------------------------------------------- */

/* Block the calling thread until any bit in `mask` is set in evtFlags.
 * Returns the (single) lowest-bit event that was pending and clears it.
 * If multiple bits are set, only one bit is consumed per call.
 */
uint32_t OS_evtWait(uint32_t mask);

/* Signal events to a thread from task context (interrupts assumed enabled). */
void OS_evtSignal(OSThread *me, uint32_t mask);

/* Signal events to a thread from ISR context.
 * Safe to call from any IRQ handler. Pends PendSV if a context switch is needed.
 */
void OS_evtSignal_FromISR(OSThread *me, uint32_t mask);

#endif /* MIROS_H */
