// InfuseSubFinder - dylib tiem vao Infuse (iOS).
// Hook: -[FCMediaFile relatedItems]. Infuse dung danh sach nay (file cung thu muc voi video) de tim sub
// ngoai. Ta them vao do cac sub khop ten o thu muc/nguon khac -> Infuse xu ly nhu sub cung thu muc.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "SFEngine.h"

static NSArray *(*sf_orig_relatedItems)(id, SEL);

static NSArray *sf_hooked_relatedItems(id self, SEL _cmd) {
    NSArray *orig = sf_orig_relatedItems(self, _cmd);
    return [[SFEngine shared] relatedItemsForMediaFile:self original:orig];
}

static BOOL sf_install_hooks(void) {
    static BOOL installed = NO;
    if (installed) return YES;

    Class cls = NSClassFromString(@"FCMediaFile");
    if (!cls) return NO;
    SEL sel = NSSelectorFromString(@"relatedItems");
    Method m = class_getInstanceMethod(cls, sel);
    if (!m) {
        SFLog(@"[hook] FCMediaFile khong co relatedItems (Infuse doi API?)");
        installed = YES;
        return NO;
    }
    sf_orig_relatedItems = (NSArray *(*)(id, SEL))method_getImplementation(m);
    method_setImplementation(m, (IMP)sf_hooked_relatedItems);
    installed = YES;
    NSString *ver = [NSBundle mainBundle].infoDictionary[@"CFBundleShortVersionString"];
    SFLog(@"[hook] da hook FCMediaFile relatedItems (Infuse %@)", ver);
    return YES;
}

static void sf_app_ready(void) {
    static BOOL done = NO;
    @synchronized([SFEngine class]) { if (done) return; done = YES; }   // chi lam mot lan moi lan mo app
    BOOL hooked = sf_install_hooks();
    if (!hooked) SFToast(@"ISF: HOOK LỖI (xem log)");
    SFInstallSettingsHook();
    SFInstallUI();
    [[SFEngine shared] startIndexingIfNeeded];
    [[SFEngine shared] startLocalWatcher];
    // che do "moi gio": kiem tra dinh ky
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSTimer scheduledTimerWithTimeInterval:600 repeats:YES block:^(NSTimer *tm) { [[SFEngine shared] startIndexingIfNeeded]; }];
    });
}

__attribute__((constructor)) static void sf_init(void) {
    @autoreleasepool {
        SFLog(@"==== InfuseSubFinder khoi dong ====");
        // hook man Cai dat ngay luc nap (nhu InfusePlus) de dong "Tim sub" co san khi man hinh dung len
        SFInstallChapters();
        SFInstallProbe();
        if (!SFInstallSettingsHook()) dispatch_async(dispatch_get_main_queue(), ^{ SFInstallSettingsHook(); });
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
