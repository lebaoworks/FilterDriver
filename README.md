# Rules DSL — Định nghĩa yêu cầu chức năng

Rút ra từ [sample.rules](sample.rules). Tài liệu này định nghĩa các thành phần cú pháp và hành vi mà một engine xử lý file `.rules` phải hỗ trợ.

| ID | Thành phần | Cú pháp | Yêu cầu chức năng | Ví dụ |
|----|------------|---------|--------------------|-------|
| R1 | Khối pattern | `pattern <name> [thuộc_tính...]` ... `end` | Mỗi file có thể chứa nhiều khối `pattern`, mỗi khối có tên định danh duy nhất và phải được đóng bằng từ khóa `end` | `pattern drop_and_run` ... `end` |
| R2 | Thuộc tính scope | `scope=<value>` (đặt trên dòng khai báo `pattern`) | Xác định phạm vi tương quan giữa các step trong pattern (ví dụ: chỉ đối chiếu các step phát sinh từ cùng actor liên quan) | `scope=related_actors` |
| R3 | Khai báo step | `step <label> op=<operation> [điều_kiện...]` | Mỗi step trong pattern có một nhãn (label) duy nhất trong phạm vi pattern và một `op` chỉ định loại sự kiện/hành động cần khớp | `step drop op=file_create ...` |
| R4 | Trình tự step | Nhiều `step` liệt kê tuần tự trong 1 pattern | Các step được đánh giá/khớp theo đúng thứ tự khai báo, thể hiện một chuỗi hành vi (sequence) cần phát hiện | `step drop ...` rồi `step exec ...` |
| R5 | Điều kiện trường dữ liệu | `<field>=<value>` hoặc `<field.attr>=<value>` | Mỗi điều kiện so khớp giá trị của một attr thuộc trường sự kiện; truy cập attr bằng dấu chấm (`path` chỉ là một attr trong số nhiều attr có thể có, ví dụ `image.path`, `image.hash`...) | `image.path="lsass.exe"` |
| R6 | Toán tử so khớp chính xác | `=` | So khớp giá trị trường bằng chuỗi chính xác | `image.path="lsass.exe"` |
| R7 | Toán tử so khớp wildcard | `~~` | So khớp giá trị trường theo mẫu glob/wildcard (hỗ trợ `*`) | `image.path~~"*.exe"` |
| R8 | Toán tử so khớp chứa | `~` | So khớp giá trị trường khi chuỗi con nằm trong (substring/contains), không cần glob | `image.path~"System32"` |
| R9 | Toán tử so khớp regex | `=~` | So khớp giá trị trường theo biểu thức chính quy (regex) | `image.path=~"^C:\\\\Windows\\\\.*\\.exe$"` |
| R10 | Tham chiếu giá trị step trước | `$<step_label>.<field>` | Điều kiện của một step có thể tham chiếu tới giá trị trường của step khác đã khai báo trước đó trong cùng pattern, dùng để liên kết dữ liệu giữa các bước | `image=$drop.image` |
| R11 | Không điều kiện trường bắt buộc | `step <label> op=<operation>` (không kèm điều kiện) | Một step hợp lệ khi chỉ cần khớp đúng loại `op`, không bắt buộc phải có điều kiện trường | `step dump op=file_create` |
| R12 | Chuỗi có dấu ngoặc kép | `"<value>"` | Giá trị so khớp (chính xác, wildcard, chứa, hoặc regex) được đặt trong dấu ngoặc kép | `"*.exe"`, `"lsass.exe"` |

## Ghi chú

- Bảng trên chỉ dựa trên các cấu trúc xuất hiện trong `sample.rules`; chưa có spec chính thức nào khác trong thư mục dự án.
- R8 (`~`) và R9 (`=~`) là toán tử bổ sung theo yêu cầu, chưa xuất hiện thực tế trong `sample.rules` — cần thống nhất thêm về độ ưu tiên/escape khi kết hợp với `~~` và `=`.
- Danh sách `op` hiện quan sát được: `file_create`, `process_create`, `process_open`. Chưa rõ danh sách đầy đủ các `op` được hỗ trợ.
- Chưa rõ các giá trị `scope` khác ngoài `related_actors`, và hành vi mặc định khi không khai báo `scope`.
