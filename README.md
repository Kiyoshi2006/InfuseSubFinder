# InfuseSubFinder

Dylib tiêm vào Infuse (iOS). Khi mở một video, nó tự tìm file sub có tên khớp ở **mọi nguồn Infuse đã thêm** (thư mục local, WebDAV, SMB...), tải về rồi nạp vào danh sách sub của player, giống như sub nhúng trong mkv. Không cần chia sẻ sub thủ công.

> Chưa test trên máy thật (chỉ test phần khớp tên). Lần chạy đầu hãy xem log (bên dưới) và gửi lại nếu có gì sai.

## Build

1. Upload toàn bộ thư mục này lên repo GitHub (giữ nguyên `.github/workflows`).
2. Tab **Actions** → **Build InfuseSubFinder** → **Run workflow**.
3. Chạy xong, tải artifact `InfuseSubFinder` → file `InfuseSubFinder.dylib`.
4. Tiêm `InfuseSubFinder.dylib` vào IPA Infuse (đang tiêm sẵn InfusePlus thì tiêm thêm cái này). Dylib chỉ dùng framework hệ thống, không cần Substrate.

## Cách hoạt động

- Sau khi mở app ~6 giây, dylib quét nền các nguồn qua chính lớp VFS của Infuse (dùng tài khoản/quyền Infuse đã có, không nhập lại mật khẩu), lập chỉ mục các file `srt/ass/ssa/vtt`. Quét lại mỗi 30 phút.
- Khi player chuẩn bị gom sub (`FCMediaFile allExternalTracksOfType:` loại 4), dylib chấm điểm tên video với các sub trong chỉ mục (bỏ qua nhiễu như 1080p/WEB/x265/nhóm sub, so số tập, mùa, năm), chọn tối đa 4 sub điểm cao, tải bằng hàm tải của Infuse rồi lưu vào cache sub của Infuse (cùng đường đi với "chia sẻ sub").
- Sub tự có trong menu chọn sub ngay lần phát đầu. Từ lần sau không tải lại.

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
