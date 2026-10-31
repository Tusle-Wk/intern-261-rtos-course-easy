/* Week 2 / Ex4 — event group sync across EVENT_BIT_COUNT producers, with
 * producer_0's signal optionally replaced by a task notification.
 */


/* Bản build 1 (baseline): PRODUCER0_USE_NOTIFY = 0 -> event group đủ 3 bit
 * Bản build 2 (notify):   PRODUCER0_USE_NOTIFY = 1 -> bit0 = task notification,
 *                                                     bit1..bit(N-1) = event group
 */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "main.h"
#include "params.h"
#include "sync_tasks.h"


//Tham số tinh chỉnh (có thể ghi đè bằng -D trên dòng lệnh biên dịch)

#ifndef SYNC_PRINT_EVERY
#define SYNC_PRINT_EVERY   50u   //cứ N lần đồng bộ thành công thì in 1 dòng + thống kê
#endif

#ifndef TEST_SLOW_PRODUCER
#define TEST_SLOW_PRODUCER 0     // 1 = thỉnh thoảng cố tình làm 1 producer chậm để gây timeout (REPORT §4)
#endif
#define SLOW_PRODUCER_IDX   (EVENT_BIT_COUNT - 1)  //producer bị làm chậm 
#define SLOW_PRODUCER_EVERY 40u                    //cứ N vòng thì chậm 1 lần 

#ifndef MEASURE_SEM_REF
#define MEASURE_SEM_REF    0     //1 = đo thêm RAM của 1 binary semaphore để làm số tham chiếu
#endif
#if MEASURE_SEM_REF
#include "semphr.h"
#endif

/* Chân probe tùy chọn cho logic analyzer. Gắn vào 1 GPIO còn rảnh, ví dụ
 *   -DPROBE_T0()='HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0)' hoặc sửa trực tiếp ở đây
 * Mặc định để rỗng nên không tốn cycle. */
#ifndef PROBE_T0
#define PROBE_T0() do { } while (0)
#endif
#ifndef PROBE_T1
#define PROBE_T1() do { } while (0)
#endif


// Hằng số / kiểm tra lúc biên dịch                                    
#define ALL_BITS_MASK ((1u << EVENT_BIT_COUNT) - 1u)   //ví dụ 3 bit -> 0b111 
#define EG_BITS_MASK  (ALL_BITS_MASK & ~1u)            // các bit còn lại trên event group khi producer_0 chuyển sang notify -> 0b110

_Static_assert(EVENT_BIT_COUNT >= 2 && EVENT_BIT_COUNT <= 8,
               "EVENT_BIT_COUNT phai trong 2..8 (8 bit luon hop le ke ca khi tick 16-bit)");

#define CYC() (DWT->CYCCNT)   //đọc bộ đếm cycle của DWT 


// Trạng thái toàn cục

static EventGroupHandle_t s_events;   // handle event group dùng chung
void *sync_task_handle;               // handle của sync_task (producer_0 cần khi dùng notify) 

/* t0 = thời điểm (cycle DWT) producer i gọi báo hiệu gần nhất.
 * Producer i ghi, sync_task đọc, nên phải là volatile. */
static volatile uint32_t s_t0[EVENT_BIT_COUNT];

/* Được điền trong sync_init(), được in ra bởi sync_task. Cố ý không printf
 * trước khi scheduler chạy. */
static uint32_t s_eg_struct_bytes;   // sizeof(StaticEventGroup_t)                     
static uint32_t s_eg_heap_bytes;     // chênh lệch free heap quanh xEventGroupCreate() 
#if MEASURE_SEM_REF
static uint32_t s_sem_struct_bytes;  // sizeof(StaticSemaphore_t)                      
static uint32_t s_sem_heap_bytes;    // chênh lệch free heap khi tạo binary semaphore  
#endif


// Hàm phụ                                                             


