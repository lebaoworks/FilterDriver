# Build: kfilter-runtime + kfilter-compiler (Rust) -> driver (WDM, C++)

Cấu trúc (đã build thành công đầu-cuối trên host thật qua cmd, không phải suy đoán):

```
filter/
  kfilter-runtime/    # part 2: no_std staticlib, load + match, link vào driver
  kfilter-compiler/   # part 1: std binary, build rule -> sparse DFA, gửi cho driver qua IOCTL
  driver/              # VS solution, WDM driver (Driver.cpp)
```

`kfilter-runtime` và `kfilter-compiler` là **2 project Cargo độc lập** (không phải 1 Cargo
workspace) -- lý do: gộp workspace khiến Cargo hợp nhất feature của `regex-automata` giữa 2 crate
(kfilter-compiler bật `std`, kfilter-runtime thì không) và phá vỡ tính `no_std` của
kfilter-runtime ("duplicate lang item `panic_impl`"). Build riêng từng cái tránh vấn đề này.

## 1. Build kfilter-runtime (part 2 -- load + match, link vào driver)

```
cd kfilter-runtime
cargo build --release
```

Output: `kfilter-runtime\target\x86_64-pc-windows-msvc\release\kfilter_core.lib`

Không còn `build.rs`/dữ liệu nhúng lúc compile. Rule data đến lúc **runtime** qua `kfilter_load`,
được driver gọi từ IOCTL handler. Bộ nhớ cho blob nhận được quản lý thủ công bằng
`ExAllocatePool2`/`ExFreePoolWithTag` (không dùng `alloc` crate của Rust -- không cần
`#[global_allocator]`). Có cơ chế refcount + spinlock để `kfilter_match` không giữ lock trong lúc
search, còn `kfilter_load` chỉ free ruleset cũ sau khi không còn match nào đang dùng nó.

## 2. Build kfilter-compiler (part 1 -- build rule -> data, gửi cho driver)

```
cd kfilter-compiler
cargo build --release
```

Output: `kfilter-compiler\target\x86_64-pc-windows-msvc\release\kfilter-compiler.exe`

Chạy `kfilter-compiler.exe <path-to-.rules>` (**sau khi** driver đã load -- xem phần "Nạp rule vào
driver đang chạy" bên dưới) sẽ: parse file `.rules` thật (pattern/step/op/field, 4 toán tử
`= ~~ ~ =~`), biên dịch từng điều kiện field thành regex, build sparse DFA, ghi ra
`kfilter_rules.dfa`, và gửi cho driver qua `DeviceIoControl` tới `\\.\KFilter`.

Đã test với cả `sample.rules` (2 pattern biên dịch được, 2 step bị skip có lý do rõ ràng -- xem
dưới) và `registry_mitre.rules` (178 rule thật từ MITRE ATT&CK -> 250 pattern) -- kết quả khớp
byte-for-byte với một benchmark tham chiếu độc lập (`registry_dfa_bench`, xem mục dưới).

**Điều kiện KHÔNG được biên dịch** (bị skip kèm cảnh báo ra stderr, không phải lỗi cứng -- đúng
với việc kernel filter chỉ prefilter, không correlate):
- Điều kiện tham chiếu `$step.field` (so sánh chéo giữa 2 step, cần state lúc runtime, không thể
  biên dịch thành regex tĩnh). Ví dụ: `drop_and_run.exec` trong `sample.rules`.
