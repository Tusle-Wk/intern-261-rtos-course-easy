/* DWT cycle counter — the only honest way to burn a known number of
 * microseconds of CPU inside a task without blocking.
 */
#include "stm32f1xx.h"
#include "tasks.h"

void dwt_init(void)
{
    /* TODO 1: enable the trace subsystem (CoreDebug DEMCR, TRCENA bit),
     *         zero DWT->CYCCNT, then enable CYCCNTENA.
     *         PM0056 does not document DWT; see the ARMv7-M ARM, C1.8. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // bat TRCENA (Trace enable) trong thanh ghi DEMCR
    DWT->CYCCNT=0; //reset bo dem chu ky ve 0
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void dwt_busy_wait_us(uint32_t microseconds)
{
    /* TODO 2: spin until `microseconds` worth of CPU cycles have elapsed.
     *         SystemCoreClock is 72000000. Handle CYCCNT wrap-around correctly
     *         — unsigned subtraction, not a comparison of absolute values.
     *         Explain in REPORT.md why the naive comparison breaks every ~59 s. */

    //systemcore clock la 72Mhz -> 1us=72cycles
    uint32_t start = DWT->CYCCNT;
    uint32_t wait_cycles = microseconds * (SystemCoreClock / 1000000); //72 cycle
    while ((DWT->CYCCNT - start) < wait_cycles) {
        // busy-wait, khong lam gi, khong nhuong cpu
    }
}
