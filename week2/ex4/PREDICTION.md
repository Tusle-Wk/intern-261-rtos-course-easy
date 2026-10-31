# Dự đoán — Week 2 / Ex4

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    `EVENT_BIT_COUNT` = 3  `SYNC_TIMEOUT_MS` = 50

(`NOTIFY_PERIOD_MS` = 5 ms → mỗi producer ngủ ngẫu nhiên 5..9 ms giữa hai lần báo hiệu.)

Giả định cấu hình (đã đối chiếu với `FreeRTOSConfig.h` và CMakeLists.txt trước khi commit):
Cortex-M3 32-bit, `configUSE_16_BIT_TICKS = 0`, heap_4, `configUSE_TRACE_FACILITY = 0`,
`configSUPPORT_STATIC_ALLOCATION = 0`, `configUSE_TASK_NOTIFICATIONS = 1`, FreeRTOS-Kernel V11.1.0.

1. **RAM event group chiếm: dự đoán 24 bytes cho cấu trúc, 32 bytes thực tế lấy từ heap.**
   Dựa vào struct `EventGroup_t` (định nghĩa riêng trong `event_groups.c`):
   - `EventBits_t uxEventBits`: 4 B (32 bit vì tick 32 bit; 24 bit dưới dùng cho người dùng,
     8 bit trên là control bit của kernel).
   - `List_t xTasksWaitingForBits`: danh sách task đang chờ bit = `uxNumberOfItems` (4) +
     `pxIndex` (4) + `xListEnd` kiểu `MiniListItem_t` (`xItemValue` 4 + `pxNext` 4 +
     `pxPrevious` 4) = 20 B.
   - Tổng 4 + 20 = **24 B**. Nếu bật cả `configSUPPORT_STATIC_ALLOCATION` và
     `configSUPPORT_DYNAMIC_ALLOCATION` thì thêm `ucStaticallyAllocated` (1 B, bị padding
     thành 4) → 28 B; nếu bật trace thêm `uxEventGroupNumber` (4 B).
   - `pvPortMalloc` (heap_4) cộng thêm header `BlockLink_t` 8 B rồi làm tròn lên bội số của 8
     → 24 + 8 = **32 B** tụt khỏi free heap.
   - Số lượng bit (3) **không** làm đổi kích thước: mọi bit nằm trong cùng một từ `uxEventBits`.

2. **RAM thêm khi dùng task notification cho 1 điểm đồng bộ: dự đoán = 0 bytes.**
   Giá trị notification (`ulNotifiedValue`, 4 B) và trạng thái (`ucNotifyState`, 1 B) là
   trường nằm sẵn trong TCB, được cấp phát cùng TCB lúc `xTaskCreate`, dù có dùng hay không
   (khi `configUSE_TASK_NOTIFICATIONS = 1`). Nên free heap của hai bản build phải bằng nhau.
   Hệ quả: bản "2 event group + 1 notification" vẫn còn nguyên **một** event group
   (24 B struct / 32 B heap) vì 2 producer còn lại vẫn cần nó → chênh lệch so với bản
   "3 điểm event group" dự đoán là 0 B. Chỗ notification thực sự tiết kiệm là so với
   semaphore nhị phân: một `Queue_t` khoảng 80 B (+ 8 B header → ~88 B heap).

3. **Thời gian đánh thức: notification nhanh hơn event-group-bit, nhưng chênh lệch không
   lớn.** Dự đoán notification khoảng 450 cycle (~6 us ở 72 MHz), event group khoảng
   600-700 cycle (~9-10 us), tức tỉ lệ khoảng 1.3-1.5 lần. Đây là ước lượng độ lớn, độ
   tin cậy thấp, sẽ đối chiếu với số đo. Lý do (số lớp hàm bên dưới mỗi API):
   - `xTaskNotifyGive` → `xTaskGenericNotify`: vào critical section, ghi giá trị + trạng
     thái vào TCB đích, nếu task đang chờ thì gỡ khỏi delayed list, đưa thẳng vào ready
     list, so priority, yield. Một lớp, không có danh sách trung gian. Phía người chờ
     (`ulTaskNotifyTake`) sau khi tỉnh vào lại critical section để đọc và xóa giá trị.
   - `xEventGroupSetBits`: `vTaskSuspendAll`, set bit, duyệt `xTasksWaitingForBits`, với mỗi
     task chờ kiểm tra điều kiện (wait-all/any), nếu thỏa thì gom bit cần xóa
     (clear-on-exit) và gọi `vTaskRemoveFromUnorderedEventList` (task vào danh sách
     pending-ready), rồi `xTaskResumeAll` mới chuyển task sang ready và yield. Nhiều bước
     hơn: duyệt list, kiểm tra điều kiện, qua pending-ready list. Phía người chờ
     (`xEventGroupWaitBits`) khi được đánh thức bằng bit thì trả về luôn giá trị bit mà
     `SetBits` đã chuẩn bị, không phải vào lại critical section.
   - Phần chung (PendSV, chọn task, đổi ngữ cảnh) giống nhau nên chênh lệch chỉ nằm ở
     "đường đi trong kernel", không phải gấp nhiều lần.

4. **Khi timeout, `xEventGroupWaitBits` trả về giá trị của event group tại thời điểm hết
   hạn** (ví dụ `0b101` nếu producer_1 trễ), không phải 0 cố định và không phải mã lỗi.
   Bit **không** bị xóa vì clear-on-exit chỉ áp dụng khi điều kiện chờ được thỏa. Phân biệt:
   `(ret & ALL_BITS_MASK) == ALL_BITS_MASK` → đủ bit; ngược lại là timeout, bit còn thiếu là
   `ALL_BITS_MASK & ~ret`. Không được kiểm tra `ret != 0` vì timeout vẫn có thể trả về giá
   trị khác 0. Bit đến đúng lúc timeout thì giá trị trả về đủ bit và vẫn tính là thành công.

5. **Tình huống không thể thay toàn bộ event group bằng notification:** nhiều task cùng chờ
   một sự kiện (broadcast), ví dụ hai task cùng chờ bit "hệ thống sẵn sàng". Notification
   luôn gửi đến đúng **một** task đích và người gửi phải biết handle của task đó; không có
   cách để một lần báo đánh thức nhiều task. Tình huống thứ hai: rendezvous nhiều-nhiều (mọi
   task vừa báo vừa chờ tất cả các task còn lại). Ngoài ra `ulTaskNotifyTake` ở chế độ đếm
   chỉ cộng dồn, không cho biết **nguồn nào** đã báo: một producer báo 3 lần cũng làm
   count = 3, trong khi event group dùng mỗi bit cho một nguồn riêng nên phân biệt được.
   Lưu ý để kiểm chứng ở REPORT: với đúng một task chờ (như `sync_task` ở bài này), có thể
   dùng `xTaskNotify(..., eSetBits)` để mỗi producer set một bit trong giá trị notification;
   nhưng khi đó phải tự viết vòng kiểm tra "đủ bit chưa", không có sẵn wait-all như
   `xEventGroupWaitBits`.