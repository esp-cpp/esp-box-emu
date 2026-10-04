/* The PSX kernel's thread primitives, on FreeRTOS.
 *
 * MGS's mts layer is a cooperative scheduler: it decides which task should run
 * and calls ChangeTh() to switch to it. On the PSX ChangeTh does not return --
 * execution resumes inside the target thread, and control comes back only when
 * somebody switches back.
 *
 * That maps onto FreeRTOS exactly: one task per PSX thread, created suspended,
 * and ChangeTh becomes "resume the target, suspend myself". The cooperative
 * semantics are preserved because at most one of these tasks is ever runnable.
 *
 * What is deliberately NOT preserved: the stack pointer MGS passes to OpenTh.
 * It allocates its own stacks out of the game heap, which FreeRTOS cannot adopt
 * -- each task gets its own instead, sized from the largest MGS asks for.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include "esp_heap_caps.h"

#define MGS_MAX_THREADS 8
/* MGS declares 2 KB stacks (GAME_STACK_SIZE in main.c) because the PSX's libc
 * was tiny. Ours is not: one newlib printf can eat a couple of KB on its own,
 * and Main()'s call chain goes several frames deep before the first output. An
 * overflow here is silent -- the task just stops -- so be generous. */
/* 20480 was a guess made before anything could be measured. The high-water
 * marks from a long session say what it actually needs:
 *
 *     t0=2100  t1=1944  t2=3164  t3=14484  t4=2080  t5=2124
 *
 * One task uses its stack; the other five never pass 3.2 KB of their 20. These
 * live in internal SRAM -- they have to, because a task whose stack is in PSRAM
 * loses access to it the instant the flash driver disables the cache, and the
 * chip resets before it can even print. So this number is spent from the
 * scarcest memory on the board, and it is the same memory the rasterizer wants
 * for hot texture pages.
 *
 * 16 KB keeps 1.9 KB over the worst mark observed and returns 24 KB. Deliberately
 * conservative: the mark is an observation of one session, not a proof, and a
 * stack overflow here would corrupt a neighbour rather than fault cleanly. The
 * real prize is per-task sizing -- five of these could run on 4 KB and free
 * ~80 KB -- but that needs the slot-to-thread mapping to be known at creation
 * time, which it is not yet. */
#define MGS_THREAD_STACK 16384
#ifdef MGS_ESPBOX
/* esp-box-emu shares its internal RAM with the emulator menu and the display
 * path, so the slots are sized from the measured high-water marks above (slot
 * 3 is the game's main loop, the others never passed 3.2KB) rather than all
 * getting 16KB. */
static const unsigned mgs_thread_stack_size[MGS_MAX_THREADS] = {
    6144, 6144, 6144, 20480, 6144, 6144, 6144, 6144 };
#define MGS_STACK_FOR(i) (mgs_thread_stack_size[(i)])
#else
#define MGS_STACK_FOR(i) ((unsigned)MGS_THREAD_STACK)
#endif

typedef struct {
    TaskHandle_t handle;
    void (*entry)(void);
    void* stack;         /* PSRAM */
    StaticTask_t* tcb;   /* must stay in internal RAM */
    int in_use;
    /* The PSX kept the interrupt mask in each task's SR register, restored on
     * every context switch: a task that transfers away inside a critical
     * section does NOT mask interrupts for whoever runs next. A single global
     * flag wedged the whole system the moment that happened; so the flag is
     * per-thread, swapped on every switch. */
    int crit;
} MgsThread;

extern volatile int psyz_critical_depth;   /* psyz libapi.c: the running task's "SR" */

static MgsThread threads[MGS_MAX_THREADS];
static int current_thread = -1;

/* The PSX's ChangeTh was a syscall and ran with interrupts masked; nothing
 * could observe it half-done. Here the vblank tick preempts at any instruction
 * and once read current_thread mid-update -- it then resumed a task that was
 * not parked and left two mts tasks runnable, which wedges the cooperative
 * scheduler without ever crashing. While a cooperative switch is in flight the
 * tick DEFERS its preemption to the next frame instead (ChangeThFromISR
 * returns 0 and mts reverts its bookkeeping). */
