# Report — Week 2 / Ex4

`student_id`: 2413858    Commit của PREDICTION.md: 897b64c

> Tham số: `EVENT_BIT_COUNT` = 3, `NOTIFY_PERIOD_MS` = 5, `SYNC_TIMEOUT_MS` = 50. Số đo lấy từ STM32F103 (Blue Pill) chạy 72 MHz, in ra UART 230400 baud, mỗi lần chạy khoảng 60 s. Log gốc đi kèm:
> `buildA.txt` (3 điểm event group, `PRODUCER0_USE_NOTIFY=0`), `buildB.txt` (2 event group + notification cho producer_0, `PRODUCER0_USE_NOTIFY=1`), `buildC.txt` (như A + `TEST_SLOW_PRODUCER=1`), `buildD.txt` (như A nhưng `EVENT_BIT_COUNT` = 4). Cả bốn bản đều có `MEASURE_SEM_REF=1`.

## 1. Bảng so sánh RAM
| | Bytes thực tế | Cách tính |
|---|---|---|
| Event group (3 điểm đồng bộ) | 32 B lấy từ heap (cấu trúc 24 B) | `sizeof(StaticEventGroup_t)` = 24 B; chênh free heap quanh `xEventGroupCreate()` = 32 B (24 B + header 8 B của heap_4, đã là bội của 8). Nguồn: dòng `[ram]` trong buildA.txt |
| Event group (2 điểm) + notification (1 điểm) | 32 B lấy từ heap (cấu trúc 24 B) + 0 B cho notification | Vẫn còn 1 event group chờ bit 1 và bit 2 nên 24 B / 32 B như trên; giá trị notification nằm sẵn trong TCB. Free heap sau khi tạo xong mọi task: 7624 B ở buildB.txt, đúng bằng 7624 B ở buildA.txt |
| Chênh lệch | 0 B | 7624 B − 7624 B = 0 B. Số bit không làm đổi kích thước event group vì mọi bit nằm chung một từ `uxEventBits` |

**Tham chiếu semaphore.** Cùng chương trình tạo thử một binary semaphore rồi xóa ngay (`MEASURE_SEM_REF`): `sizeof(StaticSemaphore_t)` = 72 B, chênh free heap = 80 B (72 B + header 8 B). Vậy thay một điểm đồng bộ bằng binary semaphore tốn thêm 80 B heap, còn thay bằng notification tốn 0 B, tiết kiệm 80 B cho mỗi điểm.

**Vì sao notification không cấp phát thêm RAM nhưng có giới hạn.** Giá trị notification (`ulNotifiedValue`, 4 B) và trạng thái (`ucNotifyState`, 1 B) là trường nằm trong TCB, được cấp phát cùng TCB lúc `xTaskCreate` dù có dùng hay không (`configUSE_TASK_NOTIFICATIONS` = 1). Nên dùng notification không sinh thêm object nào. Đổi lại: mỗi task chỉ có `configTASK_NOTIFICATION_ARRAY_ENTRIES` = 1 giá trị notification (cấu hình hiện tại), chỉ chính task đó chờ được trên giá trị của nó, và muốn thêm index thì mỗi index tốn thêm 4 B + 1 B trong TCB của mọi task, kể cả task không dùng.

**Ghi chú về cách đo.** Dùng `sizeof` cấu trúc tĩnh và chênh free heap thực tế (có chạy `pvPortMalloc`/`vPortFree` thử 1 byte trước khi đo, vì heap_4 chỉ khởi tạo ở lần cấp phát đầu tiên và trước đó `xPortGetFreeHeapSize()` trả về 0).

## 2. Bảng so sánh tốc độ đánh thức
| Cách | Thời gian đánh thức (cycle) | Thời gian đánh thức (us) |
|---|---|---|
| Event-group-bit (producer_1 hoặc 2) | 1054 | 14.63 |
| Task notification (producer_0) | 887 | 12.31 |

