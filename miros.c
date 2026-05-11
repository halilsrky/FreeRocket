/****************************************************************************
* MInimal Real-time Operating System (MiROS), ARM-CLANG port.
* version 0.26 — priority scheduler + event flags
*
* Originally based on Miro Samek's MiROS lesson 25 teaching aid.
* Extended for the SKYRTOS migration:
*   - Strict priority preemption (highest set bit in OS_readySet wins)
*   - Per-thread event flags (evtFlags / evtWaitMask)
*   - OS_evtWait blocks the caller; one event bit consumed per call (LSB-first)
*   - OS_evtSignal / OS_evtSignal_FromISR wake the target thread
*
* SPDX-License-Identifier: GPL-3.0-or-later
****************************************************************************/
#include <stdint.h>
#include "stm32f446xx.h"   /* CMSIS: SCB, __get_PRIMASK, __disable_irq, ... */
#include "miros.h"
#include "qassert.h"

Q_DEFINE_THIS_FILE

OSThread * volatile OS_curr; /* pointer to the current thread */
OSThread * volatile OS_next; /* pointer to the next thread to run */

/* OS_thread[0] is reserved for the idle thread.
 * OS_thread[1..31] hold user threads, indexed by their priority.
 * The priority IS the index — there is no separate "thread number" anymore.
 */
OSThread *OS_thread[32];
uint32_t OS_readySet; /* bit N set => OS_thread[N] is ready (N >= 1) */

OSThread idleThread;
void main_idleThread() {
    while (1) {
        OS_onIdle();
    }
}

void OS_init(void *stkSto, uint32_t stkSize) {
    /* set the PendSV interrupt priority to the lowest level 0xFF */
    SCB->SHP[10] = 0xFFU;  /* SHP[10] is PendSV's 8-bit priority */

    /* register the idle thread at priority 0 */
    OSThread_start(&idleThread,
                   0U,
                   &main_idleThread,
                   stkSto, stkSize);
}

void OS_sched(void) {
    uint32_t nextIdx;

    if (OS_readySet == 0U) {
        nextIdx = 0U;  /* idle */
    } else {
        /* highest set bit = highest priority ready thread.
         * __builtin_clz(0) is UB — guarded by the check above.
         */
        nextIdx = 31U - (uint32_t)__builtin_clz(OS_readySet);
    }

    OSThread * const next = OS_thread[nextIdx];
    if (next != OS_curr) {
        OS_next = next;
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void OS_run(void) {
    OS_onStartup();

    __asm volatile ("cpsid i");
    OS_sched();
    __asm volatile ("cpsie i");

    /* the following code should never execute */
    Q_ERROR();
}

void OS_tick(void) {
    uint8_t n;
    for (n = 1U; n < 32U; ++n) {
        OSThread *t = OS_thread[n];
        if ((t != (OSThread *)0) && (t->timeout != 0U)) {
            --t->timeout;
            if (t->timeout == 0U) {
                /* OS_delay-based wakeup. evtWaitMask is 0 for delayed threads. */
                OS_readySet |= (1U << n);
            }
        }
    }
}

void OS_delay(uint32_t ticks) {
    __asm volatile ("cpsid i");

    /* never call OS_delay from the idle thread */
    Q_REQUIRE(OS_curr != OS_thread[0]);

    OS_curr->timeout = ticks;
    OS_readySet &= ~(1U << OS_curr->prio);
    OS_sched();
    __asm volatile ("cpsie i");
}

void OSThread_start(
    OSThread *me,
    uint8_t prio,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize)
{
    Q_REQUIRE(prio < Q_DIM(OS_thread));
    Q_REQUIRE(OS_thread[prio] == (OSThread *)0); /* prio collision check */

    /* round down the stack top to the 8-byte boundary
    * NOTE: ARM Cortex-M stack grows down from hi -> low memory
    */
    uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);
    uint32_t *stk_limit;

    *(--sp) = (1U << 24);  /* xPSR */
    *(--sp) = (uint32_t)threadHandler; /* PC */
    *(--sp) = 0x0000000EU; /* LR  */
    *(--sp) = 0x0000000CU; /* R12 */
    *(--sp) = 0x00000003U; /* R3  */
    *(--sp) = 0x00000002U; /* R2  */
    *(--sp) = 0x00000001U; /* R1  */
    *(--sp) = 0x00000000U; /* R0  */
    *(--sp) = 0x0000000BU; /* R11 */
    *(--sp) = 0x0000000AU; /* R10 */
    *(--sp) = 0x00000009U; /* R9 */
    *(--sp) = 0x00000008U; /* R8 */
    *(--sp) = 0x00000007U; /* R7 */
    *(--sp) = 0x00000006U; /* R6 */
    *(--sp) = 0x00000005U; /* R5 */
    *(--sp) = 0x00000004U; /* R4 */

    me->sp = sp;
    me->prio = prio;
    me->timeout = 0U;
    me->evtFlags = 0U;
    me->evtWaitMask = 0U;

    /* round up the bottom of the stack to the 8-byte boundary */
    stk_limit = (uint32_t *)(((((uint32_t)stkSto - 1U) / 8) + 1U) * 8);

    /* pre-fill the unused part of the stack with 0xDEADBEEF */
    for (sp = sp - 1U; sp >= stk_limit; --sp) {
        *sp = 0xDEADBEEFU;
    }

    OS_thread[prio] = me;

    /* Idle (prio 0) is never in OS_readySet — the scheduler falls back to it
     * only when readySet is empty.
     */
    if (prio > 0U) {
        OS_readySet |= (1U << prio);
    }
}

/* ---- Event flag API ----------------------------------------------------- */

uint32_t OS_evtWait(uint32_t mask) {
    uint32_t pending;
    uint32_t bit;

    __asm volatile ("cpsid i");
    Q_REQUIRE(OS_curr != OS_thread[0]);
    Q_REQUIRE(mask != 0U);

    while ((OS_curr->evtFlags & mask) == 0U) {
        OS_curr->evtWaitMask = mask;
        OS_readySet &= ~(1U << OS_curr->prio);
        OS_sched();
        __asm volatile ("cpsie i");
        /* PendSV switches us out here. When we resume, re-check evtFlags
         * (signal could have set a different bit, or a spurious wakeup). */
        __asm volatile ("cpsid i");
    }
    OS_curr->evtWaitMask = 0U;

    /* Consume the lowest set bit in (evtFlags & mask). Other bits stay
     * pending for the next OS_evtWait call.
     */
    pending = OS_curr->evtFlags & mask;
    bit = pending & ((~pending) + 1U); /* isolate LSB */
    OS_curr->evtFlags &= ~bit;

    __asm volatile ("cpsie i");
    return bit;
}

void OS_evtSignal(OSThread *me, uint32_t mask) {
    __asm volatile ("cpsid i");
    me->evtFlags |= mask;
    if ((me->evtWaitMask != 0U) && ((me->evtWaitMask & me->evtFlags) != 0U)) {
        me->evtWaitMask = 0U;
        OS_readySet |= (1U << me->prio);
        OS_sched();
    }
    __asm volatile ("cpsie i");
}

void OS_evtSignal_FromISR(OSThread *me, uint32_t mask) {
    /* Nested-safe: save & restore PRIMASK. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    me->evtFlags |= mask;
    if ((me->evtWaitMask != 0U) && ((me->evtWaitMask & me->evtFlags) != 0U)) {
        me->evtWaitMask = 0U;
        OS_readySet |= (1U << me->prio);
        OS_sched(); /* pends PendSV; tail-chains on ISR exit */
    }

    __set_PRIMASK(primask);
}

