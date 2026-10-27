# InfuseSubFinder

Dylib tiêm vào Infuse (iOS). Khi mở một video, nó tự tìm file sub có tên khớp ở **mọi nguồn Infuse đã thêm** (thư mục local, WebDAV, SMB...), tải về rồi nạp vào danh sách sub của player, giống như sub nhúng trong mkv. Không cần chia sẻ sub thủ công.

> Chưa test trên máy thật (chỉ test phần khớp tên). Lần chạy đầu hãy xem log (bên dưới) và gửi lại nếu có gì sai.

## Build

1. Upload toàn bộ thư mục này lên repo GitHub (giữ nguyên `.github/workflows`).
2. Tab **Actions** → **Build InfuseSubFinder** → **Run workflow**.
3. Chạy xong, tải artifact `InfuseSubFinder` → file `InfuseSubFinder.dylib`.
4. Tiêm `InfuseSubFinder.dylib` vào IPA Infuse (đang tiêm sẵn InfusePlus thì tiêm thêm cái này). Dylib chỉ dùng framework hệ thống, không cần Substrate.

## Thông báo trên màn hình (v0.3)

Dylib hiện một dòng thông báo nhỏ ở đầu màn hình (6 giây) ở các bước: đã nạp/hook OK, quét xong bao nhiêu sub, và mỗi lần phát video thì khớp/nạp được bao nhiêu sub (hoặc lý do không được). Nếu quét xong mà không thấy sub nào, dylib hiện một bảng chẩn đoán (danh sách nguồn, số thư mục/file/sub, các đuôi file Infuse trả về) — chụp màn hình bảng này gửi người phát triển. `"diagnostic": true` để luôn hiện bảng sau mỗi lần quét. Tắt thông báo bằng `"showToast": false`.

## Cách hoạt động (v0.8)

- Dylib hook `-[FCMediaFile relatedItems]`: danh sách file "cùng thư mục với video" mà Infuse dùng để tìm sub ngoài. Dylib thêm vào đó các sub khớp tên ở thư mục/nguồn khác, nên Infuse xử lý y như sub nằm chung thư mục (hiện trong menu sub, không lưu cache, không bị trùng khi mở lại).
- Chỉ mục sub (`index.json`) lưu trên đĩa, nạp ngay khi mở app. Chỉ quét nền khi chưa có chỉ mục hoặc đã cũ hơn `rescanHours` (12 giờ), mỗi lần mở app tối đa một lần. Quét: iPhone/bookmark trước, mạng sau; bỏ thư mục nội bộ `vfLocalFiles`.
- Lần đầu cài chưa có chỉ mục nên cần đợi quét xong (vài phút) rồi mở video.

## Bảng cài đặt trong app

Trong Cài đặt của Infuse có dòng **Tìm sub (InfuseSubFinder)** ngay dưới "Infuse Plus", bấm vào sẽ đẩy sang màn hình riêng:

- Trạng thái + thanh tiến trình tổng, nút **Làm mới tất cả** (đang quét thì thành **Hủy tất cả**), chế độ **tự động làm mới** (Thủ công / Mỗi lần mở / Mỗi giờ / 12 giờ).
- Danh sách **nguồn** (iPhone, WebDAV, FTP, Drive...). Mỗi nguồn và mỗi thư mục con đều có **công tắc bên trái** (bật/tắt tìm sub ở đó) và **nút làm mới bên phải** (đang quét thì thành nút hủy, có tiến trình riêng). Các mục có thể quét cùng lúc, không giới hạn một.
- Bấm vào một mục để đi vào bên trong: thư mục con và danh sách file sub. Nguồn iPhone tự cập nhật nền mỗi vài giây (kể cả khi không đứng trong thư mục đó); thư mục mới xuất hiện sẽ tự được quét.

Trang thư mục mở ngay từ chỉ mục (không chờ server) rồi tự đọc lại thư mục mỗi 3 giây (iPhone) hoặc 10 giây (nguồn mạng). iPhone: file thêm/xóa tự cập nhật vào chỉ mục; nguồn mạng: hiện nút "Thêm N sub mới" / "Xóa N sub đã mất".

Thiết lập lưu ở `settings.json`. Sub quét được trước vào chỉ mục trước (cập nhật mỗi 25 thư mục). Chỉ mục luôn loại trùng theo đường dẫn file.

## Cấu hình và log

Trong thư mục Documents của Infuse: `InfuseSubFinder/config.json` (tạo tự động lần đầu) và `InfuseSubFinder/log.txt`.