Số liệu từ cùng một lần chạy (buildB.txt, khối cuối `[sync #8100]`, t = 60.1 s): event group n = 7416 mẫu, notification n = 3890 mẫu. Notification nhanh hơn 167 cycle (2.32 us), tỉ lệ 1054 / 887 = 1.19. Chỉ so sánh hai dòng trong cùng một lần chạy; các bản build khác cho event group: 1051 cycle (buildA), 1073 cycle (buildC, giá trị avg), 1076 cycle (buildD), tức dao động khoảng 2% giữa các bản build. Nhiều khả năng do vị trí code trong Flash thay đổi: có lần chỉ sửa chuỗi `printf` ở banner, không đụng vào phần đo, mà độ trễ event group đã đổi từ 1051 sang 1076 cycle.

Mô tả cách đo (2 điểm DWT: lúc gọi set/notify, lúc sync_task bắt đầu chạy):

- Bộ đếm `DWT->CYCCNT` được bật trong `sync_init()` (`DEMCR.TRCENA`, `DWT_CTRL.CYCCNTENA`). Ở 72 MHz, 72 cycle = 1 us.
- **t0:** producer đọc `DWT->CYCCNT` ngay trước khi gọi `xEventGroupSetBits` (producer_1, 2) hoặc `xTaskNotifyGive` (producer_0), lưu vào `s_t0[idx]`.
- **t1:** `sync_task` đọc `DWT->CYCCNT` ngay khi hàm chờ trả về (`xEventGroupWaitBits` hoặc `ulTaskNotifyTake`). Độ trễ = t1 − t0, phép trừ `uint32_t` không dấu nên vẫn đúng khi bộ đếm tràn.
- **Priority:** `sync_task` (idle + 2) cao hơn producer (idle + 1), nên ngay khi được đánh thức nó chiếm CPU. Khoảng t1 − t0 vì thế gồm: thời gian chạy hàm báo hiệu, chuyển ngữ cảnh qua PendSV, và phần kết của hàm chờ. Không gồm thời gian chờ.
- **Lọc mẫu:** chỉ nhận mẫu khi t0 xảy ra sau lúc `sync_task` bắt đầu chờ (`(int32_t)(t0 − t_enter) > 0`), tức là `sync_task` thật sự đang block và được chính lệnh báo hiệu đó đánh thức. Với event group, mẫu lấy từ producer có t0 muộn nhất (producer hoàn tất điều kiện chờ). Với notification, đòi thêm `notified == 1`. Vì vậy số mẫu notification chỉ bằng 48% số lần đồng bộ (3890 / 8100): khi producer_0 báo lúc `sync_task` chưa nằm trong lệnh chờ notification (ví dụ đang chờ bit ở bước B), notification nằm chờ sẵn và lần đó không đo được. Event group đạt 92% (7416 / 8100); phần còn lại là các lần cả hai bit đã set sẵn trước khi `sync_task` vào lệnh chờ.
- **In thưa:** `sync_task` chỉ in mỗi 50 lần đồng bộ thành công (thống kê min / avg / max tích lũy), nên UART nằm ngoài cửa sổ đo.
- **Kiểm tra chéo cách đổi cycle sang us:** chu kỳ trung bình đo bằng DWT so với tính từ tick hệ thống: buildA 7464.55 us so với 7466.38 us (59731 ms / 8000 lần), lệch 0.024%; buildB lệch 0.026%; buildD lệch 0.024%.
- **Hạn chế:** (1) không dùng logic analyzer (macro `PROBE_T0/T1` để rỗng), nên chưa tách riêng được từng thành phần của 887 và 1054 cycle. (2) min = avg = max ở hầu hết các khối, tức gần như không có jitter, vì mỗi producer thức dậy ngay sau tick và đi cùng một đường code mỗi lần; số đo này không phản ánh nhiễu từ ngắt bất đồng bộ. Chỉ có một ngoại lệ: `NOTIFY max` nhảy lên 1155 cycle từ khối `#3850` (t = 28.5 s), tức một mẫu cao hơn 268 cycle (3.7 us), có thể do một ngắt (SysTick hoặc TIM4) rơi trúng cửa sổ đo; avg không bị ảnh hưởng đáng kể vì chỉ 1 mẫu trong hơn 1900 mẫu. (3) mỗi lần in UART chiếm `sync_task` vài ms (polling), nên chu kỳ đồng bộ ngay sau mỗi lần in bị méo, ảnh hưởng khoảng 2% số mẫu chu kỳ (1 trong 50).

