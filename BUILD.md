# Build: kfilter-lib + kfilter-compiler + kfilter-cli (Rust) -> driver (WDM, C++)

Cấu trúc (đã build thành công đầu-cuối trên host thật qua cmd, không phải suy đoán):

```
filter/
  kfilter-lib/         # "lib": Ruleset (load blob + match_state) -- portable core, dùng chung
  kfilter-compiler/     # "compiler": lib (parse .rules + build DFA/state-map) + bin (CLI gửi IOCTL)
  kfilter-cli/           # "cli": import cả 2 cái trên, mô phỏng filter offline trên file event
  driver/                 # VS solution, WDM driver (Driver.cpp), link kfilter-lib bản kernel
```

Cả 3 crate Rust là **project Cargo độc lập** (không phải 1 workspace) -- gộp workspace khiến Cargo
hợp nhất feature `regex-automata` giữa các crate std/no_std khác nhau, phá vỡ build no_std (xem
lịch sử: lỗi "duplicate lang item `panic_impl`"). Build riêng từng cái tránh vấn đề này.

## kfilter-lib: 2 chế độ build qua Cargo feature `kernel`

`kfilter-lib` chứa đúng 1 kiểu `Ruleset` (load DFA blob + `match_state`) -- **không cấp phát bộ
nhớ, không phụ thuộc OS** -- nên dùng chung được cho cả 2 nơi:

- **Mặc định** (không feature gì): rlib bình thường, link thẳng `std`. Dùng bởi `kfilter-cli`.
- **`kernel`**: thêm `#![no_std]` + module FFI (`ExAllocatePool2`-based pool allocator, refcount +
  spinlock, export `kfilter_init/kfilter_load/kfilter_match/kfilter_unload`). Driver link bản này.

```
cd kfilter-lib
cargo build --release                                              # cho kfilter-cli
cargo build --release --features kernel -Z build-std=core,compiler_builtins --target x86_64-pc-windows-msvc   # cho driver
```

**`-Z build-std=core,compiler_builtins` là bắt buộc cho bản kernel, không phải tuỳ chọn.** Lý do
(phát hiện thật qua link error, không phải đọc doc suông): `-C panic=abort` chỉ áp cho code của
chính crate mình, **không** áp lại cho `core`/`compiler_builtins` lấy sẵn từ sysroot rustc (chúng
được compile 1 lần, đóng gói cùng toolchain, với panic=unwind mặc định). Hậu quả: link driver báo
`LNK2001: unresolved external symbol __CxxFrameHandler3` (personality routine cho SEH unwind) --
biến mất hoàn toàn sau khi thêm `-Z build-std` để rebuild `core`/`compiler_builtins` từ source với
đúng `panic=abort`. Máy bạn có sẵn toolchain nightly nên dùng `-Z` được luôn, không cần cấu hình gì
thêm.

Còn `_fltused` (linker MSVC yêu cầu bất cứ khi nào code đụng thanh ghi XMM/SSE, kể cả không tính
toán dấu phẩy động thật -- đây không phải hàm được gọi lúc unwind mà chỉ là symbol đánh dấu, an
toàn để tự định nghĩa) -- đã thêm `#[no_mangle] pub static _fltused: i32 = 0;` trong module kernel
của `kfilter-lib`.

Output kernel: `kfilter-lib\target\x86_64-pc-windows-msvc\release\kfilter_lib.lib`

Không còn `build.rs`/dữ liệu nhúng lúc compile. Rule data đến lúc **runtime** qua `kfilter_load`,
được driver gọi từ IOCTL handler. `kfilter_load` nhận **1 blob đóng gói nhiều DFA** (1 DFA/`(op,
field)`, xem mục kfilter-compiler ngay dưới) -- tự parse header + từng entry, copy vào pool memory
bằng `ExAllocatePool2`/`ExFreePoolWithTag` (không dùng `alloc` crate của Rust -- không cần
`#[global_allocator]`). `DfaSlot` giờ giữ 1 mảng `Entry{op,field,Ruleset}` thay vì 1 `Ruleset` duy
nhất -- `kfilter_match` nhận thêm `op`/`field` (byte slice) để chọn đúng entry trước khi match. Vẫn
giữ cơ chế refcount + spinlock để `kfilter_match` không giữ lock trong lúc search, còn `kfilter_load`
chỉ free ruleset cũ sau khi không còn match nào đang dùng nó.