static volatile int change_in_flight;

static void thread_trampoline(void* arg) {
    MgsThread* t = (MgsThread*)arg;
    /* Park BEFORE running anything. Creating the task and then suspending it
     * from the creator is a race: on a dual-core chip FreeRTOS can start it on
     * the other core immediately, so the PSX thread would run before the code
     * that created it had finished filling in its task control block. */
    vTaskSuspend(NULL);
    if (t->entry) {
        t->entry();
    }
    /* a PSX thread that returns simply stops being scheduled */
    t->in_use = 0;
    vTaskSuspend(NULL);
    for (;;) {
    }
}

unsigned long OpenTh(unsigned long (*func)(), unsigned long sp,
                     unsigned long gp) {
    int i;
    (void)sp; /* MGS's own stack; FreeRTOS supplies ours */
    (void)gp;
    for (i = 0; i < MGS_MAX_THREADS; i++) {
        if (!threads[i].in_use) {
            break;
        }
    }
    if (i == MGS_MAX_THREADS) {
        printf("[thread] OpenTh: out of slots (max %d)\n", MGS_MAX_THREADS);
        return (unsigned long)-1;
    }
    printf("[thread] OpenTh slot %d func %p\n", i, (void*)func);
    threads[i].entry = (void (*)(void))func;
    threads[i].in_use = 1;

    /* Stacks go to PSRAM. FreeRTOS takes them from internal RAM by default,
     * and internal RAM is this chip's scarce resource -- with three tasks
     * already up, the fourth xTaskCreate() failed, OpenTh returned -1, and
     * every later ChangeTh() to that task was rejected. That is what stalled
     * the boot: not a scheduling deadlock, just no room.
     *
     * The task control block itself must stay internal (FreeRTOS touches it
     * from contexts where PSRAM may be unreachable); only the stack moves. */
    /* ...but they cannot LIVE there. PSRAM is reached through the same cache
     * the flash driver switches off while it reads, so the moment a task with a
     * PSRAM stack calls fopen() the CPU loses access to its own stack and the
     * chip resets instantly -- too early even to print a panic. That is exactly
     * what happened at the first CD read. Internal RAM it is; the stacks are
     * smaller to make them fit, which is affordable because MGS itself only
     * asked for 2 KB. PSRAM stays as a fallback for tasks that never touch
     * files, rather than failing the OpenTh outright. */
    threads[i].stack = heap_caps_malloc(MGS_STACK_FOR(i), MALLOC_CAP_INTERNAL);
    if (!threads[i].stack) {
        printf("[thread] slot %d: no internal RAM, falling back to PSRAM\n", i);
        threads[i].stack = heap_caps_malloc(MGS_STACK_FOR(i), MALLOC_CAP_SPIRAM);
    }
    threads[i].tcb = heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    if (!threads[i].stack || !threads[i].tcb) {
        printf("[thread] OpenTh: no memory for slot %d (stack %p tcb %p)\n", i,
               threads[i].stack, (void*)threads[i].tcb);
        free(threads[i].stack);
        free(threads[i].tcb);
        threads[i].stack = NULL;
        threads[i].tcb = NULL;
        threads[i].in_use = 0;
        return (unsigned long)-1;
    }
    /* Pinned to core 0, without exception. The vblank tick preempts the game
     * on this same core (esp32_vblank.c); letting FreeRTOS float an mts task
     * to core 1 re-creates the cross-core race where ChangeThFromISR reads a
     * stale current_thread and resumes a task parked mid-ChangeTh -- which is
     * how a second copy of Main() once started running interleaved with the
     * first. */
    threads[i].handle = xTaskCreateStaticPinnedToCore(
        thread_trampoline, "mgs_th", MGS_STACK_FOR(i), &threads[i], 5,
        (StackType_t*)threads[i].stack, threads[i].tcb, 0);
    if (!threads[i].handle) {
        printf("[thread] OpenTh: xTaskCreateStatic failed for slot %d\n", i);
        threads[i].in_use = 0;
        return (unsigned long)-1;
    }
    /* the task parks itself on entry (see thread_trampoline); nothing to do
     * here but hand back the slot as the PSX thread id */
    return (unsigned long)i;
}

