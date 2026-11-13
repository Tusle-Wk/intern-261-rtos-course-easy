# Report — Week 3 / Ex4

`student_id`: 2413858    Commit của PREDICTION.md: ______

## 1. Bảng CPU %
| Task | CPU % (lần đo 1) | CPU % (lần đo 2) |
|---|---|---|
| IDLE | 97,70 | 97,53 |
| stats | 1,68 | 1,86 |
| task_beta | 0,35 | 0,35 |
| task_alpha | 0,23 | 0,23 |
| supervisor | 0,04 | 0,04 |
| Tổng | 100,00 | ≈100 |

Nguồn: `serialdebug_4.txt`. Lần đo 1 ở t ≈ 2 s (dòng 14–18, tổng 146 376 904 chu kỳ), lần đo 2 ở t ≈ 24 s (dòng 146–150, tổng 1 759 536 906 chu kỳ). % tính từ số đếm thô chia cho tổng 5 task. Bảng in ra ở t ≈ 0 không dùng vì tổng mới vài nghìn chu kỳ.

Giải thích Idle task chiếm bao nhiêu % và vì sao.

IDLE chiếm ≈ 97,5% (97,70% rồi 97,53%) vì hệ thống gần như không có việc: alpha/beta mỗi vòng chỉ giữ mutex vài chục µs rồi ngủ `vTaskDelay` 5 + 20 ms, supervisor phần lớn thời gian bị block chờ event group, stats chỉ in một bảng mỗi 2 s. Lúc CPU rảnh thì chạy IDLE. stats chiếm 1,7–1,9% chủ yếu do `printf` qua `HAL_UART_Transmit` chặn (≈ 380 byte × 86,8 µs ≈ 33 ms mỗi 2000 ms ≈ 1,65%). Bảng do `vTaskGetRunTimeStats` in ra chỉ hiện phần nguyên (IDLE 97%, stats 1%, còn lại `<1%`, cộng chỉ được 98%) vì cắt phần thập phân; tính lại từ số đếm thô thì tổng ≈ 100%. DWT CYCCNT 32-bit tràn sau 59,65 s nên bảng chỉ đúng trong 59 s đầu; cả hai lần đo nằm trong khoảng này.

## 2. Bằng chứng tìm deadlock
Log/ảnh `uxTaskGetSystemState` **trước khi sửa**: trạng thái từng task, chỉ
rõ `task_alpha`/`task_beta` đang `eBlocked` và đang chờ mutex nào. Giải thích
chính xác thứ tự lấy mutex nào gây ra deadlock (AB-BA).

Bản chạy: `deadlock_demo.c` giữ nguyên thứ tự gốc (alpha lấy A rồi B, beta lấy B rồi A), IWDG tắt. Log: `serialdebug_1.txt`. Kết quả `uxTaskGetSystemState` ở t ≈ 2 s và t ≈ 30 s:

```
t ≈ 2 s   (dòng 20–24)             t ≈ 30 s  (dòng 188–192)
stats      eRunning runtime=2439765        stats      eRunning runtime=39545082
IDLE       eReady   runtime=143897545      IDLE       eReady   runtime=2158285098
supervisor eBlocked runtime=32298          supervisor eBlocked runtime=467428
task_alpha eBlocked runtime=3617           task_alpha eBlocked runtime=3617
task_beta  eBlocked runtime=3686           task_beta  eBlocked runtime=3686
```

`task_alpha` và `task_beta` cùng `eBlocked` và `ulRunTimeCounter` đứng yên ở 3617 và 3686 trong cả 30 s (đã đứng từ bảng đầu tiên lúc t ≈ 0, dòng 11–12), trong khi stats, IDLE, supervisor vẫn tăng. Hai task không còn được chạy nhưng hệ thống không chết. supervisor `eBlocked` là bình thường (đang chờ event group).

`uxTaskGetSystemState` chỉ cho trạng thái và runtime, không cho biết mutex. Nên khi đang treo tôi dùng gdb đọc chủ sở hữu từng mutex (`debug.png`):

