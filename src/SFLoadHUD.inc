// ---- Man hinh tai video chi tiet: thay vong xoay cua Infuse bang bang trang thai (nguon, MB da tai, toc do, giai doan)
#import <stdatomic.h>

static BOOL SFLoadHUDOn(void) {
    id v = [[NSUserDefaults standardUserDefaults] objectForKey:@"sf.loadhud"];
    return v ? [v boolValue] : YES;
}

#define SF_LS_MAX 16
static struct { char name[40]; _Atomic unsigned long long bytes; _Atomic unsigned long long reads; } sf_ls[SF_LS_MAX];
static int sf_ls_n = 0;
static BOOL sf_hudHooked = NO;

static void SFHookRead(NSString *cn) {
    Class c = NSClassFromString(cn); if (!c || sf_ls_n >= SF_LS_MAX) return;
    SEL s = NSSelectorFromString(@"readBuffer:ofSize:");
    Method m = class_getInstanceMethod(c, s); if (!m) return;
    Class sc = class_getSuperclass(c);
    Method sm = sc ? class_getInstanceMethod(sc, s) : NULL;
    if (sm && method_getImplementation(sm) == method_getImplementation(m)) return;   // ke thua -> bo qua
    char *rt = method_copyReturnType(m), *a3 = method_copyArgumentType(m, 3);
    int rk = (rt[0] == 'i' || rt[0] == 'I') ? 1 : ((rt[0] == 'q' || rt[0] == 'Q' || rt[0] == 'l' || rt[0] == 'L') ? 2 : 0);
    int ak = (a3 && (a3[0] == 'i' || a3[0] == 'I')) ? 1 : ((a3 && (a3[0] == 'q' || a3[0] == 'Q' || a3[0] == 'l' || a3[0] == 'L')) ? 2 : 0);
    SFLog(@"[load] %@ readBuffer:ofSize: enc=%s ret=%s arg3=%s", cn, method_getTypeEncoding(m), rt, a3);
    free(rt); free(a3);
    IMP orig = method_getImplementation(m);
    int slot = sf_ls_n;
    IMP n = NULL;
    if (rk == 1 && ak == 1) n = imp_implementationWithBlock(^int(id self, void *buf, int sz) {
        int r = ((int (*)(id, SEL, void *, int))orig)(self, s, buf, sz);
        if (r > 0) { atomic_fetch_add(&sf_ls[slot].bytes, (unsigned long long)r); atomic_fetch_add(&sf_ls[slot].reads, 1); }
        return r; });
    else if (rk == 1 && ak == 2) n = imp_implementationWithBlock(^int(id self, void *buf, long long sz) {
        int r = ((int (*)(id, SEL, void *, long long))orig)(self, s, buf, sz);
        if (r > 0) { atomic_fetch_add(&sf_ls[slot].bytes, (unsigned long long)r); atomic_fetch_add(&sf_ls[slot].reads, 1); }
        return r; });
    else if (rk == 2 && ak == 1) n = imp_implementationWithBlock(^long long(id self, void *buf, int sz) {
        long long r = ((long long (*)(id, SEL, void *, int))orig)(self, s, buf, sz);
        if (r > 0) { atomic_fetch_add(&sf_ls[slot].bytes, (unsigned long long)r); atomic_fetch_add(&sf_ls[slot].reads, 1); }
        return r; });
    else if (rk == 2 && ak == 2) n = imp_implementationWithBlock(^long long(id self, void *buf, long long sz) {
        long long r = ((long long (*)(id, SEL, void *, long long))orig)(self, s, buf, sz);
        if (r > 0) { atomic_fetch_add(&sf_ls[slot].bytes, (unsigned long long)r); atomic_fetch_add(&sf_ls[slot].reads, 1); }
        return r; });
    if (!n) { SFLog(@"[load] bo qua %@ (kieu tham so la)", cn); return; }
    strlcpy(sf_ls[slot].name, cn.UTF8String, sizeof(sf_ls[slot].name));
    method_setImplementation(m, n);
    sf_ls_n++;
}

static void SFInstallLoadHooks(void) {
    if (sf_hudHooked) return; sf_hudHooked = YES;
    for (NSString *cn in @[ @"FCInputStreamSpeedMeter", @"FCHTTPInputStream", @"FCMultipleHTTPInputStream", @"FCCloudInputStream",
                            @"FCNQSMBInputStream", @"FCSLSMBInputStream", @"FCNFSInputStream", @"infuse.ReadAheadInputStream",
                            @"FCPlexInputStream", @"infuse.EmbyInputStream", @"FCMegaNzInputStream", @"FCBookmarkInputStream",
                            @"FCHDInputStream", @"FCSandiskInputStream", @"FCUDFInputStream", @"FCISOInputStream" ])
        SFHookRead(cn);
    SFLog(@"[load] da hook %d lop input stream", sf_ls_n);
}

static NSString *SFSourceLabel(const char *n) {
    NSString *s = [NSString stringWithUTF8String:n];
    if ([s containsString:@"SMB"]) return @"SMB";
    if ([s containsString:@"NFS"]) return @"NFS";
    if ([s containsString:@"HTTP"]) return @"HTTP/WebDAV";
    if ([s containsString:@"Cloud"]) return @"Cloud";
    if ([s containsString:@"Plex"]) return @"Plex";
    if ([s containsString:@"Emby"]) return @"Emby/Jellyfin";
    if ([s containsString:@"Mega"]) return @"MEGA";
    if ([s containsString:@"HD"] || [s containsString:@"Sandisk"]) return @"Ổ đĩa";
    if ([s containsString:@"ReadAhead"] || [s containsString:@"SpeedMeter"]) return @"Mạng/nguồn";
    return s;
}