// Bật bộ đếm cycle DWT (Cortex-M3 có DWT->CYCCNT)
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  //bật khối DWT/ITM   
    DWT->CYCCNT = 0;                                 //xóa bộ đếm         
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;            //cho bộ đếm chạy     
}

/* Đổi cycle -> micro giây nhân 100 (2 chữ số thập phân mà không cần printf float).
 * Ví dụ 72 MHz: 72 cycle = 1 us. */
static uint32_t cyc_to_us_x100(uint32_t cyc)
{
    uint32_t cyc_per_us = SystemCoreClock / 1000000u;
    return (uint32_t)(((uint64_t)cyc * 100u) / cyc_per_us);
}

// Bộ sinh số giả ngẫu nhiên xorshift32 (state phải khác 0)
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Thống kê độ trễ: số mẫu, nhỏ nhất, lớn nhất, tổng (64 bit để không tràn).
typedef struct {
    uint32_t n;
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} lat_stat_t;

#define LAT_STAT_INIT { 0u, UINT32_MAX, 0u, 0u }

// Thêm 1 mẫu vào thống kê.
static void stat_add(lat_stat_t *s, uint32_t v)
{
    s->n++;
    s->sum += v;
    if (v < s->min) { s->min = v; }
    if (v > s->max) { s->max = v; }
}

// In thống kê ra UART: số mẫu, min/avg/max theo cycle, avg theo us
static void stat_print(const char *name, const lat_stat_t *s)
{
    if (s->n == 0u) {
        printf("  %-10s n=0\r\n", name);
        return;
    }
    uint32_t avg = (uint32_t)(s->sum / s->n);
    printf("  %-10s n=%lu  min=%lu  avg=%lu  max=%lu cyc | avg=%lu.%02lu us\r\n",
           name, (unsigned long)s->n, (unsigned long)s->min,
           (unsigned long)avg, (unsigned long)s->max,
           (unsigned long)(cyc_to_us_x100(avg) / 100u),
           (unsigned long)(cyc_to_us_x100(avg) % 100u));
}

/* Độ trễ đánh thức khi chờ bằng event group.
 * Lệnh gọi hoàn tất điều kiện chờ là của producer có t0 MUỘN NHẤT trước t_wake
 * (tức hiệu t_wake - t0 nhỏ nhất). Mẫu chỉ hợp lệ nếu t0 đó xảy ra SAU khi
 * sync_task đã vào hàm chờ (t_enter): vì sync_task có priority cao nhất nên
 * producer chỉ chạy được khi sync_task đang bị block, do đó t0 > t_enter nghĩa
 * là sync_task thật sự đang block và được chính lệnh gọi này đánh thức. */

static void record_eg_latency(lat_stat_t *st, uint32_t t_enter,
                              uint32_t t_wake, uint32_t mask)
{
    uint32_t best   = UINT32_MAX;
    uint32_t best_i = 0u;

    for (uint32_t i = 0u; i < EVENT_BIT_COUNT; i++) {
        if ((mask & (1u << i)) == 0u) {
            continue;                    // bỏ qua producer không thuộc mask đang xét 
        }
        uint32_t d = t_wake - s_t0[i];   // trừ không dấu: vẫn đúng khi CYCCNT tràn 
        if (d < best) {
            best   = d;
            best_i = i;
        }
    }
    //So sánh có dấu để xử lý đúng khi bộ đếm 32-bit quay vòng
    if (best != UINT32_MAX && (int32_t)(s_t0[best_i] - t_enter) > 0) {
        stat_add(st, best);
    }
}


// sync_init

