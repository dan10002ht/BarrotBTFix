//
//  kmod_info.c
//  BarrotBTFix
//
//  Kernel module descriptor. The bundle identifier must match Info.plist.
//

#include <mach/mach_types.h>

#define BBF_STR_(x) #x
#define BBF_STR(x)  BBF_STR_(x)

extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);
extern kern_return_t BarrotBTFix_kern_start(kmod_info_t *, void *);
extern kern_return_t BarrotBTFix_kern_stop(kmod_info_t *, void *);

__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(io.github.dan10002ht.BarrotBTFix, BBF_STR(MODULE_VERSION), _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = BarrotBTFix_kern_start;
__private_extern__ kmod_stop_func_t *_antimain = BarrotBTFix_kern_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
