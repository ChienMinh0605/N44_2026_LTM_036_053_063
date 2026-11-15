#!/usr/bin/env bash
# Tổng hợp thí nghiệm C và F thành bảng Markdown (in ra màn hình).
#   ./scripts/summarize_CF.sh [data] > data/summary_CF.md
# Một tin được định danh bằng (boot_id, topic, seq); tin nhận nhiều lần chỉ tính một lần.
set -euo pipefail
export LC_ALL=C
cd "$(dirname "$0")/.."
ROOT="${1:-data}"

# stats <thư mục> <off_ms> <on_ms>: in "gửi sent buffered dropped nhận_duy_nhất mất trùng
#                      gửi_lúc_sự_cố nhận_lúc_sự_cố trễ_trung_vị_lúc_sự_cố trễ_lớn_nhất_lúc_sự_cố"
# "lúc sự cố" = tin có status buffered (C), hoặc timestamp_ms trong [off_ms, on_ms] khi off_ms > 0 (F).
stats() {
  awk -F, -v off="$2" -v on="$3" '
    FNR == 1 { next }
    FILENAME ~ /sent\.csv$/ {
      k = $1 SUBSEP $2 SUBSEP $3; n++; st[$6]++
      if (off > 0 ? ($5 >= off && $5 <= on) : $6 == "buffered") { gap++; ingap[k] = 1 }
      sent[k] = 1
      next
    }
    {  # logger.csv: recv_time_ms,topic,boot_id,seq,timestamp_ms,...
      k = $3 SUBSEP $2 SUBSEP $4
      if (!(k in sent)) next
      if (k in got) { dup++; next }
      got[k] = 1; uniq++
      if (k in ingap) { gapgot++; lat[gapgot] = $1 - $5 }
    }
    END {
      med = "-"; mx = "-"
      if (gapgot > 0) {
        for (i = 2; i <= gapgot; i++) {  # sắp xếp chèn: mawk không có asort, n chỉ vài trăm
          v = lat[i]
          for (j = i - 1; j >= 1 && lat[j] > v; j--) lat[j + 1] = lat[j]
          lat[j + 1] = v
        }
        med = lat[int(gapgot / 2) + 1]; mx = lat[gapgot]  # trung vị: phần tử thứ n/2 như thí nghiệm H
      }
      printf "%d %d %d %d %d %d %d %d %d %s %s\n", n, st["sent"], st["buffered"], st["dropped"],
             uniq, n - uniq, dup, gap, gapgot, med, mx
    }' "$1/sent.csv" "$1/logger.csv"
}

# logger_gap <logger_events.csv>: "lúc Logger ngắt  lúc Logger kết nối lại" (lần ngắt đầu tiên)
logger_gap() {
  awk -F, 'NR > 1 && $3 == "disconnected" && !off { off = $1; next }
           off && $3 == "connected" { print off, $1; exit }' "$1"
}
off_sec() { sed -n 's/^off_sec=//p' "$(ls "$ROOT"/"$1"/*/scenario.txt | head -1)"; }

# reconnect_s <events.csv> <off_ms>: số giây từ lúc gây sự cố tới lần "connected" đầu tiên sau đó
reconnect_s() {
  awk -F, -v off="$2" 'NR > 1 && $3 == "connected" && $1 > off { printf "%.1f", ($1 - off) / 1000; exit }' "$1"
}

echo "## C – Broker tắt $(off_sec C) s giữa lượt (QoS 1): bộ đệm Gateway tắt và bật"
echo
echo "| Thư mục | Gửi | sent / buffered / dropped | Nhận (duy nhất) | Mất | Trùng | Gateway kết nối lại sau (s) | Tin xả bù: trễ trung vị / lớn nhất (ms) |"
echo "|---|---|---|---|---|---|---|---|"
for d in "$ROOT"/C/buffer-*_run*; do
  [[ -s $d/logger.csv ]] || continue
  read -r n s b dr u lost dup _ _ med mx < <(stats "$d" 0 0)
  rc=$(reconnect_s "$d/events.csv" "$(cat "$d/off_ms.txt")")
  echo "| $(basename "$d") | $n | $s / $b / $dr | $u | $lost | $dup | ${rc:--} | $med / $mx |"
done

echo
echo "## F – Logger dừng $(off_sec F) s rồi chạy lại cùng client_id: QoS 0 và QoS 1"
echo
echo "| Thư mục | Gửi | Nhận (duy nhất) | Mất | Trùng | Gửi lúc Logger tắt | Nhận bù | Trễ tin nhận bù: trung vị / lớn nhất (ms) |"
echo "|---|---|---|---|---|---|---|---|"
for d in "$ROOT"/F/qos*_run*; do
  [[ -s $d/logger.csv ]] || continue
  read -r n _ _ _ u lost dup gap gapgot med mx < <(stats "$d" $(logger_gap "$d/logger_events.csv"))
  echo "| $(basename "$d") | $n | $u | $lost | $dup | $gap | $gapgot | $med / $mx |"
done