| Khóa | Ý nghĩa |
|---|---|
| `enabled` | bật/tắt |
| `minScore` | điểm tối thiểu (mặc định 60) |
| `maxPerVideo` | số sub tối đa nạp cho mỗi video (4) |
| `languages` | lọc ngôn ngữ, vd `["Vietnamese","English"]`; rỗng = tất cả |
| `excludeSources` | tên nguồn bỏ qua (khớp một phần) |
| `includeCloudSources` | quét cả Drive/OneDrive... (mặc định tắt vì chậm) |
| `maxDepth`, `maxFoldersPerSource` | giới hạn độ sâu / số thư mục khi quét |
| `skipSameFolder` | bỏ qua sub cùng thư mục với video (Infuse đã tự thấy) |
| `aliases` | tên khác nhau của cùng một phim, vd `[["danmachi","dungeon ni deai wo motomeru no wa machigatteiru darou ka"]]` |

Sửa xong khởi động lại Infuse (hoặc đợi lần quét kế).

## Giới hạn đã biết

- Đổi API nội bộ của Infuse (hiện hỗ trợ theo bản 8.2.4) có thể làm hook không chạy; dylib sẽ ghi log chứ không crash.
- Sub đặt tên không có số tập và tên phim khác hẳn video thì không khớp được; dùng `aliases` hoặc đặt sub trong thư mục có tên phim.
- Chưa tự bấm chọn sub; chỉ đưa vào danh sách.
- Tên có chữ hoa non-ASCII (vd "Đ") được so khớp phân biệt hoa thường.
- Lần mở video đầu tiên có thể chậm vài giây (tải sub qua mạng, tối đa `downloadTimeoutSeconds`).

## Test

```
gcc -Wall -Wextra -Isrc src/SFMatch.c tests/test_match.c -o /tmp/t && /tmp/t
```

## Chương MKV → nút skip của InfusePlus (v0.15)

Dylib đọc **chương (chapter) trong file video** và đổi thành đoạn Intro / Recap / Preview / Outro cho InfusePlus, nên nút Skip, thanh tiến trình tô màu, tự động skip / hỏi trước... chạy y như khi có dữ liệu từ TheIntroDB.

- Trong Cài đặt → Infuse Plus → **Intros**, dưới mỗi loại có thêm dòng **Chapters**: nhập tên các chương cách nhau bằng dấu phẩy (ví dụ `Opening, OP`). Để trống = tắt loại đó. Mặc định: Intro `Opening, OP, Intro` · Recap `Recap, Previously` · Preview `Preview, Next` · Outro `Ending, ED, Credits, Outro`.
- Tên khớp không phân biệt hoa thường; `OP` khớp cả `OP 2`, `OP1`, `Opening Theme`.
- Dữ liệu từ chương **đè** dữ liệu online ở loại tương ứng, loại nào chương không có thì vẫn dùng dữ liệu online.
- Log (`log.txt`) ghi dòng `[chap]` liệt kê chương đọc được và loại nào khớp.
- Dòng **Priority** ở đầu mục Segment Settings: `Local` (ưu tiên chương trong video) hoặc `Network` (ưu tiên dữ liệu TheIntroDB/IntroDB). Loại nào bên ưu tiên không có (không có chương khớp tên, hoặc mạng không có dữ liệu) thì dùng bên kia.
- Video **chưa khớp metadata** vẫn chạy: chương được dùng bằng một "asset giả" nên nút Skip và thanh tiến trình vẫn hiện.
- Sửa lỗi của InfusePlus: menu lưu Ask = 3, Auto-skip = 2 nhưng bộ xử lý hiểu ngược → chọn Ask lại tự skip. Dylib đã đảo lại cho đúng.
- Bật "Skip Intros" của InfusePlus để dùng.

- **Skip Alert: hold until segment ends** (công tắc dưới "Skip Alert Duration"): với chế độ Ask, nút Skip nằm trên màn đến hết đoạn thay vì tự tắt sau N giây.
- **Chapter Buttons** (dưới "Display Overlay Button"): Off / Previous / Next / Previous + Next. Hiện nút ⏮ ⏭ bên trái nút (i) trong player, nhảy chương theo chương trong file mkv, không cần mạng hay metadata. Video không có chương thì nút tự ẩn.
- **Show Skip again when seeking back**: bật thì tua lại vào đoạn intro/ED đã đi qua nút Skip hiện lại đến hết đoạn; tắt thì như InfusePlus mặc định.
- Nút (i) tắt → nút Skip đang giữ biến mất; bật lại → hiện lại nếu đang ở trong đoạn. Đổi tập/hết video → nút Skip cũ tự bỏ.
- "Tự tắt sau" của thông báo gắn sub là bánh xe cuộn 1–10 giây.
- Thanh tiến trình được yêu cầu vẽ lại ngay khi đoạn từ chương được gắn.
- Thông báo "ISF: gắn N sub…" gắn vào màn hình trên cùng với auto layout nên luôn nằm giữa, kể cả khi player xoay ngang.
- Trang **Tìm sub** có mục **THÔNG BÁO**: công tắc "Báo khi gắn sub vào video" và chọn "Tự tắt sau" 1–10 giây (nút bấm ra danh sách như các nút "10 giây" của InfusePlus) (lưu trong `settings.json`, không cần sửa config).
- **Probe tạm (v0.23)**: ghi `probe.txt` (cùng thư mục log.txt) khi mở player, để nghiên cứu màn hình load.