- Step không có field condition nào (cần kiến trúc DFA-theo-op để biểu diễn "khớp bất cứ khi nào
  op này xảy ra", chưa làm). Ví dụ: `dump_lsass.dump` trong `sample.rules`.

**`matched_line` = số dòng trong file `.rules`, không phải index nội bộ của DFA.** Mỗi điều kiện
field compile thành 1 pattern trong DFA (pattern id 0..N-1 nội bộ), nhưng `kfilter-compiler` giữ
song song 1 bảng `pattern_id -> line_number` (dòng chứa `step` sinh ra điều kiện đó), gửi kèm DFA
cho driver. `kfilter_match` tự tra bảng này trước khi trả về, nên caller (C++ driver, và sau này
là consumer đọc log) nhận thẳng số dòng source -- mở đúng dòng đó trong `.rules` là biết ngay rule
nào khớp, không cần tra thêm bảng riêng ở tầng khác.

Giới hạn cần biết: nếu 2 điều kiện ở 2 dòng khác nhau compile ra **cùng 1 regex y hệt** (có thật
trong `registry_mitre.rules`, ví dụ nhiều technique cùng theo dõi 1 registry key), DFA chỉ báo
đúng 1 trong 2 dòng đó khi khớp (semantics "pattern ưu tiên thấp nhất thắng" của
`build_many`/`try_search_fwd`). Chấp nhận được cho mục đích prefilter (kernel chỉ cần biết "có gì
đó đáng quan tâm" để đẩy event lên); muốn biết **đầy đủ** mọi rule khớp thì tầng correlation
user-mode phải tự so lại toàn bộ ruleset trên event đã forward, không dựa vào 1 `matched_line` duy
nhất.

## 3. Build driver

Từ **Developer Command Prompt for VS**:

```
cd driver
msbuild driver.sln /p:Configuration=Release /p:Platform=x64
```

Output: `driver\x64\Release\kfilterdrv.sys`

## Nạp rule vào driver đang chạy (chưa làm trong phiên này)

Driver hiện build **unsigned** (`SignMode=Off`) và **chưa được load/cài đặt** lên máy -- đây là
bước có rủi ro thật (driver lỗi có thể BSOD máy), nên mình dừng lại ở "build thành công", chưa tự
ý thực hiện. Muốn test thật cần, theo thứ tự:

1. Bật test-signing: `bcdedit /set testsigning on` rồi **reboot**.
2. Test-sign `kfilterdrv.sys` (SignTool + cert tự tạo, hoặc bật lại `SignMode=TestSign` trong
   vcxproj để MSBuild tự làm).
3. Cài + start service: `sc create kfilterdrv type= kernel binPath= <path>\kfilterdrv.sys` rồi
   `sc start kfilterdrv`.
4. Chạy `kfilter-compiler.exe` -- nó mở `\\.\KFilter` và gửi rule qua IOCTL.
5. Xem log qua DebugView (`DbgPrint` trong `Driver.cpp`).
6. `sc stop kfilterdrv` / `sc delete kfilterdrv` khi xong.

Nói mình biết nếu muốn thực hiện các bước này -- đây là hành động có thể ảnh hưởng máy thật nên
để bạn xác nhận trước.

## Bảo mật IOCTL

Device `\\.\KFilter` được tạo bằng `IoCreateDeviceSecure` + `SDDL_DEVOBJ_SYS_ALL_ADM_ALL`
(`wdmsec.lib`) -- chỉ SYSTEM và Administrators mở được. Vì IOCTL này **thay thế toàn bộ ruleset
đang lọc**, để mở cho mọi process sẽ là lỗ hổng nghiêm trọng (một process thường có thể tự gửi
ruleset rỗng để "làm mù" bộ lọc).

## API kernel đã verify trực tiếp trên WDK headers/lib (không đoán)

Vài API kernel không đúng như tên trong `wdm.h` gợi ý -- đã kiểm bằng `findstr`/`dumpbin` trên
WDK 10.0.26100.0 thật thay vì tin theo trí nhớ:

- `KeAcquireSpinLock`/`KeReleaseSpinLock` chỉ là **macro** trong header. Trên x64, macro acquire
  expand thành `KeAcquireSpinLockRaiseToDpc` (không phải `KfAcquireSpinLock` -- symbol đó chỉ tồn
  tại cho x86, không có trong `ntoskrnl.lib` bản x64, gây lỗi `LNK2019` lúc đầu). `KeReleaseSpinLock`
  thì được export thẳng dưới tên đó trên x64.
- `POOL_FLAG_NON_PAGED = 0x40`, `POOL_FLAGS` là `ULONG64` -- đúng như đoán, nhưng đã confirm qua
  `wdm.h` thay vì suy đoán.
- `ExAllocatePool2`/`ExFreePoolWithTag` là symbol thật, export trực tiếp.
- `IoCreateDeviceSecure` + `SDDL_DEVOBJ_SYS_ALL_ADM_ALL` cần link thêm `wdmsec.lib` (đã thêm vào
  `driver.vcxproj`).

## File tham chiếu phát hiện giữa chừng (không phải do phiên này tạo)

`kfilter-compiler/src/bin/registry_dfa_bench/` (đã dọn vào thư mục con để không phá build --
Cargo tự coi mọi file `.rs` trực tiếp trong `src/bin/` là 1 binary riêng, kể cả file chỉ nhằm làm
module phụ trợ) chứa 1 benchmark + bảng 250 regex tham chiếu, ghi "AUTO-GENERATED by
scratchpad/gen_outputs.py" -- không phải file mình tạo ra trong phiên này, khả năng cao đến từ một
phiên/agent khác đang làm song song trên cùng project (không tìm thấy `gen_outputs.py` trong repo).
Đã dùng nó để đối chiếu: parser thật cho ra DFA **giống hệt kích thước** (380863 bytes) với bảng
tham chiếu đó -- xác nhận độc lập logic escape/regex đúng. Không xoá gì, chỉ di chuyển để build
chạy được; báo bạn biết vì đây là file lạ xuất hiện giữa phiên, không phải để bạn phải xử lý gì.

## Việc chưa làm (ngoài phạm vi lần này)

- Hook thu sự kiện kernel thật (`ObRegisterCallbacks`/minifilter/process-notify) gọi vào
  `kfilter_match` trên hot path.
- Tổ chức nhiều DFA theo (op, field) thay vì 1 DFA duy nhất (đã bàn hướng thiết kế, chưa áp dụng
  vào code) -- sẽ cũng giải quyết luôn giới hạn "duplicate pattern" và "step không có field
  condition" nêu ở mục trên.
- Test thật driver trên máy (test-sign + load) -- xem mục "Nạp rule vào driver đang chạy" ở trên.