```
p ((struct tskTaskControlBlock *)((struct QueueDefinition *)'deadlock_demo.c'::s_mutex_a)->u.xSemaphore.xMutexHolder)->pcTaskName
$1 = "task_alpha"
p ((struct tskTaskControlBlock *)((struct QueueDefinition *)'deadlock_demo.c'::s_mutex_b)->u.xSemaphore.xMutexHolder)->pcTaskName
$2 = "task_beta"
```

`s_mutex_a` do `task_alpha` giữ, `s_mutex_b` do `task_beta` giữ. Cả hai đều `eBlocked` vĩnh viễn nên `task_alpha` đang chờ `s_mutex_b` (beta giữ) và `task_beta` đang chờ `s_mutex_a` (alpha giữ).

Nguyên nhân (AB-BA): alpha: Take A → `vTaskDelay(5)` → Take B. beta (gốc): Take B → `vTaskDelay(5)` → Take A. Trong 5 ms ngủ, alpha đang giữ A và beta đang giữ B; khi tỉnh dậy alpha xin B, beta xin A, cả hai dùng `portMAX_DELAY` nên chờ mãi, tạo vòng chờ A → B → A. `vTaskDelay(5)` làm cửa sổ "đua" đủ rộng nên deadlock xảy ra ngay chu kỳ đầu.

## 3. Sửa lỗi
Mô tả thay đổi bạn làm để hết deadlock (thứ tự lấy mutex thống nhất). Xác
nhận chạy ổn định sau khi sửa (thời gian chạy thử).

Đổi `task_beta` lấy mutex cùng thứ tự với alpha: Take A → `vTaskDelay(5)` → Take B → Give B → Give A (trước đây: Take B → Take A). Thứ tự thống nhất A rồi B nên không còn vòng chờ: task nào lấy được A trước thì chắc chắn lấy được B, task kia chờ ở A khi chưa giữ B.

Xác nhận (`serialdebug_4.txt`): runtime alpha/beta tăng đều (alpha 338961 → 4044382, beta 508219 → 6094638 từ t ≈ 2 s đến t ≈ 24 s), trong khi trước khi sửa đứng yên ở 3617/3686. Thời gian chạy thử: ≈ 24 s (`serialdebug_4.txt`) và ≈ 89 s (`serialdebug_2.txt`), cả hai chỉ có một dòng `boot`, không treo, không reset.

## 4. Watchdog
Phép tính reload IWDG. Log/video board tự reset khi deadlock được tái tạo lại
có chủ đích, kèm thời gian từ lúc treo đến lúc reset — so với
`IWDG_TIMEOUT_MS`.

Reload: IWDG chạy bằng LSI ≈ 40 kHz, prescaler /4 nên bộ đếm chạy 40 000 / 4 = 10 kHz. reload = `IWDG_TIMEOUT_MS` × 10 000 / 1000 = 250 × 10 = **2500** (nhỏ hơn giới hạn 12-bit là 4095). Kiểm tra: 2500 / 10 kHz = 250 ms.

Demo: đặt lại thứ tự gốc cho beta (Take B → Take A), giữ nguyên supervisor và IWDG. Kết quả:
- `serialdebug_3.txt`: UART lặp dòng `boot` 132 lần liên tiếp, mỗi lần kèm bảng stats có alpha/beta `eBlocked`, runtime đứng yên 3635/3704. Board treo rồi bị IWDG reset, lặp lại mãi.
- Logic analyzer (PulseView) trên chân TX: 18 lần `boot` trong 5 s (`pulseview0.png`). Khoảng giữa hai lần `boot` liên tiếp đo được **268,619 ms** (≈ 3,72 Hz, con trỏ +246 ms đến +515 ms, `pulseview.png`).

Thời gian từ lúc treo đến lúc reset: deadlock xảy ra ≈ 5 ms sau khi IWDG bật và không có lần refresh nào, nên IWDG đếm cạn từ lúc bật. Chu kỳ 268,6 ms gồm thời gian IWDG đếm cạn cộng thời gian khởi động lại đến dòng `boot` (không đo riêng được). Vậy từ lúc treo đến lúc reset ≤ 268,6 − 5 ≈ 263 ms, so với `IWDG_TIMEOUT_MS` = 250 ms thì chênh ≤ ≈ 14 ms (chu kỳ lệch +18,6 ms). Chênh lệch do thời gian boot và sai số LSI (LSI thực nằm trong 30–60 kHz, không đúng 40 kHz), và nằm trong dải 167–333 ms đã dự đoán.