void sync_init(void)
{
    dwt_init();

    /* TODO 1: tạo event group và đo RAM nó chiếm.
     * Hiệu free heap trước/sau = số byte thật sự bị lấy khỏi heap
     * (gồm cả header của heap_4 và phần làm tròn).
     *
     * LƯU Ý: heap_4 khởi tạo "lười" ở lần pvPortMalloc đầu tiên; trước đó
     * xPortGetFreeHeapSize() trả về 0 và phép trừ bên dưới sẽ ra số rác.
     * Nên cấp phát rồi giải phóng thử 1 byte để "làm nóng" heap trước khi đo
     * (free sẽ gộp khối lại, free heap trở về đúng giá trị ban đầu). */
    void *warmup = pvPortMalloc(1u);
    if (warmup != NULL) {
        vPortFree(warmup);
    }

    size_t heap_before = xPortGetFreeHeapSize();
    s_events = xEventGroupCreate();
    size_t heap_after = xPortGetFreeHeapSize();

    if (s_events == NULL) {            /* heap quá nhỏ: dừng tại đây, thấy rõ trong debugger */
        taskDISABLE_INTERRUPTS();
        for (;;) {
        }
    }

    /* StaticEventGroup_t có kích thước đúng bằng EventGroup_t (private trong
     * event_groups.c), nên sizeof của nó chính là kích thước cấu trúc. */
    s_eg_struct_bytes = (uint32_t)sizeof(StaticEventGroup_t);
    s_eg_heap_bytes   = (uint32_t)(heap_before - heap_after);

#if MEASURE_SEM_REF
    /* Số tham chiếu: 1 binary semaphore tốn bao nhiêu heap (tạo xong xóa ngay). */
    heap_before = xPortGetFreeHeapSize();
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    heap_after = xPortGetFreeHeapSize();
    s_sem_struct_bytes = (uint32_t)sizeof(StaticSemaphore_t);
    s_sem_heap_bytes   = (uint32_t)(heap_before - heap_after);
    if (sem != NULL) {
        vSemaphoreDelete(sem);
    }
#endif
}


// producer_task