## kfilter-compiler: lib (logic) + bin (CLI gửi IOCTL)

```
cd kfilter-compiler
cargo build --release
```

Output: `kfilter-compiler\target\x86_64-pc-windows-msvc\release\kfilter-compiler.exe`

Logic parse `.rules` + compile nằm ở `src/lib.rs` (`kfilter_compiler` crate) -- `kfilter-cli` import
thẳng, không phải shell ra binary. `src/main.rs` là CLI mỏng dùng lại lib đó: parse `.rules`
(pattern/step/op/field, 4 toán tử `= ~~ ~ =~`), biên dịch từng điều kiện thành regex.

**Build 1 DFA riêng cho mỗi cặp (op, field) khác nhau** (`compile_ruleset` trả `CompiledRuleset {
entries: Vec<RulesetEntry> }`), không phải 1 DFA phẳng dùng chung như thiết kế trước. Lý do: đo
thật trên `registry_mitre.rules` (250 pattern) cho thấy tách theo (op,field) **nhỏ hơn** (162,225
so với 390,956 bytes, giảm 58.5%), **build nhanh hơn** (29.8ms so với 241.7ms, ~8x), và
**match nhanh hơn** (331ns so với 441ns/lần gọi, ~25%) so với 1 DFA phẳng -- không phải đánh đổi,
tách ra tốt hơn trên cả 3 mặt vì DFA phẳng phải giữ thêm state chỉ để phân biệt các tổ hợp pattern
từ những field không liên quan, việc không bao giờ thực sự cần thiết (1 field chỉ bao giờ test với
pattern của đúng field đó).

Mỗi entry ghi kèm `state_map: BTreeMap<u32, Vec<LineInfo>>` riêng (không cần `op`/`field` trong
`LineInfo` nữa như thiết kế trước -- entry đã tự scope theo đúng 1 (op,field), không còn nguy cơ
field khác lẫn kết quả). `serialize_entries` đóng gói tất cả entry thành 1 blob theo wire-format:
`[magic][version][entry_count]` rồi lặp lại `[op_len][op][field_len][field][dfa_len][dfa bytes]` --
gửi qua `DeviceIoControl` tới `\\.\KFilter`, đồng thời ghi `kfilter_rules.dfa` (bản blob này) +
`kfilter_state_map.json` (lồng theo từng entry) ra đĩa.

**Điều kiện KHÔNG được biên dịch** (bị skip kèm cảnh báo ra stderr, không phải lỗi cứng -- đúng
với việc kernel filter chỉ prefilter, không correlate):
- Điều kiện tham chiếu `$step.field` (so sánh chéo giữa 2 step, cần state lúc runtime, không thể
  biên dịch thành regex tĩnh). Ví dụ: `drop_and_run.exec` trong `sample.rules`.
