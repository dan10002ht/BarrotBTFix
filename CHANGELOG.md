# Changelog

## 1.1.0

- Force the public own address in both `LE_Set_Scan_Parameters` call sites, so `LE_Set_Scan_Enable` no longer fails with `0x12` and `bluetoothd` stops restarting (reason 718).
- First public release. Bundle identifier `io.github.dan10002ht.BarrotBTFix`.

## 1.0.0

- Ignore the `HCI_Host_Buffer_Size` (0x0C33) error during controller initialization.
