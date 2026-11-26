//
//  BarrotBTFix.cpp
//  SPDX-License-Identifier: BSD-3-Clause
//  Lilu plugin: lets macOS bluetoothd accept Barrot (33fa) USB Bluetooth dongles
//  that reject HCI_Host_Buffer_Size (0x0C33) with "Unsupported Feature" (0x11).
//
//  bluetoothd's initCmdHostBufferSizeCB passes the command status on to the next
//  init step and aborts HCI init when it is non-zero. Apple already ignores the
//  same error for the following Host Flow Control command. This patch makes the
//  callback report success (status still gets logged).
//
//  macOS 15.8.3 (24H241) x86_64 bluetoothd, initCmdHostBufferSizeCB + 7:
//      89 fb    mov %edi,%ebx   ->   31 db    xor %ebx,%ebx
//  The 32-byte context below is unique in that binary; any other build is left alone.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_user.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_version.hpp>

#define MODULE_SHORT "barrot"

static const uint8_t kHostBufSizeOriginal[] = {
	0x89, 0xFB, 0x85, 0xFF, 0x74, 0x41, 0xBF, 0x02, 0x00, 0x00, 0x00, 0xBE, 0x2E, 0x00, 0x00, 0x00,
	0xE8, 0x03, 0x58, 0xF8, 0xFF, 0x84, 0xC0, 0x74, 0x2E, 0x48, 0x8D, 0x3D, 0x16, 0xCC, 0x73, 0x00
};

static const uint8_t kHostBufSizePatched[] = {
	0x31, 0xDB, 0x85, 0xFF, 0x74, 0x41, 0xBF, 0x02, 0x00, 0x00, 0x00, 0xBE, 0x2E, 0x00, 0x00, 0x00,
	0xE8, 0x03, 0x58, 0xF8, 0xFF, 0x84, 0xC0, 0x74, 0x2E, 0x48, 0x8D, 0x3D, 0x16, 0xCC, 0x73, 0x00
};

static_assert(sizeof(kHostBufSizeOriginal) == sizeof(kHostBufSizePatched), "patch size mismatch");

// LE Set Scan Parameters (HCI 0x200B, command table index 0xAB) is sent with
// Own_Address_Type = 0x01 (random), but bluetoothd never sends LE_Set_Random_Address
// to a third-party controller. Per Core spec the Barrot chip then rejects
// LE Set Scan Enable with 0x12, and bluetoothd restarts (reason 718).
// Both call sites of the generic command sender pass Own_Address_Type in r9d;
// force it to 0 (public address). Passive/active scanning works the same.

// Site 1 (0x1001F2686): ... mov $0xab,%edi; mov -0x64(%rbp),%edx; ...; mov %eax,%r9d -> xor %r9d,%r9d
static const uint8_t kScanParams1Original[] = {
	0x48, 0x83, 0xEC, 0x08, 0x48, 0x8D, 0x35, 0xEB, 0x00, 0x00, 0x00, 0xBF, 0xAB, 0x00, 0x00, 0x00,
	0x8B, 0x55, 0x9C, 0x8B, 0x4D, 0xA0, 0x44, 0x8B, 0x45, 0xA4, 0x41, 0x89, 0xC1, 0x31, 0xC0, 0xFF
};
static const uint8_t kScanParams1Patched[] = {
	0x48, 0x83, 0xEC, 0x08, 0x48, 0x8D, 0x35, 0xEB, 0x00, 0x00, 0x00, 0xBF, 0xAB, 0x00, 0x00, 0x00,
	0x8B, 0x55, 0x9C, 0x8B, 0x4D, 0xA0, 0x44, 0x8B, 0x45, 0xA4, 0x45, 0x31, 0xC9, 0x31, 0xC0, 0xFF
};

// Site 2 (0x1001F2D66): movzbl -0x30(%rbp),%r9d -> xor %r9d,%r9d; xchg %ax,%ax (2-byte nop)
static const uint8_t kScanParams2Original[] = {
	0x44, 0x0F, 0xB6, 0x4D, 0xD0, 0x44, 0x0F, 0xB6, 0x15, 0x42, 0xD2, 0x86, 0x00, 0x48, 0x83, 0xEC,
	0x08, 0x48, 0x8D, 0x35, 0x8E, 0x04, 0x00, 0x00, 0xBF, 0xAB, 0x00, 0x00, 0x00, 0x31, 0xC0, 0x41
};
static const uint8_t kScanParams2Patched[] = {
	0x45, 0x31, 0xC9, 0x66, 0x90, 0x44, 0x0F, 0xB6, 0x15, 0x42, 0xD2, 0x86, 0x00, 0x48, 0x83, 0xEC,
	0x08, 0x48, 0x8D, 0x35, 0x8E, 0x04, 0x00, 0x00, 0xBF, 0xAB, 0x00, 0x00, 0x00, 0x31, 0xC0, 0x41
};

static_assert(sizeof(kScanParams1Original) == sizeof(kScanParams1Patched), "patch size mismatch");
static_assert(sizeof(kScanParams2Original) == sizeof(kScanParams2Patched), "patch size mismatch");

static mach_vm_address_t orig_cs_validate {};

static void patched_cs_validate_page(vnode_t vp, memory_object_t pager, memory_object_offset_t page_offset,
                                     const void *data, int *validated_p, int *tainted_p, int *nx_p) {
	char path[PATH_MAX];
	int pathlen = PATH_MAX;
	FunctionCast(patched_cs_validate_page, orig_cs_validate)(vp, pager, page_offset, data, validated_p, tainted_p, nx_p);
	if (vn_getpath(vp, path, &pathlen) == 0 && UNLIKELY(strcmp(path, "/usr/sbin/bluetoothd") == 0)) {
		if (KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE,
		                                  kHostBufSizeOriginal, sizeof(kHostBufSizeOriginal),
		                                  kHostBufSizePatched, sizeof(kHostBufSizePatched)))
			SYSLOG(MODULE_SHORT, "patched initCmdHostBufferSizeCB in %s", path);
		if (KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE,
		                                  kScanParams1Original, sizeof(kScanParams1Original),
		                                  kScanParams1Patched, sizeof(kScanParams1Patched)))
			SYSLOG(MODULE_SHORT, "patched LE scan params own address (site 1) in %s", path);
		if (KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE,
		                                  kScanParams2Original, sizeof(kScanParams2Original),
		                                  kScanParams2Patched, sizeof(kScanParams2Patched)))
			SYSLOG(MODULE_SHORT, "patched LE scan params own address (site 2) in %s", path);
	}
}

static void pluginStart() {
	SYSLOG(MODULE_SHORT, "start");
	lilu.onPatcherLoadForce([](void *user, KernelPatcher &patcher) {
		KernelPatcher::RouteRequest csRoute("_cs_validate_page", patched_cs_validate_page, orig_cs_validate);
		if (!patcher.routeMultipleLong(KernelPatcher::KernelID, &csRoute, 1))
			SYSLOG(MODULE_SHORT, "failed to route cs_validate_page");
	});
}

static const char *bootargOff[]   { "-barrotoff" };
static const char *bootargDebug[] { "-barrotdbg" };
static const char *bootargBeta[]  { "-barrotbeta" };

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	LiluAPI::AllowNormal | LiluAPI::AllowSafeMode,
	bootargOff, arrsize(bootargOff),
	bootargDebug, arrsize(bootargDebug),
	bootargBeta, arrsize(bootargBeta),
	KernelVersion::Sequoia,
	KernelVersion::Sequoia,
	pluginStart
};