## 3. Khi nào KHÔNG dùng được notification
Tình huống bạn dự đoán ở PREDICTION.md, kiểm chứng lại: đúng hay sai, vì sao.

**Dự đoán (PREDICTION.md ý 5):** không thể thay hoàn toàn event group bằng notification khi (a) nhiều task cùng chờ một sự kiện (broadcast), (b) rendezvous nhiều-nhiều, (c) cần biết nguồn nào đã báo (notification dạng đếm chỉ cộng dồn). **Kết luận: đúng**, kiểm chứng như sau.

1. **Một người nhận duy nhất.** Trong code, producer_0 phải gọi `xTaskNotifyGive((TaskHandle_t)sync_task_handle)`: API nhận đúng một handle task đích, và chỉ task đó chờ được trên giá trị notification của chính nó (`ulTaskNotifyTake` đọc notification của task đang gọi). Muốn hai task cùng chờ "producer_0 đã báo", producer_0 phải biết cả hai handle và gọi hai lần, hai lần gọi này không nguyên tử. Ngược lại `xEventGroupSetBits` chỉ cần gọi một lần: kernel duyệt toàn bộ danh sách `xTasksWaitingForBits` và đánh thức mọi task có điều kiện thỏa, người gửi không cần biết ai đang chờ. Đây là broadcast mà notification không có.
2. **Nguồn được phân biệt nhờ bit riêng.** Ở buildC (mục 4), dòng log `ret=0x03 missing=0x04` chỉ ra đúng producer_2 là nguồn thiếu, chỉ vì mỗi producer có một bit riêng. Nếu cả ba producer cùng dùng `xTaskNotifyGive` rồi chờ bằng `ulTaskNotifyTake`, kết quả chỉ là một số đếm: một producer báo 3 lần cũng cho count = 3, giống ba producer báo mỗi nguồn một lần, nên không biết nguồn nào thiếu.
3. **Không biểu diễn được "đủ tất cả" trong một lệnh chờ.** Trong buildB, producer_1 và producer_2 vẫn phải dùng event group vì điều kiện "cả hai bit cùng set" (wait-all, clear-on-exit) do `xEventGroupWaitBits` làm trong một lệnh; `ulTaskNotifyTake` không có khái niệm này. Hệ quả thiết kế thấy ngay trong code: `sync_task` phải chờ tuần tự (bước A chờ notification, bước B chờ event group), nên thứ tự chờ cố định và, trong trường hợp xấu nhất, timeout cộng dồn tới 2 × 50 ms trước khi báo lỗi, trong khi bản chỉ dùng event group chỉ chờ tối đa 50 ms trong một lệnh. Đây là suy ra từ cấu trúc code, chưa đo.
4. **Rendezvous nhiều-nhiều:** mỗi task vừa báo vừa chờ tất cả các task còn lại. Event group làm được bằng `xEventGroupSync` (set bit của mình và chờ các bit còn lại trong một lệnh). Với notification, mỗi task phải gửi riêng cho N − 1 task còn lại và tự đếm.
5. **Chưa kiểm chứng:** biến thể `xTaskNotify(..., eSetBits)` (mỗi producer set một bit trong giá trị notification, nên với đúng một task chờ vẫn phân biệt được nguồn). Bài này không chạy thử, chỉ suy ra từ API; khi đó vẫn phải tự viết vòng kiểm tra "đủ bit chưa" và vẫn chỉ có một task nhận.