void producer_task(void *argument)
{
    uint32_t idx = (uint32_t)(uintptr_t)argument;   // chỉ số producer 0..N-1

    // mỗi task một seed riêng; xorshift không được có state = 0
    uint32_t rng = (0x9E3779B9u * (idx + 1u)) ^ 0xA5A5A5A5u;
    if (rng == 0u) {
        rng = 1u;
    }
#if TEST_SLOW_PRODUCER
    uint32_t loops = 0u;
#endif

    for (;;) {
        /* TODO 2: chu kỳ ngẫu nhiên nhỏ, từ NOTIFY_PERIOD_MS đến 2*NOTIFY_PERIOD_MS-1 ms
         * (ví dụ NOTIFY_PERIOD_MS = 5 -> 5..9 ms). */
        uint32_t delay_ms = NOTIFY_PERIOD_MS + (xorshift32(&rng) % NOTIFY_PERIOD_MS);

#if TEST_SLOW_PRODUCER
        /* Cố ý trễ hơn SYNC_TIMEOUT_MS để sync_task bị timeout. */
        if (idx == SLOW_PRODUCER_IDX && (++loops % SLOW_PRODUCER_EVERY) == 0u) {
            delay_ms = SYNC_TIMEOUT_MS + 30u;
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

#if PRODUCER0_USE_NOTIFY
        if (idx == 0) {
            /* TODO 3: producer_0 báo bằng task notification.
             * Thứ tự: probe GPIO -> lấy t0 -> gọi API, để probe không cộng vào số đo. */
            PROBE_T0();
            s_t0[0] = CYC();                                   /* t0 */
            xTaskNotifyGive((TaskHandle_t)sync_task_handle);
        } else
#endif
        {
            /* TODO 4: các producer còn lại báo bằng cách set bit riêng của mình
             * trên event group (bit idx), cùng kỷ luật timestamp như trên. */
            PROBE_T0();
            s_t0[idx] = CYC();                                 /* t0 */
            (void)xEventGroupSetBits(s_events, (EventBits_t)(1u << idx));
        }
    }
}

// sync_task

void sync_task(void *argument)
{
    (void)argument;

    lat_stat_t st_eg = LAT_STAT_INIT;   // độ trễ đánh thức qua bit của event group
#if PRODUCER0_USE_NOTIFY
    lat_stat_t st_nt = LAT_STAT_INIT;   // độ trễ đánh thức qua task notification
#endif

    uint32_t n_ok = 0u, n_timeout = 0u;  // số lần đồng bộ thành công / timeout
    uint32_t t_prev = 0u;                // thời điểm đồng bộ lần trước (cycle)
    bool     have_prev = false;          // lần trước có thành công không (để tính chu kỳ)
    uint64_t period_sum = 0u;            // tổng chu kỳ đo được (cycle)
    uint32_t period_n = 0u;              // số chu kỳ đã đo
    uint32_t last_period = 0u;           // chu kỳ gần nhất (cycle)

    // Banner đầu: thông tin cấu hình và số đo RAM
    printf("\r\n[ex4] id=%s mode=EG(%u)%s bits=%u period=%ums timeout=%ums clk=%luHz\r\n",
           STUDENT_ID,
           (unsigned)(PRODUCER0_USE_NOTIFY ? (EVENT_BIT_COUNT - 1u) : EVENT_BIT_COUNT),
           PRODUCER0_USE_NOTIFY ? "+NOTIFY(1)" : "",
           (unsigned)EVENT_BIT_COUNT, (unsigned)NOTIFY_PERIOD_MS,
           (unsigned)SYNC_TIMEOUT_MS, (unsigned long)SystemCoreClock);
    printf("[ram] sizeof(StaticEventGroup_t)=%lu B, heap delta of xEventGroupCreate=%lu B, "
           "free heap now=%lu B\r\n",
           (unsigned long)s_eg_struct_bytes, (unsigned long)s_eg_heap_bytes,
           (unsigned long)xPortGetFreeHeapSize());
#if MEASURE_SEM_REF
    printf("[ram] ref: sizeof(StaticSemaphore_t)=%lu B, heap delta of binary semaphore=%lu B\r\n",
           (unsigned long)s_sem_struct_bytes, (unsigned long)s_sem_heap_bytes);
#endif

    for (;;) {
        bool     all_ok;      // true = đủ mọi nguồn; false = timeout
        uint32_t t_sync;      // thời điểm toàn bộ điều kiện được thỏa
        uint32_t ret_bits;    // giá trị bit trả về (dùng cho log timeout)
        uint32_t missing;     // bit còn thiếu khi timeout

#if PRODUCER0_USE_NOTIFY
        // TODO 5 — bước A: phần đóng góp của producer_0 qua notification
        uint32_t t_enter = CYC();   // lúc bắt đầu chờ 
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SYNC_TIMEOUT_MS));
        uint32_t t_woke = CYC();    // t1: lúc sync_task chạy lại
        PROBE_T1();

        if (notified == 0u) {
            // Hết hạn mà không có notify: ulTaskNotifyTake trả 0
            all_ok   = false;
            ret_bits = 0u;
            missing  = 1u;                         // bit 0 = producer_0
            t_sync   = CYC();
            // Bỏ vòng dở dang để bit cũ không ghép với sự kiện vòng sau. 
            (void)xEventGroupClearBits(s_events, EG_BITS_MASK);
        } else {
            // Mẫu độ trễ hợp lệ: đúng 1 notify và nó được gửi SAU khi ta đã block
            if (notified == 1u && (int32_t)(s_t0[0] - t_enter) > 0) {
                stat_add(&st_nt, t_woke - s_t0[0]);
            }

            // Bước B: chờ các producer còn lại qua bit của event group
            uint32_t t_enter_eg = CYC();
            EventBits_t bits = xEventGroupWaitBits(s_events, EG_BITS_MASK,
                                                   pdTRUE,   // clear-on-exit 
                                                   pdTRUE,   // chờ TẤT CẢ bit 
                                                   pdMS_TO_TICKS(SYNC_TIMEOUT_MS));
            uint32_t t_woke_eg = CYC();
            t_sync   = t_woke_eg;
            ret_bits = (uint32_t)bits;
            // Đủ bit hay timeout? Phải so với mask, không được kiểm tra "bits != 0".
            all_ok   = ((bits & EG_BITS_MASK) == EG_BITS_MASK);
            missing  = EG_BITS_MASK & ~(uint32_t)bits;

            if (all_ok) {
                record_eg_latency(&st_eg, t_enter_eg, t_woke_eg, EG_BITS_MASK);
            } else {
                (void)xEventGroupClearBits(s_events, EG_BITS_MASK);
            }
        }
#else
        /* TODO 6: chờ đủ mọi bit trên event group. */
        uint32_t t_enter = CYC();   // lúc bắt đầu chờ
        EventBits_t bits = xEventGroupWaitBits(s_events, ALL_BITS_MASK,
                                               pdTRUE,   // clear-on-exit  
                                               pdTRUE,   // chờ TẤT CẢ bit 
                                               pdMS_TO_TICKS(SYNC_TIMEOUT_MS));
        uint32_t t_woke = CYC();    // t1: lúc sync_task chạy lại
        PROBE_T1();

        t_sync   = t_woke;
        ret_bits = (uint32_t)bits;
        // Timeout vẫn có thể trả về giá trị khác 0 (ví dụ 0b101), nên phải so với mask
        all_ok   = ((bits & ALL_BITS_MASK) == ALL_BITS_MASK);
        missing  = ALL_BITS_MASK & ~(uint32_t)bits;

        if (all_ok) {
            record_eg_latency(&st_eg, t_enter, t_woke, ALL_BITS_MASK);
        } else {
            /* Khi timeout, bit đã set KHÔNG bị xóa (clear-on-exit chỉ áp dụng khi
             * điều kiện thỏa) -> xóa tay để bắt đầu vòng mới sạch. */
            (void)xEventGroupClearBits(s_events, ALL_BITS_MASK);
        }
#endif

        /* TODO 7: in thời điểm đồng bộ và chu kỳ đo được ra UART. */
        if (!all_ok) {
            n_timeout++;
            have_prev = false;      // chu kỳ băng qua 1 lần timeout là vô nghĩa
            printf("[sync] TIMEOUT t=%lu ms ret=0x%02lX missing=0x%02lX (timeouts=%lu)\r\n",
                   (unsigned long)((uint64_t)xTaskGetTickCount() * 1000u / configTICK_RATE_HZ),
                   (unsigned long)ret_bits, (unsigned long)missing,
                   (unsigned long)n_timeout);
            continue;
        }

        n_ok++;
        if (have_prev) {            // chỉ tính chu kỳ khi lần trước cũng thành công
            last_period = t_sync - t_prev;
            period_sum += last_period;
            period_n++;
        }
        t_prev    = t_sync;
        have_prev = true;

        // In thưa để UART không làm lệch phép đo (115200 baud: ~5 ms cho 60 ký tự)
        if ((n_ok % SYNC_PRINT_EVERY) == 0u && period_n != 0u) {
            uint32_t avg_period = (uint32_t)(period_sum / period_n);
            printf("[sync #%lu] t=%lu ms  period=%lu.%02lu us (avg %lu.%02lu us)\r\n",
                   (unsigned long)n_ok,
                   (unsigned long)((uint64_t)xTaskGetTickCount() * 1000u / configTICK_RATE_HZ),
                   (unsigned long)(cyc_to_us_x100(last_period) / 100u),
                   (unsigned long)(cyc_to_us_x100(last_period) % 100u),
                   (unsigned long)(cyc_to_us_x100(avg_period) / 100u),
                   (unsigned long)(cyc_to_us_x100(avg_period) % 100u));
            stat_print("EG-bit", &st_eg);
#if PRODUCER0_USE_NOTIFY
            stat_print("NOTIFY", &st_nt);
#endif
        }
    }
}