/* Called from the tick every ~48 s. uxTaskGetStackHighWaterMark reports the
 * smallest free margin a task has ever had, in words, so the deepest the game
 * has actually gone is MGS_THREAD_STACK minus that -- the number the stack
 * size should be sized from, plus headroom for a path not exercised yet. */
void Mgs_ReportThreadStacks(void) {
    int i;
    unsigned total_free = 0;
    printf("[stack] used of %d B:", MGS_THREAD_STACK);
    for (i = 0; i < MGS_MAX_THREADS; i++) {
        if (threads[i].in_use && threads[i].handle) {
            unsigned free_b =
                (unsigned)uxTaskGetStackHighWaterMark(threads[i].handle) *
                sizeof(StackType_t);
            printf(" t%d=%u/%u", i, (unsigned)MGS_STACK_FOR(i) - free_b, (unsigned)MGS_STACK_FOR(i));
            total_free += free_b;
        }
    }
    printf(" | reclaimable %u B\n", total_free);
}

long CloseTh(unsigned long thread) {
    int i = (int)thread;
    if (i < 0 || i >= MGS_MAX_THREADS || !threads[i].in_use) {
        return 0;
    }
    threads[i].in_use = 0;
    if (threads[i].handle) {
        vTaskDelete(threads[i].handle);
        threads[i].handle = 0;
    }
    return 1;
}

long ChangeTh(unsigned long thread) {
    int target = (int)thread;
    int self;

    /* FIRST, before anything -- even the validation and its printf. Any window
     * where the tick can preempt while this function has read current_thread
     * but not finished the switch lets the vblank act on half-truth: it once
     * fired inside the printf below, saw the OLD current, "resumed" the very
     * task that was mid-switch, and the mts bookkeeping skewed one task over
     * -- the ready bit of the woken task was consumed by the wrong task and
     * the whole scheduler starved without crashing. */
    change_in_flight = 1;
    self = current_thread;

    if (target < 0 || target >= MGS_MAX_THREADS || !threads[target].in_use) {
        change_in_flight = 0;
        printf("[thread] ChangeTh: bad tid %d\n", target);
        return 0;
    }
    if (target == self) {
        change_in_flight = 0;
        return 1;
    }

    /* ChangeTh must NEVER return to its caller until somebody switches back:
     * mts_receive() sets up its wait and then yields through here, so a
     * ChangeTh that returns early makes the receive look like it completed
     * with no sender -- which is exactly the "RECV ?? SRC -2" loop.
     *
     * The very first call comes from whatever task bootstrapped the game
     * (app_main's), which is not yet one of ours. Adopt it into a slot so it
     * can be parked and resumed like any other. */
    if (self < 0) {
        int i;
        for (i = 0; i < MGS_MAX_THREADS; i++) {
            if (!threads[i].in_use) {
                break;
            }
        }
        if (i < MGS_MAX_THREADS) {
            threads[i].handle = xTaskGetCurrentTaskHandle();
            threads[i].entry = 0;
            threads[i].in_use = 1;
            self = i;
        }
    }

    printf("[thread] ChangeTh %d -> %d\n", self, target);
    if (self >= 0 && self < MGS_MAX_THREADS) {
        threads[self].crit = psyz_critical_depth;
    }
    psyz_critical_depth = threads[target].crit;
    current_thread = target;
    vTaskResume(threads[target].handle);

    if (self >= 0 && self < MGS_MAX_THREADS && threads[self].handle) {
        /* cleared just before parking: from here on current_thread is
         * consistent and the tick may preempt again */
        change_in_flight = 0;
        vTaskSuspend(NULL);
    } else {
        change_in_flight = 0;
        /* nothing to park: spin the scheduler so the target actually runs */
        vTaskDelay(1);
    }
    return 1;
}