static UIView *SFFindClass(UIView *v, NSString *cn, int d) {
    if (d > 10) return nil;
    if ([NSStringFromClass([v class]) isEqualToString:cn]) return v;
    for (UIView *s in v.subviews) { UIView *r = SFFindClass(s, cn, d + 1); if (r) return r; }
    return nil;
}
static BOOL SFEffectivelyVisible(UIView *v) {
    for (UIView *x = v; x; x = x.superview) { if (x.hidden || x.alpha < 0.05) return NO; }
    return v.window != nil;
}
static NSString *SFMB(unsigned long long b) {
    if (b >= 1024ULL * 1024 * 1024) return [NSString stringWithFormat:@"%.2f GB", b / 1073741824.0];
    return [NSString stringWithFormat:@"%.1f MB", b / 1048576.0];
}

@interface SFLoadHUDView : UIVisualEffectView
@property (nonatomic, strong) UILabel *l1, *l2, *l3;
@end
@implementation SFLoadHUDView
- (instancetype)init {
    if ((self = [super initWithEffect:nil])) {
        self.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.78];
        self.layer.cornerRadius = 14; self.clipsToBounds = YES; self.userInteractionEnabled = NO;
        _l1 = [UILabel new]; _l1.font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold]; _l1.textColor = UIColor.whiteColor;
        _l2 = [UILabel new]; _l2.font = [UIFont monospacedDigitSystemFontOfSize:13 weight:UIFontWeightRegular]; _l2.textColor = [UIColor colorWithWhite:1 alpha:0.85];
        _l3 = [UILabel new]; _l3.font = [UIFont systemFontOfSize:12]; _l3.textColor = [UIColor colorWithWhite:1 alpha:0.6]; _l3.numberOfLines = 2;
        for (UILabel *l in @[ _l1, _l2, _l3 ]) l.textAlignment = NSTextAlignmentCenter;
        UIStackView *st = [[UIStackView alloc] initWithArrangedSubviews:@[ _l1, _l2, _l3 ]];
        st.axis = UILayoutConstraintAxisVertical; st.spacing = 3; st.translatesAutoresizingMaskIntoConstraints = NO;
        [self.contentView addSubview:st];
        [NSLayoutConstraint activateConstraints:@[
            [st.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:10],
            [st.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-10],
            [st.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:14],
            [st.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-14] ]];
    }
    return self;
}
@end

// ---- bang theo doi lien tuc (keo di chuyen, phong to/thu nho), bat bang nut ben trai (i) trong player
static BOOL SFMonShown(void) { return [[NSUserDefaults standardUserDefaults] boolForKey:@"sf.monshow"]; }
static void SFMonToggle(void) {
    NSUserDefaults *u = [NSUserDefaults standardUserDefaults];
    [u setBool:![u boolForKey:@"sf.monshow"] forKey:@"sf.monshow"];
}
static CGFloat SFMonScale(void) { double v = [[NSUserDefaults standardUserDefaults] doubleForKey:@"sf.monscale"]; return v > 0 ? (CGFloat)v : 1.0; }

@interface SFMonView : UIVisualEffectView
@property (nonatomic, strong) UILabel *title, *body;
@property (nonatomic, assign) CGPoint startCenter;
@end
@implementation SFMonView
- (instancetype)init {
    if ((self = [super initWithEffect:nil])) {
        self.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.78];
        self.layer.cornerRadius = 12; self.clipsToBounds = YES;
        _title = [UILabel new]; _title.text = @"Theo dõi phát"; _title.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold]; _title.textColor = UIColor.whiteColor;
        _body = [UILabel new]; _body.numberOfLines = 0;
        _body.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightRegular]; _body.textColor = [UIColor colorWithWhite:1 alpha:0.9];
        UIButton *(^mk)(NSString *, SEL) = ^UIButton *(NSString *t, SEL sel) {
            UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
            [b setTitle:t forState:UIControlStateNormal]; b.titleLabel.font = [UIFont systemFontOfSize:18 weight:UIFontWeightBold];
            b.tintColor = UIColor.whiteColor; [b addTarget:self action:sel forControlEvents:UIControlEventTouchUpInside];
            [b.widthAnchor constraintEqualToConstant:30].active = YES; return b; };
        UIStackView *hdr = [[UIStackView alloc] initWithArrangedSubviews:@[ _title, [UIView new], mk(@"−", @selector(smaller)), mk(@"+", @selector(bigger)), mk(@"✕", @selector(closeMe)) ]];
        hdr.axis = UILayoutConstraintAxisHorizontal; hdr.alignment = UIStackViewAlignmentCenter;
        UIStackView *st = [[UIStackView alloc] initWithArrangedSubviews:@[ hdr, _body ]];
        st.axis = UILayoutConstraintAxisVertical; st.spacing = 4; st.translatesAutoresizingMaskIntoConstraints = NO;
        [self.contentView addSubview:st];
        [NSLayoutConstraint activateConstraints:@[
            [st.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:6],
            [st.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-10],
            [st.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:12],
            [st.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-6],
            [st.widthAnchor constraintEqualToConstant:230] ]];
        [self addGestureRecognizer:[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(pan:)]];
        [self addGestureRecognizer:[[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(pinch:)]];
    }
    return self;
}
- (void)applyScale:(CGFloat)k {
    k = MAX(0.6, MIN(2.2, k));
    [[NSUserDefaults standardUserDefaults] setDouble:k forKey:@"sf.monscale"];
    self.transform = CGAffineTransformMakeScale(k, k);
}
- (void)smaller { [self applyScale:SFMonScale() - 0.15]; }
- (void)bigger { [self applyScale:SFMonScale() + 0.15]; }
- (void)closeMe { [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"sf.monshow"]; [self removeFromSuperview]; }
- (void)pan:(UIPanGestureRecognizer *)g {
    UIView *host = self.superview; if (!host) return;
    CGPoint t = [g translationInView:host];
    if (g.state == UIGestureRecognizerStateBegan) _startCenter = self.center;
    CGPoint c = CGPointMake(_startCenter.x + t.x, _startCenter.y + t.y);
    c.x = MAX(20, MIN(host.bounds.size.width - 20, c.x)); c.y = MAX(20, MIN(host.bounds.size.height - 20, c.y));
    self.center = c;
    if (g.state == UIGestureRecognizerStateEnded) {
        NSUserDefaults *u = [NSUserDefaults standardUserDefaults];
        [u setDouble:c.x / host.bounds.size.width forKey:@"sf.monx"]; [u setDouble:c.y / host.bounds.size.height forKey:@"sf.mony"];
    }
}
- (void)pinch:(UIPinchGestureRecognizer *)g {
    static CGFloat base;
    if (g.state == UIGestureRecognizerStateBegan) base = SFMonScale();
    [self applyScale:base * g.scale];
}
@end