/* ---- PendSV context switch (unchanged) ---------------------------------- */

__attribute__ ((naked))
void PendSV_Handler(void) {
__asm volatile (
    /* __disable_irq(); */
    "  CPSID         I                 \n"

    /* if (OS_curr != (OSThread *)0) { */
    "  LDR           r1,=OS_curr       \n"
    "  LDR           r1,[r1,#0x00]     \n"
    "  CMP           r1,#0             \n"
    "  BEQ           PendSV_restore    \n"

    /*     push registers r4-r11 on the stack */
#if (__ARM_ARCH == 6)               // if ARMv6-M...
    "  SUB           sp,sp,#(8*4)     \n" // make room for 8 registers r4-r11
    "  MOV           r0,sp            \n" // r0 := temporary stack pointer
    "  STMIA         r0!,{r4-r7}      \n" // save the low registers
    "  MOV           r4,r8            \n" // move the high registers to low registers...
    "  MOV           r5,r9            \n"
    "  MOV           r6,r10           \n"
    "  MOV           r7,r11           \n"
    "  STMIA         r0!,{r4-r7}      \n" // save the high registers
#else                               // ARMv7-M or higher
    "  PUSH          {r4-r11}          \n"
#endif

    /*     OS_curr->sp = sp; */
    "  LDR           r1,=OS_curr       \n"
    "  LDR           r1,[r1,#0x00]     \n"
    "  MOV           r0,sp             \n"
    "  STR           r0,[r1,#0x00]     \n"
    /* } */

    "PendSV_restore:                   \n"
    /* sp = OS_next->sp; */
    "  LDR           r1,=OS_next       \n"
    "  LDR           r1,[r1,#0x00]     \n"
    "  LDR           r0,[r1,#0x00]     \n"
    "  MOV           sp,r0             \n"

    /* OS_curr = OS_next; */
    "  LDR           r1,=OS_next       \n"
    "  LDR           r1,[r1,#0x00]     \n"
    "  LDR           r2,=OS_curr       \n"
    "  STR           r1,[r2,#0x00]     \n"

    /* pop registers r4-r11 */
#if (__ARM_ARCH == 6)
    "  MOV           r0,sp             \n"
    "  MOV           r2,r0             \n"
    "  ADDS          r2,r2,#(4*4)      \n"
    "  LDMIA         r2!,{r4-r7}       \n"
    "  MOV           r8,r4             \n"
    "  MOV           r9,r5             \n"
    "  MOV           r10,r6            \n"
    "  MOV           r11,r7            \n"
    "  LDMIA         r0!,{r4-r7}       \n"
    "  ADD           sp,sp,#(8*4)      \n"
#else
    "  POP           {r4-r11}          \n"
#endif

    /* __enable_irq(); */
    "  CPSIE         I                 \n"

    /* return to the next thread */
    "  BX            lr                \n"
    );
}