/* the scheduler asks who is running */
int Mgs_CurrentThread(void) { return current_thread; }

/* Resume a PSX thread WITHOUT parking the caller.
 *
 * On the console, waking a task from the vblank interrupt was a side effect of
 * writing the new TCB into the kernel header: the interrupt return then landed
 * in the woken task. There is no equivalent here, and plain ChangeTh() is wrong
 * because it suspends whoever calls it -- and the caller is the vblank tick,
 * which must keep running. So: make the target runnable and return. The next
 * cooperative ChangeTh() from game code will settle who actually holds the CPU.
 */
long ChangeThFromISR(unsigned long thread) {
    int target = (int)thread;
    static int noisy = 12;
    if (change_in_flight) {
        /* a cooperative ChangeTh is mid-update; preempting on a half-written
         * current_thread is how two tasks once ended up runnable. Skip this
         * frame; the caller reverts its bookkeeping and the next vblank
         * retries. */
        return 0;
    }
    if (target < 0 || target >= MGS_MAX_THREADS || !threads[target].in_use) {
        if (noisy > 0) {
            noisy--;
            printf("[isr] ChangeThFromISR: bad tid %d\n", target);
        }
        return 0;
    }
    if (noisy > 0) {
        noisy--;
        printf("[isr] resume tid %d (was %d)\n", target, current_thread);
    }
    /* mts is cooperative: exactly one of its tasks may be runnable. Resuming
     * the target without parking the outgoing one leaves two of them runnable
     * on a preemptive RTOS, they trample each other's scheduler state, and
     * nothing progresses. The vblank tick is not an mts task, so it is free to
     * suspend the outgoing one on their behalf. */
    if (current_thread >= 0 && current_thread < MGS_MAX_THREADS &&
        current_thread != target && threads[current_thread].handle) {
        vTaskSuspend(threads[current_thread].handle);
        /* swap the per-task interrupt mask exactly like ChangeTh does */
        threads[current_thread].crit = psyz_critical_depth;
    }
    psyz_critical_depth = threads[target].crit;
    current_thread = target;
    vTaskResume(threads[target].handle);
    return 1;
}

/* a parking spot for game tasks the port retires (e.g. the SIO console) */
void Mgs_TaskSleepForever(void) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#ifdef MGS_ESPBOX
/* esp-box-emu: pause / resume / stop ---------------------------------------
 * The emulator menu suspends the game: only the running PSX thread is live
 * (the others park themselves in ChangeTh), so that is the one to suspend and
 * resume. Stop deletes every task the scheduler created and forgets the slots;
 * the task that bootstrapped the game (adopted in ChangeTh, entry == 0) is
 * owned by the caller and is skipped. */
static int paused_thread = -1;

void Mgs_ThreadsPause(void) {
    paused_thread = current_thread;
    if (paused_thread >= 0 && paused_thread < MGS_MAX_THREADS && threads[paused_thread].handle) {
        vTaskSuspend(threads[paused_thread].handle);
    }
}

void Mgs_ThreadsResume(void) {
    if (paused_thread >= 0 && paused_thread < MGS_MAX_THREADS && threads[paused_thread].handle &&
        threads[paused_thread].in_use) {
        vTaskResume(threads[paused_thread].handle);
    }
    paused_thread = -1;
}

void Mgs_ThreadsStopAll(void) {
    int i;
    for (i = 0; i < MGS_MAX_THREADS; i++) {
        if (threads[i].handle && threads[i].entry) {
            vTaskDelete(threads[i].handle);
        }
        free(threads[i].stack);
        free(threads[i].tcb);
        threads[i].handle = 0;
        threads[i].stack = NULL;
        threads[i].tcb = NULL;
        threads[i].entry = 0;
        threads[i].in_use = 0;
        threads[i].crit = 0;
    }
    current_thread = -1;
    change_in_flight = 0;
    paused_thread = -1;
}
#endif /* MGS_ESPBOX */
