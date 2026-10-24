// InfuseSubFinder - dylib tiem vao Infuse (iOS).
// Hook: -[FCMediaFile allExternalTracksOfType:] voi type = 4 (phu de). Truoc khi Infuse gom
// danh sach sub ngoai, ta quet cac nguon, tim sub khop ten va nap vao cache sub cua Infuse
// (cung co che nhu khi nguoi dung "chia se" file sub), nen sub hien trong menu chon sub.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "SFEngine.h"

static const NSUInteger kSFSubtitleTrackType = 4;

static id (*sf_orig_allExternalTracksOfType)(id, SEL, NSUInteger);

static id sf_hooked_allExternalTracksOfType(id self, SEL _cmd, NSUInteger type) {
    if (type == kSFSubtitleTrackType) {
        @try {
            [[SFEngine shared] prepareSubtitlesForMediaFile:self];
        } @catch (NSException *e) {
            SFLog(@"[hook] ngoai le: %@", e);
        }
    }
    return sf_orig_allExternalTracksOfType(self, _cmd, type);
}

static BOOL sf_install_hooks(void) {
    static BOOL installed = NO;
    if (installed) return YES;

    Class cls = NSClassFromString(@"FCMediaFile");
    if (!cls) return NO;
    SEL sel = NSSelectorFromString(@"allExternalTracksOfType:");
    Method m = class_getInstanceMethod(cls, sel);
    if (!m) {
        SFLog(@"[hook] FCMediaFile khong co allExternalTracksOfType: (Infuse doi API?)");
        installed = YES;  // khong thu lai
        return NO;
    }
    sf_orig_allExternalTracksOfType = (id (*)(id, SEL, NSUInteger))method_getImplementation(m);
    method_setImplementation(m, (IMP)sf_hooked_allExternalTracksOfType);
    installed = YES;

    NSString *ver = [NSBundle mainBundle].infoDictionary[@"CFBundleShortVersionString"];
    SFLog(@"[hook] da hook FCMediaFile allExternalTracksOfType: (Infuse %@)", ver);
    return YES;
}

static void sf_app_ready(void) {
    BOOL hooked = sf_install_hooks();
    SFToast(hooked ? @"ISF v0.7: đã nạp, hook OK — đang quét sub..." : @"ISF v0.4: đã nạp nhưng HOOK LỖI (xem log)");
    [[SFEngine shared] startIndexingIfNeeded];
}

__attribute__((constructor)) static void sf_init(void) {
    @autoreleasepool {
        SFLog(@"==== InfuseSubFinder khoi dong ====");
        if (!sf_install_hooks()) {
            // lop chua san sang ngay luc nap dylib -> thu lai khi app chay
            dispatch_async(dispatch_get_main_queue(), ^{ sf_install_hooks(); });
        }
        [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
                                                          object:nil
                                                           queue:nil
                                                      usingBlock:^(NSNotification *n) {
            // cho Infuse nap xong danh sach nguon roi moi quet
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3 * NSEC_PER_SEC)),
                           dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{ sf_app_ready(); });
        }];
    }
}