## 4. Timeout
Log/ảnh một lần `sync_task` bị timeout (cố tình làm 1 producer chậm hơn
`SYNC_TIMEOUT_MS`), và giá trị trả về của `xEventGroupWaitBits` lúc đó.

Cấu hình: buildC (`TEST_SLOW_PRODUCER=1`). Producer cuối (producer_2, bit 2) cứ mỗi 40 vòng ngủ `SYNC_TIMEOUT_MS + 30` = 80 ms thay vì 5 đến 9 ms, trong khi `sync_task` chỉ chờ tối đa 50 ms. Trích buildC.txt (từ đầu log):

```
[sync] TIMEOUT t=344 ms ret=0x03 missing=0x04 (timeouts=1)
[sync #50] t=462 ms  period=9000.04 us (avg 7583.33 us)
  EG-bit     n=50  min=1074  avg=1074  max=1074 cyc | avg=14.91 us
[sync] TIMEOUT t=698 ms ret=0x03 missing=0x04 (timeouts=2)
```

- **Giá trị trả về của `xEventGroupWaitBits` khi timeout là `0x03`**, tức giá trị hiện tại của event group (bit 0 và bit 1 đã set, bit 2 chưa), không phải 0 và không phải mã lỗi. Chờ `0b111` (wait-all) không thỏa nên hàm trả về khi hết 50 ms.
- Vì điều kiện chờ không thỏa, **clear-on-exit không xảy ra**: hai bit 0 và 1 vẫn còn trong event group. Code phải tự gọi `xEventGroupClearBits` để vòng sau không ghép bit cũ với sự kiện mới.
- **Phân biệt đủ bit và timeout** bằng `(ret & ALL_BITS_MASK) == ALL_BITS_MASK`; bit thiếu = `ALL_BITS_MASK & ~ret` = `0x04`. Không được kiểm tra `ret != 0`, vì giá trị `0x03` ở đây khác 0 nhưng vẫn là timeout.
- **Thống kê toàn log:** 168 dòng TIMEOUT trong 59.8 s, cả 168 dòng đều `ret=0x03 missing=0x04`, không có dòng nào khác. Khoảng cách trung bình giữa hai lần timeout 356 ms (334 đến 381 ms), khớp với tính toán: 39 vòng × khoảng 7 ms + 80 ms ≈ 353 ms. Sau mỗi timeout, `sync_task` đồng bộ lại bình thường (6450 lần đồng bộ thành công xen kẽ).

## 5. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân vật lý.

| Mục (PREDICTION.md) | Dự đoán | Đo được | Kết luận |
|---|---|---|---|
| RAM event group (ý 1) | 24 B cấu trúc, 32 B heap | 24 B, 32 B | khớp |
| Notification thêm RAM (ý 2) | 0 B | 0 B (free heap 7624 = 7624) | khớp |
| Binary semaphore tham chiếu (ý 2) | khoảng 80 B cấu trúc, ~88 B heap | 72 B, 80 B | lệch −8 B ở cả hai |
| Độ trễ notification (ý 3) | khoảng 450 cycle (~6 us) | 887 cycle (12.31 us) | lệch, gấp khoảng 2 lần |
| Độ trễ event group (ý 3) | 600 đến 700 cycle (~9 đến 10 us) | 1054 cycle (14.63 us) | lệch, gấp 1.5 đến 1.76 lần |
| Tỉ lệ event group / notification (ý 3) | 1.3 đến 1.5 | 1.19 | hướng đúng, độ lớn thấp hơn |
| Hành vi khi timeout (ý 4) | trả về bit hiện tại, không phải 0 | `ret=0x03`, 168 / 168 lần | khớp |
| Tình huống không dùng notification (ý 5) | broadcast, rendezvous, mất nguồn | xem mục 3 | khớp |
| Chu kỳ đồng bộ khi N = 4 (mục 6) | ≈ 7.58 ms ± 0.1 | 7.557 ms | khớp (−0.26%) |