- **Màn hình tải chi tiết** (Tìm sub → "THÔNG BÁO & TẢI VIDEO"): thay vòng xoay của player bằng bảng trạng thái: giai đoạn (kết nối / đọc đầu file / phân tích / đệm), loại nguồn, số MB đã đọc, tốc độ MB/s, thời gian chờ và gợi ý (mạng chậm, nguồn không trả dữ liệu, máy giải mã nặng). Tắt công tắc để dùng vòng xoay gốc.
- **Sửa**: màn hình tải chi tiết chỉ hiện lúc đang tải/đệm, tự ẩn khi video đang chạy (nhận biết bằng thời gian phát đang tiến).
- **Nút bảng theo dõi** (Tìm sub → "Nút bảng theo dõi trong player"): thêm nút biểu đồ bên trái nút (i). Bấm để hiện/ẩn bảng nổi: trạng thái (tải/phát/đệm/dừng), vị trí, nguồn, MB đã đọc, tốc độ, số lần đứng hình, nhiệt độ máy. Kéo để di chuyển, chụm hai ngón hoặc nút −/+ để đổi cỡ, ✕ để đóng. Độc lập với màn hình tải.
- **Thanh cache (vạch trắng mờ)**: tự khởi động lại bộ cập nhật khi Infuse làm nó ngừng; vẽ lại mỗi giây.
- **Chặn 2x do giữ màn hình** của Infuse gốc khi tuỳ chọn "Tăng tốc phát lại" của InfusePlus đang tắt.
- **Ẩn 2 nút tua**: ép ẩn liên tục (Infuse hay bật lại).
- **Log đứng hình**: `[stall]` kèm tốc độ, nhiệt, fps, giải mã phần mềm. Bảng theo dõi thêm dòng Giải mã/FPS/Đệm đầy.
- Tìm sub → "THÔNG BÁO & TẢI VIDEO": công tắc **Ghi log** và **Probe dò lỗi** (probe mặc định tắt).
- Bảng theo dõi: reset bộ đếm khi đổi tập trong player; thêm Video (độ phân giải/fps), Giải mã (phần cứng/mềm, đọc từ FCVideoStreamDescriptor), Trình phát (tên lớp engine).
- **FPS sống** trong bảng theo dõi (đếm khung hình thật qua AVSampleBufferDisplayLayer/CAMetalLayer, so với fps gốc) và **báo cáo cuối phiên** (log `[report]` + toast khi bật nút bảng theo dõi; dòng "Phiên trước" trong bảng). Đệm đầy/chuyển giải mã phần mềm đếm trực tiếp qua hook tracker.
- **Disable stock hold-to-2x** (InfusePlus → Trình phát, dưới "Tăng tốc phát lại"): chặn giữ màn hình tăng tốc 2x mặc định của Infuse. Báo cáo cuối phiên: "kịp giờ %" thay cho "fps", tên engine, tốc độ nguồn do mình đo.
- Sửa lỗi nút Skip hiện lại nhiều lần sau khi bấm (player báo thời gian đích rồi lùi về keyframe): chỉ xoá đánh dấu khi ra ngoài đoạn ổn định ≥1.5s, và nhảy tiếp qua hết đoạn nếu bị lùi lại.

- v0.32: tat toast Skip cu truoc khi hien toast moi (het 2 nut Skip chong nhau); bo viec day toast len.
- v0.33: khong goi askBeforeSkipping lai khi toast cua doan do dang hien (het 2 toast Skip chong nhau).
- v0.33: toast Skip: chi giu 1 cai (cai cu bi tat), (i) tat thi tat het toast, doi tap thi reset trang thai skip; log dong [toast].
- v0.34: khong tat toast Skip vua tao cho tap moi (loi mat nut skip luc dau tap); them log [toast].
- v0.35: doi tap thi xoa _skippedSegments cua tap truoc (sua mat nut Skip Prologue/Recap dau tap).
- v0.36: don code thua (day toast len, quet toast), them log [handle] de tim ly do Prologue dau tap khong hien Skip.
- v0.37: chi nhay tiep doan khi nguoi dung THAT SU bam Skip (hook INFPIntroController seekToTime:), het tu bo qua Opening khi Prologue dinh lien Opening.
