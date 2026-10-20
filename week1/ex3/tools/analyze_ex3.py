import csv
import sys

def analyze_pulseview_csv(csv_file, target_period_ms=17.0, sample_count=800):
    times = []
    last_state = None
    
    print(f"Đang phân tích file: {csv_file}...")
    try:
        with open(csv_file, 'r') as f: #đọcfile
            reader = csv.reader(f)
            next(reader, None)  #bỏ qua dòng tiêu đề
            
            for row in reader:
                if len(row) < 2: continue
                try:
                    t = float(row[0].strip())#strip để remove space nếu có
                    state = int(row[1].strip())
                except ValueError:
                    continue
                
                #phát hiện cạnh (trạng thái pin thay đổi từ 0 lên 1 hoặc 1 xuống 0)
                if last_state is not None and state != last_state:
                    times.append(t * 1000)  #đổi s sang ms
                last_state = state
                
    except FileNotFoundError:
        print(f"Không tìm thấy file {csv_file}!")
        return

    #số lượng chu kỳ đo được = số cạnh phát hiện được - 1
    cycles_found = len(times) - 1
    if cycles_found < sample_count:
        print(f"CẢNH BÁO: Chỉ tìm thấy {cycles_found} chu kỳ. Yêu cầu ít nhất {sample_count} chu kỳ.")
        sample_count = cycles_found
        if sample_count <= 0: return

    #tính mảng các chu kỳ (khoảng cách delta T giữa các lần toggle liên tiếp)
    periods = [times[i] - times[i-1] for i in range(1, sample_count + 1)]
    
    min_ms = min(periods)
    max_ms = max(periods)
    avg_ms = sum(periods) / sample_count
    jitter_us = (max_ms - min_ms) * 1000  #đổi ra micro-giây (us)
    
    #Drift = tổng thời gian thực tế - tổng thời gian lý thuyết
    total_time_ms = times[sample_count] - times[0]
    theoretical_time_ms = target_period_ms * sample_count
    drift_ms = total_time_ms - theoretical_time_ms

    print("-" * 40)
    print(f"Số mẫu                : {sample_count}")
    print(f"Min                   : {min_ms:.4f} ms")
    print(f"Max                   : {max_ms:.4f} ms")
    print(f"Trung bình            : {avg_ms:.4f} ms")
    print(f"Jitter (Max - Min)    : {jitter_us:.2f} us")
    print(f"Drift tổng            : {drift_ms:.4f} ms")
    print("-" * 40)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Sử dụng: python analyze_ex3.py <file.csv>")
    else:
        #chu kỳ đích 17ms, lấy 800 mẫu
        analyze_pulseview_csv(sys.argv[1], target_period_ms=17.0, sample_count=800)