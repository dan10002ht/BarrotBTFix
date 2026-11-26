# Technical notes

All offsets refer to the x86_64 slice of `/usr/sbin/bluetoothd` from macOS 15.8.3 (24H241).

## Background

On macOS 12 and later, `bluetoothd` only accepts Apple's internal controllers. BlueToolFixup removes that restriction, and many generic USB dongles (CSR8510, BCM20702) then work. Barrot dongles (`33fa:0010`) still fail: `bluetoothd` sets `bluetoothExternalDongleFailed = 01` in NVRAM and keeps restarting.

An HCI trace (`PacketLogger` / `bluetoothd` HCI logging) showed that over 20 initialization commands succeed. The controller reports a valid address, Bluetooth 5.4 and LE support. Exactly two places fail.

## Patch 1: HCI_Host_Buffer_Size (0x0C33)

The Barrot controller answers `HCI_Host_Buffer_Size` with status `0x11` (Unsupported Feature or Parameter Value). That is allowed by the Core spec: the command is only needed when the host enables controller-to-host flow control.

`initCmdHostBufferSizeCB` passes the status to the next init step, and a non-zero status aborts the whole initialization. Right after it, Apple already ignores the same error for `HCI_Set_Controller_To_Host_Flow_Control` (log line `HostFlowControl unsupported (ignoring)`).

The patch clears the status that is passed on, while the original value is still logged:

```
initCmdHostBufferSizeCB + 7
    89 fb        mov  %edi, %ebx      ->   31 db   xor %ebx, %ebx
```

32 bytes of context are matched to keep the match unique.

### Why the pattern is not masked

The function has three siblings with identical code, differing only in the RIP-relative address of their log string. Masking that `lea` displacement (to survive minor macOS updates) matches **4 functions** instead of 1, which would silently ignore errors from three other init commands. The pattern is therefore exact. It must be updated per macOS build.

## Patch 2 and 3: LE scan own address type

After initialization, `bluetoothd` sends:

```
LE_Set_Scan_Parameters (0x200B)  Own_Address_Type = 0x01 (random)
LE_Set_Scan_Enable     (0x200C)  -> Command Complete, status 0x12
```

`LE_Set_Random_Address` (0x2005) is never sent to a third-party controller. Per the Core spec (Vol 4, Part E, 7.8.11), enabling scanning with a random own address that has not been set must fail with `0x12` (Invalid HCI Command Parameters). The Barrot chip does exactly that, while Apple's controllers apparently tolerate it. `bluetoothd` treats the failure as fatal (reason 718) and restarts about every 10 seconds.

The `LE` → `OverrideUsePublicAddress` preference exists, but its handler is empty in release builds.

`bluetoothd` builds HCI commands through a table-driven sender. `LE_Set_Scan_Parameters` is table index `0xAB`. Both call sites pass `Own_Address_Type` in `%r9d`, and the patch forces it to 0 (public):

```
site 1 (0x1001F2686 + 26)
    41 89 c1        mov    %eax, %r9d              ->  45 31 c9        xor %r9d, %r9d

site 2 (0x1001F2D66)
    44 0f b6 4d d0  movzbl -0x30(%rbp), %r9d       ->  45 31 c9 66 90  xor %r9d, %r9d ; nop
```

Scanning with the controller's public address works the same for passive and active scans.

## Patching mechanism

Like BlueToolFixup, the plugin routes the kernel function `cs_validate_page`. After the original validates a page of a file being mapped, the plugin checks whether the file is `/usr/sbin/bluetoothd` and runs `KernelPatcher::findAndReplace` for each pattern on that 4 KiB page. Each pattern lies entirely within one page, so none is split across a page boundary.

Nothing on disk is modified. Note that the patched page lives in the unified buffer cache, so reading `/usr/sbin/bluetoothd` while the kext is loaded returns the patched bytes.

## Known gaps

- `LE_Create_Connection` (0x200D) and extended variants probably carry the same random own address type. BLE peripherals may need the same fix there.
- The chip sends one extra byte in some event parameters (the bug the Linux kernel quirk handles). On macOS this only produces a warning so far.

## Porting to a new macOS build

1. Boot without the kext (or with `-barrotoff`) so the on-disk bytes are original.
2. `lipo /usr/sbin/bluetoothd -thin x86_64 -output bt`
3. Find `initCmdHostBufferSizeCB` (the binary is stripped, but the function name is a log string: follow its cross-reference) and the two `LE_Set_Scan_Parameters` call sites (`mov $0xab, %edi` near the scan interval constants).
4. Add the new 32-byte patterns in `src/BarrotBTFix.cpp`, check each matches exactly once and sits within one 4 KiB page.