static NSString *SFTimeStr(double t) {
    if (!(t >= 0)) t = 0; long s = (long)t;
    return s >= 3600 ? [NSString stringWithFormat:@"%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60] : [NSString stringWithFormat:@"%02ld:%02ld", s / 60, s % 60];
}
static BOOL SFAnimating(CALayer *l, int d) {
    if (l.animationKeys.count) return YES;
    if (d > 3) return NO;
    for (CALayer *s in l.sublayers) if (SFAnimating(s, d + 1)) return YES;
    return NO;
}



static id SFFindObjOfClass(NSString *cn, NSArray *roots) {
    Class target = NSClassFromString(cn); if (!target) return nil;
    NSMutableSet *seen = [NSMutableSet set];
    NSMutableArray *q = [NSMutableArray array];
    for (id r in roots) if (r && r != (id)NSNull.null) [q addObject:@[ r, @0 ]];
    int guard = 0;
    while (q.count && guard++ < 400) {
        NSArray *e = q.firstObject; [q removeObjectAtIndex:0];
        id o = e[0]; int d = [e[1] intValue];
        NSValue *k = [NSValue valueWithPointer:(__bridge void *)o];
        if ([seen containsObject:k]) continue; [seen addObject:k];
        if ([o isKindOfClass:target]) return o;
        if (d >= 3) continue;
        NSString *own = NSStringFromClass([o class]);
        if ([o isKindOfClass:[UIView class]] || [own hasPrefix:@"NS"] || [own hasPrefix:@"__NS"] || [own hasPrefix:@"UI"]) {
            if ([o isKindOfClass:[NSArray class]]) { int n = 0; for (id x in (NSArray *)o) { if (n++ > 40) break; [q addObject:@[ x, @(d + 1) ]]; } }
            continue;
        }
        for (Class c = [o class]; c && ![NSStringFromClass(c) hasPrefix:@"NS"]; c = class_getSuperclass(c)) {
            unsigned n = 0; Ivar *iv = class_copyIvarList(c, &n);
            for (unsigned i = 0; i < n; i++) {
                const char *ty = ivar_getTypeEncoding(iv[i]);
                if (!ty || ty[0] != '@') continue;
                id v = nil; @try { v = object_getIvar(o, iv[i]); } @catch (...) {}
                if (v) [q addObject:@[ v, @(d + 1) ]];
            }
            free(iv);
        }
    }
    return nil;
}


// ---- dem khung hinh hien thi that (AVSampleBufferDisplayLayer / CAMetalLayer) -> FPS song
#define SF_FL_MAX 6
static struct { _Atomic uintptr_t layer; _Atomic unsigned long long n; } sf_fl[SF_FL_MAX];
static _Atomic int sf_fl_n = 0;
static void SFCountFrame(id layer) {
    uintptr_t p = (uintptr_t)(__bridge void *)layer;
    int cnt = atomic_load(&sf_fl_n); if (cnt > SF_FL_MAX) cnt = SF_FL_MAX;
    for (int i = 0; i < cnt; i++) if (atomic_load(&sf_fl[i].layer) == p) { atomic_fetch_add(&sf_fl[i].n, 1); return; }
    int i = atomic_fetch_add(&sf_fl_n, 1);
    if (i < SF_FL_MAX) { atomic_store(&sf_fl[i].n, 1); atomic_store(&sf_fl[i].layer, p); }
}
static BOOL sf_frameHooked = NO;
static void SFInstallFrameHooks(void) {
    if (sf_frameHooked) return; sf_frameHooked = YES;
    Class a = NSClassFromString(@"AVSampleBufferDisplayLayer");
    Method ma = a ? class_getInstanceMethod(a, NSSelectorFromString(@"enqueueSampleBuffer:")) : NULL;
    if (ma) {
        IMP o = method_getImplementation(ma); SEL sel = NSSelectorFromString(@"enqueueSampleBuffer:");
        method_setImplementation(ma, imp_implementationWithBlock(^void(id self, void *sb) { SFCountFrame(self); ((void (*)(id, SEL, void *))o)(self, sel, sb); }));
    }
    Class m = NSClassFromString(@"CAMetalLayer");
    Method mm = m ? class_getInstanceMethod(m, NSSelectorFromString(@"nextDrawable")) : NULL;
    if (mm) {
        IMP o = method_getImplementation(mm); SEL sel = NSSelectorFromString(@"nextDrawable");
        method_setImplementation(mm, imp_implementationWithBlock(^id(id self) { SFCountFrame(self); return ((id (*)(id, SEL))o)(self, sel); }));
    }
    SFLog(@"[fps] hook AVSampleBufferDisplayLayer=%d CAMetalLayer=%d", ma != NULL, mm != NULL);
}

// ---- thong ke cuoi phien cua Infuse (FCVVCPlaybackStatisticsTracker)
static _Atomic int sf_liveFull = 0, sf_liveSoft = 0;
static NSString *sf_lastEngine; static double sf_lastAvg;
static void (*sf_orig_finish)(id, SEL, id);
static void (*sf_orig_markfull)(id, SEL);
static void (*sf_orig_markfb)(id, SEL);
static void (*sf_orig_initstats)(id, SEL);
static void sf_hooked_markfull(id self, SEL c) { atomic_fetch_add(&sf_liveFull, 1); sf_orig_markfull(self, c); }
static void sf_hooked_markfb(id self, SEL c) { atomic_store(&sf_liveSoft, 1); sf_orig_markfb(self, c); }
static void sf_hooked_initstats(id self, SEL c) { atomic_store(&sf_liveFull, 0); atomic_store(&sf_liveSoft, 0); sf_orig_initstats(self, c); }
static void sf_hooked_finish(id self, SEL c, id player) {
    sf_orig_finish(self, c, player);
    @try {
        id st = nil;
        for (NSString *k in @[ @"accumulatedStatistics", @"playbackStatistics" ]) {
            id x = SFObj0(self, k); if (!x) continue;
            if (!st) st = x;
            if ([[x valueForKey:@"playbackTime"] doubleValue] > 0) { st = x; break; }
        }
        if (!st) { SFLog(@"[report] khong doc duoc thong ke cuoi phien"); return; }
        double pt = [[st valueForKey:@"playbackTime"] doubleValue], fps = [[st valueForKey:@"fpsRate"] doubleValue];
        int fb = [[st valueForKey:@"fullBufferings"] intValue]; BOOL soft = [[st valueForKey:@"fallbackedToSoftware"] boolValue];
        NSString *types = [[[st valueForKey:@"usedPlayerTypes"] allObjects] componentsJoinedByString:@","] ?: @"";
        id iss = SFObj0(st, @"inputStreamStats"); double mbps = 0;
        if (iss) { @try { mbps = [[iss valueForKey:@"averageReadingSpeedMbps"] doubleValue]; } @catch (...) {} }
        NSString *r = [NSString stringWithFormat:@"%.0fs · kịp giờ %.0f%% · %@ · đệm đầy %d · nguồn %.2f MB/s", pt, fps * 100.0, soft ? @"giải mã phần mềm" : (sf_lastEngine.length ? sf_lastEngine : types), MAX(fb, atomic_load(&sf_liveFull)), sf_lastAvg / 1048576.0];
        SFLog(@"[report] %@ | %@", r, st);
        [[NSUserDefaults standardUserDefaults] setObject:r forKey:@"sf.lastreport"];
        if (pt > 5 && [[NSUserDefaults standardUserDefaults] boolForKey:@"sf.monbtn"])
            dispatch_async(dispatch_get_main_queue(), ^{ SFToast([@"Phiên vừa xem: " stringByAppendingString:r]); });
    } @catch (NSException *e) { SFLog(@"[report] loi: %@", e); }
}
static BOOL sf_statsHooked = NO;
static void SFInstallStatsHooks(void) {
    if (sf_statsHooked) return;
    Class c = NSClassFromString(@"FCVVCPlaybackStatisticsTracker"); if (!c) return;
    sf_statsHooked = YES;
    Method m1 = class_getInstanceMethod(c, NSSelectorFromString(@"finishTrackingPlaybackSessionWithPlayer:"));
    Method m2 = class_getInstanceMethod(c, NSSelectorFromString(@"markFullBuffering"));
    Method m3 = class_getInstanceMethod(c, NSSelectorFromString(@"markFallback"));
    Method m4 = class_getInstanceMethod(c, NSSelectorFromString(@"initializeStatisticsForNewMovie"));
    if (m1) { sf_orig_finish = (void (*)(id, SEL, id))method_getImplementation(m1); method_setImplementation(m1, (IMP)sf_hooked_finish); }
    if (m2) { sf_orig_markfull = (void (*)(id, SEL))method_getImplementation(m2); method_setImplementation(m2, (IMP)sf_hooked_markfull); }
    if (m3) { sf_orig_markfb = (void (*)(id, SEL))method_getImplementation(m3); method_setImplementation(m3, (IMP)sf_hooked_markfb); }
    if (m4) { sf_orig_initstats = (void (*)(id, SEL))method_getImplementation(m4); method_setImplementation(m4, (IMP)sf_hooked_initstats); }
    SFLog(@"[report] hook tracker finish=%d full=%d fallback=%d init=%d", m1 != NULL, m2 != NULL, m3 != NULL, m4 != NULL);
}

static void SFHideRewind(UIView *root) {
    static NSArray *labels;
    if (!labels) {
        NSMutableArray *a = [NSMutableArray array];
        for (NSString *k in @[ @"pcVORewindBack", @"pcVORewindForward" ]) {
            NSString *v = [[NSBundle mainBundle] localizedStringForKey:k value:@"" table:nil];
            if (v.length) [a addObject:v];
        }
        labels = a;
    }
    if (!labels.count) return;
    NSMutableArray *stack = [NSMutableArray arrayWithObject:root];
    int guard = 0;
    while (stack.count && guard++ < 600) {
        UIView *v = stack.lastObject; [stack removeLastObject];
        if ([v isKindOfClass:[UIButton class]] && v.accessibilityLabel && [labels containsObject:v.accessibilityLabel]) {
            if (!v.hidden) v.hidden = YES;
            continue;
        }
        [stack addObjectsFromArray:v.subviews];
    }
}

// ---- chan "giu de tang toc 2x" goc cua Infuse khi InfusePlus 'longTapToSpeed' dang tat
static void (*sf_orig_setrate)(id, SEL, float);
static float sf_normalRate = 1.0f;
static ptrdiff_t sf_2xOff = -1;
static BOOL SFIs2x(id pc) {
    if (sf_2xOff < 0) return NO;
    return *((uint8_t *)(__bridge void *)pc + sf_2xOff) != 0;
}
static void sf_hooked_setrate(id self, SEL _cmd, float r) {
    BOOL no2x = [[NSUserDefaults standardUserDefaults] boolForKey:@"sf.no2x"];
    if (r != 1.0f) { NSArray *cs = [NSThread callStackSymbols]; SFLog(@"[2x] setPlaybackRate:%.2f flag=%d no2x=%d off=%ld stack=%@", r, SFIs2x(self), no2x, (long)sf_2xOff, [[cs subarrayWithRange:NSMakeRange(1, MIN((NSUInteger)6, cs.count - 1))] componentsJoinedByString:@" | "]); }
    if (no2x && r == 2.0f && SFIs2x(self)) { SFLog(@"[2x] chan (co 2x truoc khi dat toc do), giu %.2fx", sf_normalRate); return; }
    if (!SFIs2x(self) && r != 2.0f) sf_normalRate = r;
    sf_orig_setrate(self, _cmd, r);
    if (!no2x || r != 2.0f) return;
    __weak id wself = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        id s = wself; if (!s) return;
        if (SFIs2x(s)) { SFLog(@"[2x] chan tang toc giu man hinh, tra ve %.2fx", sf_normalRate); sf_orig_setrate(s, _cmd, sf_normalRate); }
    });
}
static BOOL sf_speedHooked = NO;
static void SFInstallSpeedFix(void) {
    if (sf_speedHooked) return;
    Class c = NSClassFromString(@"infuse.PlaybackController"); if (!c) c = NSClassFromString(@"_TtC6infuse18PlaybackController");
    if (!c) return;
    Method m = class_getInstanceMethod(c, NSSelectorFromString(@"setPlaybackRate:"));
    Ivar iv = class_getInstanceVariable(c, "isSpeed2xMode");
    if (!m) return;
    char *a2 = method_copyArgumentType(m, 2);
    SFLog(@"[2x] setPlaybackRate: enc=%s arg=%s ivar=%d off=%ld", method_getTypeEncoding(m), a2, iv != NULL, iv ? (long)ivar_getOffset(iv) : -1L);
    BOOL isFloat = a2 && a2[0] == 'f'; free(a2);
    if (!iv || !isFloat) { sf_speedHooked = YES; return; }
    sf_2xOff = ivar_getOffset(iv);
    sf_orig_setrate = (void (*)(id, SEL, float))method_getImplementation(m);
    method_setImplementation(m, (IMP)sf_hooked_setrate);
    sf_speedHooked = YES;
}