- Step không có field condition nào (cần kiến trúc DFA-theo-op để biểu diễn "khớp bất cứ khi nào
  op này xảy ra", chưa làm). Ví dụ: `dump_lsass.dump` trong `sample.rules`.

## kfilter-cli: mô phỏng offline, không cần driver thật

```
cd kfilter-cli
cargo build --release
kfilter-cli.exe <rules-file> <events-file.evt>
```

Import cả `kfilter_lib` (mặc định, không bật `kernel`) lẫn `kfilter_compiler` (làm thư viện): parse
+ compile `.rules` -> mỗi entry `(op,field)` load thành 1 `Ruleset` riêng (đúng code path driver
dùng cho từng entry, chỉ khác không có FFI kernel). Toàn bộ chạy user-mode, không cần driver load,
không cần IOCTL -- dùng để test rule nhanh.

**File `.evt`**: mỗi dòng 1 event có cấu trúc thật -- `op=<op> <field1>="<value1>"
<field2>="<value2>" ...` (cùng kiểu quote/escape `\\`/`\"` như `.rules`). Ví dụ:
`op=registry_set key_path="HKLM\..." value_name="ImagePath"`.

**Đánh giá AND/OR thật**: với mỗi field của event, tra `(event.op, field_name)` ra đúng entry (1
`HashMap` lookup, O(1)), chạy `match_state` của entry đó (đúng 1 lần/field, giống hệt kernel sẽ
làm) rồi tra `state_map` **của riêng entry đó** -- không cần lọc `op`/`field` thủ công sau khi
match nữa như thiết kế trước, vì entry đã tự scope đúng 1 field rồi (DFA không còn cách nào trả về
kết quả của field khác). 1 group (AND) coi là thoả khi đã thấy đủ **hết** field mà group đó yêu
cầu; step thoả khi có ít nhất 1 group thoả (OR).

Test thật xác nhận hành vi **giống hệt** thiết kế lọc thủ công trước đó (đúng như kỳ vọng, vì ngữ
nghĩa AND/OR không đổi, chỉ đổi cách triển khai): chuỗi hoàn toàn không phải registry path
(`C:\this\is\not\a\registry\key`) báo đúng "no step fully satisfied"; case AND thiếu 1 field
(`key_path` có, `value_name` thiếu cho `suspicious_registry_write` group 0) cũng báo đúng không
thoả thay vì false-positive.

## Build driver

Từ **Developer Command Prompt for VS** (sau khi đã build `kfilter-lib` bản `--features kernel` ở
trên):

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
4. Chạy `kfilter-compiler.exe <rules-file>` -- nó mở `\\.\KFilter` và gửi rule qua IOCTL.
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
- `-C panic=abort` không tự áp cho `core`/`compiler_builtins` lấy từ sysroot -- cần `-Z build-std`
  (xem mục kfilter-lib ở trên) để tránh `LNK2001: __CxxFrameHandler3`.

## File tham chiếu phát hiện giữa chừng (không phải do phiên này tạo)

`kfilter-compiler/src/bin/registry_dfa_bench/` (đã dọn vào thư mục con để không phá build --
Cargo tự coi mọi file `.rs` trực tiếp trong `src/bin/` là 1 binary riêng, kể cả file chỉ nhằm làm
module phụ trợ) chứa 1 benchmark + bảng 250 regex tham chiếu, ghi "AUTO-GENERATED by
scratchpad/gen_outputs.py" -- không phải file mình tạo ra trong phiên này, khả năng cao đến từ một
phiên/agent khác đang làm song song trên cùng project (không tìm thấy `gen_outputs.py` trong repo).
Đã dùng nó để đối chiếu: parser thật cho ra DFA **giống hệt kích thước** với bảng tham chiếu đó --
xác nhận độc lập logic escape/regex đúng. Không xoá gì, chỉ di chuyển để build chạy được.

## Việc chưa làm (ngoài phạm vi lần này)

- Hook thu sự kiện kernel thật (`ObRegisterCallbacks`/minifilter/process-notify) gọi vào
  `kfilter_match` trên hot path. **Driver (C++) chưa implement logic AND/OR** -- logic đó hiện chỉ
  có trong `kfilter-cli` (Rust); khi nối hook thật, `Driver.cpp` cần cùng logic (gọi `kfilter_match`
  đúng op/field cho từng field của event, gom kết quả theo group, so với field group yêu cầu) --
  hoặc để hẳn ở tầng user-mode nhận log từ driver, tuỳ quyết định kiến trúc lúc đó. Việc lọc
  op/field bản thân **không còn cần làm thủ công nữa** (đã chuyển vào cấu trúc DFA, xem mục
  kfilter-compiler/kfilter-lib ở trên).
- Step không có field condition (`step dump op=file_create`, không field nào) vẫn bị skip -- cần
  kiến trúc riêng để biểu diễn "khớp mọi lúc op này xảy ra, không cần check field nào" (có thể tận
  dụng luôn cấu trúc entry theo op hiện có, ví dụ 1 entry đặc biệt "op này luôn khớp").
- Test thật driver trên máy (test-sign + load) -- xem mục "Nạp rule vào driver đang chạy" ở trên.
