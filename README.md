# BarrotBTFix

A [Lilu](https://github.com/acidanthera/Lilu) plugin that makes **Barrot-based USB Bluetooth dongles** (USB vendor `33fa`, for example the **UGREEN BT 5.4** adapter) work on **macOS 15 Sequoia** Hackintoshes.

[Tiếng Việt bên dưới ↓](#tiếng-việt)

> **Status:** experimental. Tested on one machine with macOS 15.8.3 (24H241). Audio over A2DP works. BLE devices (mice, keyboards) are not confirmed yet.

## What works

| Feature | Status |
|---|---|
| Controller initializes, Bluetooth shows **On** with a real address | ✅ |
| Bluetooth speakers and headphones (A2DP) | ✅ tested |
| `bluetoothd` stays up (no restart loop with reason 718) | ✅ |
| Hands-free (HFP) microphone | ❓ not tested |
| BLE mice and keyboards | ❓ not tested, may need another patch (LE Create Connection) |
| AirDrop, Handoff, Continuity | ❌ needs a real Apple/Broadcom card |

## Supported hardware

| USB ID | Product | Status |
|---|---|---|
| `33fa:0010` | UGREEN BT 5.4 USB adapter (Barrot BR8554) | ✅ tested |
| `33fa:0012` | Barrot BT 6.0 adapters | ❓ probably, not tested |

Check yours with: `ioreg -p IOUSB -l -w0 | grep -E '"idVendor" = 13306'` (13306 = `0x33fa`).

## Supported macOS

The patches match exact byte sequences in `/usr/sbin/bluetoothd` (x86_64). They are **only verified on macOS 15.8.3 (24H241)**. The kext only loads on Sequoia (Darwin 24).

On any other build the patterns are simply not found and **nothing is patched**: the system stays safe, Bluetooth just won't work. After a macOS update, check `log show --last boot --predicate 'eventMessage CONTAINS "barrot"'` and open an issue.

## Installation (OpenCore)

1. Download `BarrotBTFix-x.y.z.zip` from [Releases](../../releases) and copy `BarrotBTFix.kext` to `EFI/OC/Kexts/`.
2. You also need **[BlueToolFixup.kext](https://github.com/acidanthera/BrcmPatchRAM/releases)** (from BrcmPatchRAM). It lets macOS 12+ use a non-Apple USB controller.
3. `config.plist → Kernel → Add`, in this order:
   1. `Lilu.kext`
   2. `BlueToolFixup.kext`
   3. `BarrotBTFix.kext`, with `MinKernel` = `24.0.0` and `MaxKernel` = `24.99.99`
4. `config.plist → NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82`:

   | Key | Type | Value |
   |---|---|---|
   | `bluetoothExternalDongleFailed` | Data | `00` |
   | `bluetoothInternalControllerInfo` | Data | `0000000000000000000000000000` (14 bytes) |

   Add both keys to `NVRAM → Delete → 7C436110-AB2A-4BBB-A880-FE41995C9F82` as well, so the values are rewritten on every boot.
5. Make sure the dongle's port is in your USB map, then reboot.

Check that it worked:

```sh
log show --last boot --predicate 'eventMessage CONTAINS "barrot"' | grep patched
system_profiler SPBluetoothDataType | grep -E 'State|Address'
```

You should see three `patched ...` lines and `State: On`.

## Boot arguments

| Argument | Effect |
|---|---|
| `-barrotoff` | Disable the plugin |
| `-barrotdbg` | Verbose logging (DEBUG Lilu only) |
| `-barrotbeta` | Allow loading on unsupported macOS versions |

## How it works

macOS initializes the controller, but the Barrot chip behaves slightly differently from Apple's chips in two places, and `bluetoothd` gives up:

1. **`HCI_Host_Buffer_Size` (0x0C33) is rejected** with *Unsupported Feature*. `bluetoothd` aborts initialization. Apple already ignores the same error for the next command (Host Flow Control), so the plugin does the same here.
2. **LE scanning uses a random address that was never set.** `LE_Set_Scan_Parameters` asks for `Own_Address_Type = random`, but `bluetoothd` never sends `LE_Set_Random_Address` to third-party controllers. The Barrot chip correctly rejects `LE_Set_Scan_Enable` with `0x12`, and `bluetoothd` restarts every ~10 seconds. The plugin makes both call sites use the public address.

The plugin hooks `cs_validate_page` (the same technique BlueToolFixup uses) and patches these bytes in memory when `bluetoothd` is loaded. Nothing on disk is modified. Details: [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Building

Requires Xcode Command Line Tools. No Xcode project needed.

```sh
./build.sh          # build/BarrotBTFix.kext and dist/BarrotBTFix-<version>.zip
./build.sh clean
```

The script downloads Lilu (DEBUG, for the SDK) and MacKernelSDK into `deps/`, pinned to fixed versions.

## Credits

- [acidanthera](https://github.com/acidanthera): Lilu, MacKernelSDK, BrcmPatchRAM / BlueToolFixup.
- [thanhnndev/mt7925-macos](https://github.com/thanhnndev/mt7925-macos): the `bluetoothd` patching approach for non-Apple controllers (BtLeDlFix).
- The Linux kernel Barrot quirk and [nwrafael/CM748-ugreen-bluetooth-adapter-patch-linux](https://github.com/nwrafael/CM748-ugreen-bluetooth-adapter-patch-linux), for documenting the chip's behavior.

## License

[BSD-3-Clause](LICENSE)

---

## Tiếng Việt

**BarrotBTFix** là plugin Lilu giúp **dongle Bluetooth USB dùng chip Barrot** (vendor `33fa`, ví dụ **UGREEN BT 5.4**) chạy được trên hackintosh **macOS 15 Sequoia**. Nhà sản xuất ghi "không hỗ trợ Mac OS", nhưng với plugin này dongle chạy được.

> **Trạng thái:** thử nghiệm. Mới thử trên một máy chạy macOS 15.8.3 (24H241). Loa và tai nghe Bluetooth (A2DP) đã chạy. Chuột và bàn phím BLE chưa được xác nhận.

### Cài đặt (OpenCore)

1. Tải `BarrotBTFix-x.y.z.zip` ở mục [Releases](../../releases), chép `BarrotBTFix.kext` vào `EFI/OC/Kexts/`.
2. Cần thêm **[BlueToolFixup.kext](https://github.com/acidanthera/BrcmPatchRAM/releases)** của acidanthera.
3. Trong `config.plist → Kernel → Add`, xếp theo thứ tự: `Lilu.kext` → `BlueToolFixup.kext` → `BarrotBTFix.kext` (đặt `MinKernel` = `24.0.0`, `MaxKernel` = `24.99.99`).
4. Trong `NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82`, thêm:
   - `bluetoothExternalDongleFailed` (Data) = `00`
   - `bluetoothInternalControllerInfo` (Data) = `0000000000000000000000000000` (14 byte)

   Thêm 2 khoá này vào `NVRAM → Delete` cùng GUID để giá trị được ghi lại mỗi lần khởi động.
5. Đảm bảo cổng USB cắm dongle có trong USB map, rồi khởi động lại.

Kiểm tra: chạy `log show --last boot --predicate 'eventMessage CONTAINS "barrot"' | grep patched`, phải thấy 3 dòng `patched ...`. Trong **Cài đặt hệ thống → Bluetooth**, Bluetooth phải ở trạng thái bật.

### Lưu ý khi cập nhật macOS

Plugin vá đúng chuỗi byte của `bluetoothd` bản **15.8.3 (24H241)**. Sau khi cập nhật macOS, nếu không tìm thấy chuỗi byte thì plugin **không vá gì cả**: máy vẫn an toàn nhưng Bluetooth sẽ không chạy. Khi đó hãy mở issue kèm phiên bản macOS (`sw_vers`).

### Boot-args

- `-barrotoff`: tắt plugin
- `-barrotdbg`: ghi log chi tiết (cần Lilu bản DEBUG)
- `-barrotbeta`: cho phép chạy trên bản macOS chưa hỗ trợ