static void SFLoadHUDTick(void) {
    static __weak id curVC; static NSDate *vcStart, *visStart; static BOOL wasVisible;
    static unsigned long long lastBytes[SF_LS_MAX]; static NSDate *lastT; static double speed; static int best = -1;
    static __weak UIView *spinner; static SFLoadHUDView *hud; static SFMonView *mon; static int tickN; static NSDate *lastCacheLog; static int cacheLogs;
    static double lastCT = -1; static NSDate *lastAdvance; static int stalls; static BOOL wasStall; static int lastSig = -1;
    static NSDate *stopSince, *cacheFix, *cacheDraw; static __weak UIView *cacheLine; static __weak id curItem; static id vdesc; static NSDate *descTried; static unsigned long long lastFrames[SF_FL_MAX]; static double liveFPS; static NSDate *lastFT;
    tickN++;
    UIViewController *vc = SFProbeTopVC();
    if (![NSStringFromClass([vc class]) isEqualToString:@"FCVideoViewController"]) {
        if (hud.superview) [hud removeFromSuperview];
        if (mon.superview) [mon removeFromSuperview];
        curVC = nil; wasVisible = NO; return;
    }
    Class PH0 = NSClassFromString(@"PlayerHelper");
    id helper0 = PH0 ? ((id (*)(id, SEL))objc_msgSend)(PH0, NSSelectorFromString(@"currentPlayer")) : nil;
    id item0 = helper0 ? SFObj0(helper0, @"mediaItem") : nil;
    BOOL newSession = NO;
    if (vc != curVC) { newSession = YES; curVC = vc; curItem = item0; }
    else if (item0 && curItem && item0 != curItem) { newSession = YES; curItem = item0; SFLog(@"[load] doi tap trong player -> reset bo dem"); }
    else if (item0 && !curItem) curItem = item0;
    if (newSession) {                                   // phien phat moi / doi tap
        vcStart = [NSDate date]; visStart = nil; wasVisible = NO; speed = 0; best = -1; lastT = nil;
        lastCT = -1; lastAdvance = nil; stalls = 0; wasStall = NO; lastSig = -1; vdesc = nil; descTried = nil; stopSince = nil;
        atomic_store(&sf_fl_n, 0); atomic_store(&sf_liveFull, 0); atomic_store(&sf_liveSoft, 0); memset(lastFrames, 0, sizeof(lastFrames));
        for (int i = 0; i < sf_ls_n; i++) { atomic_store(&sf_ls[i].bytes, 0); atomic_store(&sf_ls[i].reads, 0); lastBytes[i] = 0; }
        spinner = nil;
    }
    UIView *panelV = vc.view;
    if (!spinner) spinner = SFFindClass(panelV, @"infuse.VideoControlPanelActivityView", 0);

    // ---- so lieu dung chung
    NSDate *now = [NSDate date];
    double dt = lastT ? [now timeIntervalSinceDate:lastT] : 0;
    int top = -1; unsigned long long topB = 0;
    for (int i = 0; i < sf_ls_n; i++) { unsigned long long b = atomic_load(&sf_ls[i].bytes); if (b > topB) { topB = b; top = i; } }
    for (int i = 0; i < sf_ls_n; i++) {   // uu tien lop cu the (HTTP/SMB/...) neu so byte gan bang lop tong
        unsigned long long b = atomic_load(&sf_ls[i].bytes);
        if (top >= 0 && i != top && b * 10 >= topB * 9 && ![SFSourceLabel(sf_ls[i].name) isEqualToString:@"Mạng/nguồn"]) { top = i; topB = b; break; }
    }
    if (top >= 0 && dt > 0.05) {
        double inst = (topB - lastBytes[top]) / dt;
        if (top != best) { best = top; speed = inst; } else speed = speed * 0.6 + inst * 0.4;
    }
    for (int i = 0; i < sf_ls_n; i++) lastBytes[i] = atomic_load(&sf_ls[i].bytes);
    lastT = now;

    {   // FPS hien thi thuc te: lop co toc do khung hinh cao nhat
        double ft = lastFT ? [now timeIntervalSinceDate:lastFT] : 0;
        int cnt = MIN(atomic_load(&sf_fl_n), SF_FL_MAX); double best2 = 0;
        for (int i = 0; i < cnt; i++) {
            unsigned long long n = atomic_load(&sf_fl[i].n);
            if (ft > 0.2) { double r = (n >= lastFrames[i] ? (n - lastFrames[i]) : 0) / ft; if (r > best2) best2 = r; }
            lastFrames[i] = n;
        }
        if (ft > 0.2) { liveFPS = liveFPS * 0.5 + best2 * 0.5; lastFT = now; } else if (!lastFT) lastFT = now;
    }
    BOOL opened = [[vc valueForKey:@"_playerOpeningSucceeded"] boolValue];
    id player = SFObj0(vc, @"player");
    Class PH = NSClassFromString(@"PlayerHelper");
    id helper = PH ? ((id (*)(id, SEL))objc_msgSend)(PH, NSSelectorFromString(@"currentPlayer")) : nil;
    double ct = helper ? SFD(helper, @"currentTime") : 0, dur = helper ? SFD(helper, @"duration") : 0;
    if (lastCT >= 0 && fabs(ct - lastCT) > 0.02) lastAdvance = now;
    lastCT = ct;
    BOOL playing = opened && lastAdvance && -[lastAdvance timeIntervalSinceNow] < 1.2;
    BOOL anim = spinner ? SFAnimating(spinner.layer, 0) : NO;
    BOOL viewVis = spinner ? SFEffectivelyVisible(spinner) : NO;
    int sig = (viewVis ? 1 : 0) | (anim ? 2 : 0) | (playing ? 4 : 0) | (opened ? 8 : 0);
    if (sig != lastSig) { lastSig = sig; SFLog(@"[load] tin hieu: viewVis=%d anim=%d playing=%d opened=%d t=%.1f", viewVis, anim, playing, opened, ct); }
    BOOL buffering = viewVis && !playing && (opened ? anim : YES);
    if (opened && buffering && !wasStall) stalls++;
    wasStall = opened && buffering;

    double elapsed = -[vcStart timeIntervalSinceNow];
    NSString *srcName = top >= 0 ? SFSourceLabel(sf_ls[top].name) : @"—";


    // ---- thong tin video/giai ma (FCVideoStreamDescriptor) tim trong do thi doi tuong cua player
    static int descTries; if (newSession) descTries = 0;
    if (!vdesc && opened && descTries < 4 && (!descTried || -[descTried timeIntervalSinceNow] > 3)) {
        descTried = now; descTries++;
        vdesc = SFFindObjOfClass(@"FCVideoStreamDescriptor", @[ vc, SFObj0(vc, @"player") ?: (id)NSNull.null, SFObj0(vc, @"playbackStrategy") ?: (id)NSNull.null ]);
        if (vdesc) SFLog(@"[load] tim thay FCVideoStreamDescriptor: %@", [[vdesc description] substringToIndex:MIN((NSUInteger)200, [[vdesc description] length])]);
    }
    NSString *vinfo = @"—", *dec = @"—";
    if (vdesc) {
        @try {
            int w = [[vdesc valueForKey:@"width"] intValue], h = [[vdesc valueForKey:@"height"] intValue];
            double f = [[vdesc valueForKey:@"fps"] doubleValue];
            BOOL hw = [[vdesc valueForKey:@"shouldUseHWDecoding"] boolValue], slow = [[vdesc valueForKey:@"usesSlowSWCodec"] boolValue];
            vinfo = [NSString stringWithFormat:@"%dx%d %.2ffps", w, h, f];
            dec = hw ? @"Phần cứng" : (slow ? @"Phần mềm (codec nặng)" : @"Phần mềm");
        } @catch (...) {}
    }
    id playerObj = SFObj0(vc, @"player");
    NSString *engine = playerObj ? NSStringFromClass([playerObj class]) : @"—";
    sf_lastEngine = engine; if (elapsed > 1 && topB > 0) sf_lastAvg = topB / elapsed;

    // ---- thong ke giai ma cua Infuse (FCPlaybackStatistics)
    id tracker = SFObj0(vc, @"_playbackStatisticsTracker"); if (!tracker) tracker = [vc valueForKey:@"_playbackStatisticsTracker"];
    id pst = tracker ? SFObj0(tracker, @"playbackStatistics") : nil;
    double fps = 0; BOOL soft = NO; int fullBuf = 0; NSString *ptypes = @"";
    if (pst) {
        @try { fps = [[pst valueForKey:@"fpsRate"] doubleValue]; soft = [[pst valueForKey:@"fallbackedToSoftware"] boolValue];
               fullBuf = [[pst valueForKey:@"fullBufferings"] intValue]; ptypes = [[[pst valueForKey:@"usedPlayerTypes"] allObjects] componentsJoinedByString:@","] ?: @""; } @catch (...) {}
    }
    fullBuf = MAX(fullBuf, atomic_load(&sf_liveFull)); if (atomic_load(&sf_liveSoft)) soft = YES;
    // ---- dung hinh khong co vong xoay (khong phai cho mang) -> ghi log nguyen nhan
    if (opened && !playing && !buffering) { if (!stopSince) stopSince = now; }
    else if (stopSince) {
        double d = -[stopSince timeIntervalSinceNow]; stopSince = nil;
        if (d > 0.8 && d < 30 && playing) {
            NSProcessInfo *pi0 = [NSProcessInfo processInfo];
            SFLog(@"[stall] dung %.1fs tai t=%.1f (khong co vong xoay) toc do=%.2fMB/s nhiet=%ld lowpower=%d fps=%.1f softFallback=%d player=%@ fullBuf=%d fpsLive=%.1f", d, ct, speed / 1048576.0, (long)pi0.thermalState, pi0.lowPowerModeEnabled, fps, soft, ptypes, fullBuf, liveFPS);
        }
    }

    // ---- thanh cache (vach trang mo tren thanh tien trinh): giu cho no luon cap nhat
    if (!cacheLine) cacheLine = SFFindClass(panelV, @"FCProgressBarCacheLineView", 0);
    if (cacheLine && opened) {
        UIView *cl = cacheLine;
        BOOL ctrlVisible = SFEffectivelyVisible(cl) || (cl.window && cl.superview && !cl.hidden);
        BOOL en = [[cl valueForKey:@"cacheUpdatingEnabled"] boolValue];
        id timer = SFObj0(cl, @"cacheUpdatingTimer");
        NSArray *rg = SFObj0(cl, @"cachedRanges");
        BOOL needFix = (!en || !timer) || (rg.count == 0 && topB > 0 && elapsed > 5);
        if (ctrlVisible && needFix && (!cacheFix || -[cacheFix timeIntervalSinceNow] > 3)) {
            cacheFix = now;
            SFLog(@"[cache] khoi dong lai cap nhat (en=%d timer=%d ranges=%lu)", en, timer != nil, (unsigned long)rg.count);
            ((void (*)(id, SEL, BOOL))objc_msgSend)(cl, NSSelectorFromString(@"setCacheUpdatingEnabled:"), NO);
            ((void (*)(id, SEL, BOOL))objc_msgSend)(cl, NSSelectorFromString(@"setCacheUpdatingEnabled:"), YES);
        }
        if (ctrlVisible && (!cacheDraw || -[cacheDraw timeIntervalSinceNow] > 1.0)) { cacheDraw = now; [cl setNeedsDisplay]; }
    }

    // ---- an 2 nut tua cua InfusePlus (Infuse hay bat lai)
    {
        Class UDc = NSClassFromString(@"INFUserDefaults");
        BOOL hideRw = UDc ? ((BOOL (*)(id, SEL, id))objc_msgSend)(UDc, NSSelectorFromString(@"boolForKey:"), @"hideRewindButtons") : NO;
        if (hideRw) SFHideRewind(panelV);
    }

    // ---- man hinh tai chi tiet (thay vong xoay)
    BOOL on = SFLoadHUDOn();
    if (spinner) {
        if (on && !spinner.layer.mask) { CALayer *m = [CALayer layer]; m.frame = spinner.bounds; m.backgroundColor = nil; spinner.layer.mask = m; }
        if (!on && spinner.layer.mask) spinner.layer.mask = nil;
    }
    if (on && spinner && buffering) {
        if (!wasVisible) { wasVisible = YES; visStart = now; }
        double waited = -[visStart timeIntervalSinceNow];
        NSString *dots = [@"" stringByPaddingToLength:(NSUInteger)(tickN / 2 % 4) withString:@"..." startingAtIndex:0];
        NSString *stage, *hint = @"";
        if (!opened) {
            if (topB == 0) stage = @"Đang kết nối tới nguồn";
            else if (!player) stage = @"Đang đọc dữ liệu đầu file";
            else stage = @"Đang phân tích và mở video";
        } else stage = @"Đang đệm dữ liệu";
        if (topB == 0 && waited > 6) hint = @"Chưa nhận được dữ liệu — nguồn bận, ngủ đông hoặc mất kết nối";
        else if (topB > 0 && speed < 150 * 1024 && waited > 4) hint = @"Tốc độ thấp — nguồn/mạng đang chậm";
        else if (opened && speed > 1.5 * 1024 * 1024 && waited > 3) hint = @"Dữ liệu đã đủ nhanh — máy đang giải mã (codec nặng như AV1/HEVC 10-bit?)";
        else if (!opened && topB > 0 && speed > 1024 * 1024 && player) hint = @"Đang dựng bộ giải mã và đọc thông tin track";
        if (!hud) hud = [SFLoadHUDView new];
        UIView *host = spinner.superview;
        if (hud.superview != host) { [hud removeFromSuperview]; [host addSubview:hud]; }
        hud.l1.text = [stage stringByAppendingString:dots];
        hud.l2.text = [NSString stringWithFormat:@"%@ · %@ · %.2f MB/s · %.0fs", srcName, SFMB(topB), speed / 1048576.0, waited];
        hud.l3.text = hint; hud.l3.hidden = hint.length == 0;
        CGFloat W = MIN(320, host.bounds.size.width - 32);
        CGSize sz = [hud systemLayoutSizeFittingSize:CGSizeMake(W, 0) withHorizontalFittingPriority:UILayoutPriorityRequired verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
        CGFloat y = CGRectGetMaxY(spinner.frame) + 8;
        if (y + sz.height > host.bounds.size.height - 8) y = MAX(8, CGRectGetMinY(spinner.frame) - sz.height - 8);
        hud.frame = CGRectMake((host.bounds.size.width - W) / 2, y, W, sz.height);
        [host bringSubviewToFront:hud];
    } else {
        wasVisible = NO;
        if (hud.superview) [hud removeFromSuperview];
    }

    // ---- bang theo doi lien tuc
    BOOL monOn = [[NSUserDefaults standardUserDefaults] boolForKey:@"sf.monbtn"] && SFMonShown();
    if (monOn) {
        if (!mon) mon = [SFMonView new];
        if (mon.superview != vc.view) {
            [mon removeFromSuperview]; [vc.view addSubview:mon];
            NSUserDefaults *u = [NSUserDefaults standardUserDefaults];
            double fx = [u objectForKey:@"sf.monx"] ? [u doubleForKey:@"sf.monx"] : 0.22, fy = [u objectForKey:@"sf.mony"] ? [u doubleForKey:@"sf.mony"] : 0.35;
            [mon layoutIfNeeded];
            CGSize s = [mon systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
            mon.bounds = CGRectMake(0, 0, s.width, s.height);
            mon.center = CGPointMake(vc.view.bounds.size.width * fx, vc.view.bounds.size.height * fy);
            mon.transform = CGAffineTransformMakeScale(SFMonScale(), SFMonScale());
        }
        double nomFPS = vdesc ? [[vdesc valueForKey:@"fps"] doubleValue] : 0;
        NSString *fpsText = !playing ? @"—" : (nomFPS > 1 ? [NSString stringWithFormat:@"%.1f / %.2f%@", liveFPS, nomFPS, liveFPS < nomFPS * 0.85 ? @"  ⚠︎ tụt" : @""] : [NSString stringWithFormat:@"%.1f", liveFPS]);
        NSString *lastRep = [[NSUserDefaults standardUserDefaults] stringForKey:@"sf.lastreport"];
        NSString *state = !opened ? @"Đang tải" : (playing ? @"Đang phát" : (buffering ? @"Đang đệm" : @"Tạm dừng"));
        double avg = elapsed > 1 ? topB / elapsed : 0;
        NSProcessInfo *pi = [NSProcessInfo processInfo];
        NSString *th = @[ @"bình thường", @"hơi ấm", @"nóng", @"rất nóng" ][MAX(0, MIN(3, (int)pi.thermalState))];
        mon.body.text = [NSString stringWithFormat:
            @"Trạng thái  %@\nVị trí      %@ / %@\nNguồn       %@\nĐã đọc      %@\nTốc độ      %.2f MB/s (TB %.2f)\nĐứng hình   %d lần\nNhiệt máy   %@%@\nVideo       %@\nGiải mã     %@\nTrình phát  %@\nFPS hiển thị %@\nĐệm đầy     %d%@%@",
            state, SFTimeStr(ct), SFTimeStr(dur), srcName, SFMB(topB), speed / 1048576.0, avg / 1048576.0, stalls, th, pi.lowPowerModeEnabled ? @"\nTiết kiệm pin: BẬT" : @"", vinfo, soft ? @"Phần mềm (đã chuyển)" : dec, engine, fpsText, fullBuf, soft ? @"   Đã chuyển PM" : @"", lastRep.length ? [@"\nPhiên trước: " stringByAppendingString:lastRep] : @""];
        CGSize s = [mon systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
        CGPoint c = mon.center; mon.bounds = CGRectMake(0, 0, s.width, s.height); mon.center = c;
        [vc.view bringSubviewToFront:mon];
    } else if (mon.superview) [mon removeFromSuperview];

    if (cacheLogs < 3 && (!lastCacheLog || -[lastCacheLog timeIntervalSinceNow] > 5)) {
        UIView *cl = SFFindClass(panelV, @"FCProgressBarCacheLineView", 0);
        if (cl) { lastCacheLog = now; cacheLogs++; SFLog(@"[load] cacheRanges=%@ opened=%d player=%d top=%d bytes=%llu", SFObj0(cl, @"cachedRanges"), opened, player != nil, top, topB); }
    }
}

void SFInstallLoadHUD(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        SFInstallLoadHooks();
        SFInstallSpeedFix();
        SFInstallFrameHooks();
        SFInstallStatsHooks();
        [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *t) { if ([UIApplication sharedApplication].applicationState != UIApplicationStateActive) return; @try { SFLoadHUDTick(); } @catch (NSException *e) { SFLog(@"[load] loi: %@", e); } }];
    });
}
