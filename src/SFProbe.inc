// ---- SFProbe: tim hieu man hinh "load video" cua Infuse (ghi probe.txt cung thu muc log.txt)
#import <mach-o/dyld.h>
static void SFProbeWrite(NSString *s) {
    NSString *p = [SFSupportDir() stringByAppendingPathComponent:@"probe.txt"];
    NSFileHandle *h = [NSFileHandle fileHandleForWritingAtPath:p];
    if (!h) { [@"" writeToFile:p atomically:YES encoding:NSUTF8StringEncoding error:nil]; h = [NSFileHandle fileHandleForWritingAtPath:p]; }
    if ([h seekToEndOfFile] > 2 * 1024 * 1024) { [h truncateFileAtOffset:0]; }
    [h writeData:[s dataUsingEncoding:NSUTF8StringEncoding]];
    [h closeFile];
}
static NSString *SFProbeVal(id o) {
    if (!o) return @"nil";
    NSString *d = nil;
    @try { d = [o description]; } @catch (...) { d = @"?"; }
    if (d.length > 90) d = [d substringToIndex:90];
    return [NSString stringWithFormat:@"%@ %@", NSStringFromClass([o class]), [d stringByReplacingOccurrencesOfString:@"\n" withString:@" "]];
}
static void SFProbeIvars(id obj, NSString *label, NSMutableString *out) {
    if (!obj) { [out appendFormat:@"## %@ = nil\n", label]; return; }
    [out appendFormat:@"## %@ : %@\n", label, NSStringFromClass([obj class])];
    int depth = 0;
    for (Class c = [obj class]; c && depth < 4; c = class_getSuperclass(c), depth++) {
        NSString *cn = NSStringFromClass(c);
        if ([cn hasPrefix:@"UI"] || [cn hasPrefix:@"NS"] || [cn hasPrefix:@"_"]) break;
        unsigned n = 0; Ivar *iv = class_copyIvarList(c, &n);
        for (unsigned i = 0; i < n && i < 150; i++) {
            const char *nm = ivar_getName(iv[i]), *ty = ivar_getTypeEncoding(iv[i]);
            NSString *val = @"";
            @try {
                if (ty[0] == '@') val = SFProbeVal(object_getIvar(obj, iv[i]));
                else { id v = [obj valueForKey:[NSString stringWithUTF8String:nm]]; val = SFProbeVal(v); }
            } @catch (...) { val = @"(n/a)"; }
            [out appendFormat:@"  %@.%s %s = %@\n", cn, nm, ty, val];
        }
        free(iv);
    }
}
static void SFProbeTree(UIView *v, int depth, NSMutableString *out, int *count) {
    if (depth > 14 || (*count)++ > 500) return;
    NSMutableString *ex = [NSMutableString string];
    if ([v isKindOfClass:[UILabel class]]) [ex appendFormat:@" text=\"%@\"", ((UILabel *)v).text];
    if ([v isKindOfClass:[UIProgressView class]]) [ex appendFormat:@" progress=%.3f", ((UIProgressView *)v).progress];
    if ([v isKindOfClass:[UIActivityIndicatorView class]]) [ex appendFormat:@" animating=%d", ((UIActivityIndicatorView *)v).isAnimating];
    if ([v isKindOfClass:[UIButton class]]) [ex appendFormat:@" title=\"%@\"", [(UIButton *)v titleForState:UIControlStateNormal]];
    for (CALayer *l in v.layer.sublayers) {
        if ([l isKindOfClass:[CAShapeLayer class]]) [ex appendFormat:@" shape(strokeEnd=%.2f)", ((CAShapeLayer *)l).strokeEnd];
    }
    [out appendFormat:@"%*s%@ f=%@ hid=%d a=%.2f id=%@%@\n", depth * 2, "", NSStringFromClass([v class]), NSStringFromCGRect(v.frame), v.hidden, v.alpha, v.accessibilityIdentifier ?: @"-", ex];
    for (UIView *s in v.subviews) SFProbeTree(s, depth + 1, out, count);
}
static UIViewController *SFProbeTopVC(void) {
    UIWindow *w = nil;
    for (UIScene *sc in [UIApplication sharedApplication].connectedScenes)
        if ([sc isKindOfClass:[UIWindowScene class]]) for (UIWindow *x in ((UIWindowScene *)sc).windows) if (x.isKeyWindow) w = x;
    UIViewController *vc = w.rootViewController;
    while (vc.presentedViewController) vc = vc.presentedViewController;
    return vc;
}
static void SFProbeClassesOnce(void) {
    static BOOL done; if (done) return; done = YES;
    NSString *exe = [[NSBundle mainBundle] executablePath];
    unsigned n = 0; const char **names = objc_copyClassNamesForImage(exe.UTF8String, &n);
    NSMutableString *all = [NSMutableString stringWithFormat:@"=== %u classes in %@ ===\n", n, exe.lastPathComponent];
    NSMutableString *meth = [NSMutableString string];
    NSArray *kw = @[ @"buffer", @"spinner", @"loading", @"progress", @"cache", @"demux", @"stream", @"playback", @"preparing", @"connect", @"seek", @"probe", @"waiting", @"stall", @"readahead", @"network" ];
    for (unsigned i = 0; i < n; i++) {
        NSString *cn = [NSString stringWithUTF8String:names[i]];
        [all appendFormat:@"%@\n", cn];
        NSString *l = cn.lowercaseString; BOOL hit = NO;
        for (NSString *k in kw) if ([l containsString:k]) { hit = YES; break; }
        if (!hit) continue;
        Class c = objc_getClass(names[i]);
        [meth appendFormat:@"\n== %@ : %@\n", cn, NSStringFromClass(class_getSuperclass(c))];
        unsigned mc = 0; Method *ms = class_copyMethodList(c, &mc);
        for (unsigned j = 0; j < mc && j < 120; j++) [meth appendFormat:@"  - %@\n", NSStringFromSelector(method_getName(ms[j]))];
        free(ms);
        unsigned ic = 0; Ivar *iv = class_copyIvarList(c, &ic);
        for (unsigned j = 0; j < ic && j < 60; j++) [meth appendFormat:@"  ivar %s %s\n", ivar_getName(iv[j]), ivar_getTypeEncoding(iv[j])];
        free(iv);
    }
    free(names);
    SFProbeWrite(all); SFProbeWrite(meth);
}
static BOOL SFProbeHasPlayerView(UIView *v, int d) {
    if (d > 7) return NO;
    NSString *l = NSStringFromClass([v class]).lowercaseString;
    if ([l containsString:@"video"] || [l containsString:@"player"] || [l containsString:@"playback"]) return YES;
    for (UIView *s in v.subviews) if (SFProbeHasPlayerView(s, d + 1)) return YES;
    return NO;
}
static void SFProbeTick(void) {
    static int ticks; ticks++;
    if (ticks == 10) { SFProbeWrite([NSString stringWithFormat:@"probe khoi dong %@\n", [NSDate date]]); SFProbeClassesOnce(); SFLog(@"[probe] da ghi probe.txt (danh sach lop)"); }
    {
        static NSString *seen; UIViewController *tv = SFProbeTopVC();
        NSString *c0 = tv ? NSStringFromClass([tv class]) : @"";
        if (![c0 isEqualToString:seen]) { seen = c0; SFLog(@"[probe] topVC=%@", c0); }
    }
    static NSString *lastCls; static NSDate *since; static int dumps;
    UIViewController *vc = SFProbeTopVC();
    NSString *cn = vc ? NSStringFromClass([vc class]) : @"";
    NSString *l = cn.lowercaseString;
    BOOL player = [l containsString:@"video"] || [l containsString:@"player"] || [l containsString:@"playback"];
    Class PH = NSClassFromString(@"PlayerHelper");
    id helper = PH && [PH respondsToSelector:NSSelectorFromString(@"currentPlayer")] ? ((id (*)(id, SEL))objc_msgSend)(PH, NSSelectorFromString(@"currentPlayer")) : nil;
    if (!player && !helper) { UIWindow *w0 = vc.view.window; player = w0 && SFProbeHasPlayerView(w0, 0); }
    if (!player && !helper) { lastCls = nil; since = nil; return; }
    if (!since) { since = [NSDate date]; dumps = 0; }
    double el = -[since timeIntervalSinceNow];
    double marks[] = { 0.5, 3, 8, 20 };
    if (dumps < 4 && el >= marks[dumps]) {
        dumps++;
        SFProbeClassesOnce();
        NSMutableString *out = [NSMutableString stringWithFormat:@"\n######## %@ dump #%d t=+%.1fs topVC=%@ ########\n", [NSDate date], dumps, el, cn];
        SFProbeIvars(vc, @"topVC", out);
        id st = SFObj0(vc, @"playbackStrategy"); SFProbeIvars(st, @"playbackStrategy", out);
        SFProbeIvars(helper, @"PlayerHelper", out);
        SFProbeIvars(SFObj0(helper, @"playbackController"), @"playbackController", out);
        SFProbeIvars(SFObj0(helper, @"settingsDataSource"), @"settingsDataSource", out);
        int cnt = 0; [out appendString:@"## view tree\n"];
        UIWindow *w = vc.view.window; if (w) SFProbeTree(w, 0, out, &cnt);
        SFProbeWrite(out);
        SFLog(@"[probe] dump #%d ghi vao probe.txt (topVC=%@)", dumps, cn);
    }
}
void SFInstallProbe(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *t) { @try { SFProbeTick(); } @catch (NSException *e) {} }];
    });
}