**Nguyên nhân các mục lệch.**

1. **Semaphore nhỏ hơn dự đoán 8 B.** Dự đoán là ước lượng thô ("khoảng 80 B"), không cộng từng trường. Cộng đúng theo `Queue_t` của cấu hình này: `pcHead` 4 + `pcWriteTo` 4 + union 8 + hai `List_t` (20 + 20) + `uxMessagesWaiting` / `uxLength` / `uxItemSize` (12) + `cRxLock` / `cTxLock` (2) = 70 B, làm tròn lên 72 B vì căn lề 4 B. Cấu hình hiện tại `configSUPPORT_STATIC_ALLOCATION` = 0, `configUSE_TRACE_FACILITY` = 0 và không bật queue set, nên không có các trường `ucStaticallyAllocated`, `uxQueueNumber`, `pxQueueSetContainer`. Cộng header heap 8 B ra 80 B, trùng số đo.
2. **Độ trễ tuyệt đối lớn hơn dự đoán khoảng 1.5 đến 2 lần.** Dự đoán chỉ nghĩ đến "đường đi trong kernel" của từng API và coi phần chung (chuyển ngữ cảnh) là nhỏ. Thực tế khoảng t1 − t0 gồm cả phần chung đó: vào / ra ngoại lệ PendSV, lưu và khôi phục ngữ cảnh, `vTaskSwitchContext` chọn task bằng vòng lặp C (vì `configUSE_PORT_OPTIMISED_TASK_SELECTION` = 0, không dùng lệnh CLZ), và phần kết của hàm chờ. Thêm nữa, Flash của F103 ở 72 MHz cần 2 wait-state, mỗi lần rẽ nhánh hoặc nạp lệnh mới làm CPU mất thêm vài chu kỳ nên CPI > 1; nếu code kernel được biên dịch với mức tối ưu thấp thì còn chậm hơn nữa (chưa kiểm tra cờ biên dịch). Những nguyên nhân này là suy luận vật lý, chưa tách riêng bằng đo từng đoạn.
3. **Tỉ lệ 1.19 thấp hơn 1.3 đến 1.5.** Chênh lệch tuyệt đối đo được là 167 cycle, nằm đúng trong khoảng dự đoán (600 đến 700 trừ 450 = 150 đến 250 cycle), nghĩa là phần "event group nhiều bước hơn" (duyệt danh sách chờ, kiểm tra điều kiện, qua danh sách pending-ready) ước lượng đúng. Sai ở phần chung: phần này giống nhau ở cả hai đường và lớn hơn tưởng tượng, nên cộng thêm vào cả hai số làm tỉ lệ bị kéo về gần 1. Dấu hiệu ủng hộ cách giải thích này: độ lệch tuyệt đối so với dự đoán của hai đường gần bằng nhau (437 cycle ở notification: 887 − 450; từ 354 đến 454 cycle ở event group: 1054 − 600 đến 700), đúng như mong đợi nếu phần bị đánh giá thấp là một hằng số chung cho cả hai. Kết luận định tính vẫn đúng: notification nhanh hơn, nhưng chỉ khoảng 16%, không phải gấp 1.3 đến 1.5 lần.
4. **Điều chưa dự đoán:** (a) gần như không có jitter (min = avg = max), đã giải thích ở mục 2; (b) cùng một logic đo nhưng độ trễ event group dao động 1051 đến 1076 cycle giữa các bản build, có thể do thay đổi vị trí code trong Flash.