## 5. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân.

- Mục 1 (treo sau bao lâu): không lệch. Runtime alpha/beta đã đứng yên ngay ở bảng đầu tiên (t ≈ 0); con số 5 ms chưa đo trực tiếp.
- Mục 2: không lệch. alpha/beta `eBlocked`, runtime đứng yên; alpha giữ `s_mutex_a`, beta giữ `s_mutex_b` (xác nhận bằng gdb). Lưu ý `uxTaskGetSystemState` không cho tên mutex, tên mutex lấy từ gdb.
- Mục 3: không lệch. reload = 2500.
- Mục 4: lệch nhỏ. Dự đoán reset ≈ 250 ms (cộng thời gian boot), đo được chu kỳ reset 268,6 ms (+18,6 ms). Nguyên nhân là thời gian boot cộng sai số LSI. Hành vi vòng reset lặp lại đúng như dự đoán.

## 6. Câu hỏi đánh đổi
Với `SUPERVISOR_BITS` và chu kỳ check-in của bạn, nếu một task thật sự chạy
chậm (không deadlock, chỉ chậm) trong đúng 1 chu kỳ, supervisor có false-trip
(reset oan) không? Trả lời bằng số đo thời gian thực tế của chu kỳ check-in.

Cấu hình: `SUPERVISOR_BITS` = 3 (alpha, beta, stats), `SUPERVISOR_WAIT_MS` = 100 ms, `IWDG_TIMEOUT_MS` = 250 ms.

Số đo: bản build tạm đo bằng DWT CYCCNT khoảng giữa hai lần `HAL_IWDG_Refresh` (chu kỳ check-in của cả nhóm 3 bit), in giá trị lớn nhất mỗi 40 lần refresh, chạy ≈ 32 s (≈ 640 lần refresh), log `test_caucuoi.txt`, đoạn đo đã gỡ khỏi bản nộp:
- Bình thường: 50,0 ms (cửa sổ đầu `max = 50000 us`), do `stats` check-in mỗi 50 ms là task chậm nhất.
- Lớn nhất: **86,977 ms**. Mỗi 2 s `stats` in bảng ≈ 33 ms bằng UART chặn nên lỡ check-in, refresh bị trễ theo.

Trả lời: **không false-trip** với mức chậm này. Trường hợp "một task chậm đúng 1 chu kỳ" đã xảy ra thật: `stats` chậm thêm ≈ 37 ms mỗi 2 s, 16 lần trong log, và không lần nào reset vì 87 ms < 250 ms. 87 ms cũng nhỏ hơn `SUPERVISOR_WAIT_MS` = 100 ms nên supervisor không timeout. Kể cả khi timeout thì chỉ không refresh lần đó và bit không bị xoá (`xEventGroupWaitBits` chỉ xoá bit khi đủ bit), task chậm check-in xong thì refresh ngay.

Ngưỡng: reset oan chỉ xảy ra khi khoảng giữa hai lần refresh vượt 250 ms. Một task chậm thêm Δ trong 1 chu kỳ thì khoảng đó ≈ gap + Δ, nên cần Δ ≳ 250 − 87 ≈ 163 ms (tính từ gap tệ nhất đo được; tính từ gap thường 50 ms thì ≈ 200 ms). Timeout 250 ms gấp ≈ 2,9 lần gap tệ nhất nên đủ dư. Đặt nhỏ hơn ≈ 87 ms thì reset oan mỗi lần `stats` in bảng, đặt quá lớn thì phát hiện treo chậm (hiện treo thật bị reset sau ≈ 250 ms, đo được ở mục 4). Số đo chỉ trong ≈ 32 s, chạy lâu hơn gap tệ nhất có thể nhích lên chút nhưng còn xa 250 ms.