## 6. Câu hỏi đánh đổi
Với `EVENT_BIT_COUNT` của **bạn**, nếu tăng thêm 1 bit (1 producer nữa) thì chu
kỳ đồng bộ trung bình đo được thay đổi thế nào? Trả lời bằng số từ dữ liệu của
bạn, rồi đo lại để kiểm chứng.

Dự đoán trước khi đo (N = 3 → 4): chu kỳ đồng bộ trung bình đo được ở N=3 là 7464.55 µs (Build A, 8000 lần đồng bộ). Mỗi producer ngủ ngẫu nhiên 5 đến 9 ms, và sync_task chỉ đồng bộ khi producer chậm nhất đã báo, nên chu kỳ gần bằng giá trị lớn nhất của N độ trễ ngẫu nhiên. Giá trị lớn nhất này tăng chậm vì bị chặn ở 9 ms. Mô phỏng Monte Carlo cho N=3 là 7.36 ms và N=4 là 7.47 ms, tức tăng khoảng 1.5%. Áp tỉ lệ đó vào số đo thực: 7464.55 × 1.015 ≈ 7.58 ms (tăng khoảng 0.11 ms). Dự đoán sai số trong khoảng ±0.1 ms.

**Kết quả đo lại (buildD.txt, `EVENT_BIT_COUNT` = 4, cùng cấu hình build A):**

| | N = 3 (buildA) | N = 4 (buildD) |
|---|---|---|
| Chu kỳ đồng bộ trung bình | 7464.55 µs (8000 lần, 59.7 s) | 7556.77 µs (7900 lần, 59.7 s) |
| `sizeof` event group / heap delta | 24 B / 32 B | 24 B / 32 B |
| Free heap sau khi tạo mọi task | 7624 B | 7016 B |
| Độ trễ đánh thức event group | 1051 cycle | 1076 cycle |
| TIMEOUT | 0 | 0 |

- **Chu kỳ tăng +92 µs (+1.2%)**, từ 7464.55 lên 7556.77 µs. So với dự đoán 7.58 ms, số đo thấp hơn 0.02 ms (−0.26%), nằm trong sai số ±0.1 ms nên dự đoán đạt. Mức tăng thực tế (+1.2%) hơi thấp hơn mức mô hình đưa ra (+1.5%), cùng chiều và cùng bậc độ lớn.
- **Lưu ý về độ tin cậy:** hai lần chạy N = 3 (buildA 7464.55 µs và buildB 7418.44 µs) đã chênh nhau 46 µs, nên mức tăng 92 µs chỉ gấp khoảng 2 lần dao động giữa các lần chạy. Kết luận "thêm một bit chỉ làm chu kỳ tăng nhẹ, khoảng 1%" đáng tin về chiều, còn con số chính xác 1.2% thì kém chắc.
- **Vì sao tăng ít:** chu kỳ xấp xỉ giá trị lớn nhất của N số ngẫu nhiên trong khoảng 5 đến 9 ms. Giá trị lớn nhất bị chặn ở 9 ms nên mỗi producer thêm vào đẩy kỳ vọng lên ngày càng ít: mô phỏng cho N = 2, 3, 4, 5 lần lượt 7.22, 7.36, 7.47, 7.55 ms (mức tăng 0.14, 0.11, 0.08 ms). Chu kỳ đồng bộ trung bình không tăng tuyến tính theo số bit.
- **Cái giá thật của một bit nữa nằm ở RAM chứ không ở thời gian:** event group không đổi (24 B / 32 B, mọi bit chung một từ), nhưng free heap giảm 608 B (7624 → 7016) vì thêm một task producer: 512 B stack (128 từ × 4 B) cộng 96 B cho TCB và header heap. Độ trễ đánh thức không thấy phụ thuộc N: danh sách chờ của event group chỉ có một task (`sync_task`) dù có 3 hay 4 producer. Chênh 1051 so với 1076 cycle nằm trong mức dao động giữa các bản build đã nói ở mục 2 (buildC với N = 3 cũng cho khoảng 1073 cycle).