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
    if ((self = [super initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterialDark]])) {
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
    if ((self = [super initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterialDark]])) {
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

static void SFLoadHUDTick(void) {
    static __weak id curVC; static NSDate *vcStart, *visStart; static BOOL wasVisible;
    static unsigned long long lastBytes[SF_LS_MAX]; static NSDate *lastT; static double speed; static int best = -1;
    static __weak UIView *spinner; static SFLoadHUDView *hud; static SFMonView *mon; static int tickN; static NSDate *lastCacheLog; static int cacheLogs;
    static double lastCT = -1; static NSDate *lastAdvance; static int stalls; static BOOL wasStall; static int lastSig = -1;
    tickN++;
    UIViewController *vc = SFProbeTopVC();
    if (![NSStringFromClass([vc class]) isEqualToString:@"FCVideoViewController"]) {
        if (hud.superview) [hud removeFromSuperview];
        if (mon.superview) [mon removeFromSuperview];
        curVC = nil; wasVisible = NO; return;
    }
    if (vc != curVC) {                                   // phien phat moi
        curVC = vc; vcStart = [NSDate date]; visStart = nil; wasVisible = NO; speed = 0; best = -1; lastT = nil;
        lastCT = -1; lastAdvance = nil; stalls = 0; wasStall = NO; lastSig = -1;
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
    if (top >= 0 && dt > 0.05) {
        double inst = (topB - lastBytes[top]) / dt;
        if (top != best) { best = top; speed = inst; } else speed = speed * 0.6 + inst * 0.4;
    }
    for (int i = 0; i < sf_ls_n; i++) lastBytes[i] = atomic_load(&sf_ls[i].bytes);
    lastT = now;

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
        NSString *state = !opened ? @"Đang tải" : (playing ? @"Đang phát" : (buffering ? @"Đang đệm" : @"Tạm dừng"));
        double avg = elapsed > 1 ? topB / elapsed : 0;
        NSProcessInfo *pi = [NSProcessInfo processInfo];
        NSString *th = @[ @"bình thường", @"hơi ấm", @"nóng", @"rất nóng" ][MAX(0, MIN(3, (int)pi.thermalState))];
        mon.body.text = [NSString stringWithFormat:
            @"Trạng thái  %@\nVị trí      %@ / %@\nNguồn       %@\nĐã đọc      %@\nTốc độ      %.2f MB/s (TB %.2f)\nĐứng hình   %d lần\nNhiệt máy   %@%@",
            state, SFTimeStr(ct), SFTimeStr(dur), srcName, SFMB(topB), speed / 1048576.0, avg / 1048576.0, stalls, th, pi.lowPowerModeEnabled ? @"\nTiết kiệm pin: BẬT" : @""];
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
        [NSTimer scheduledTimerWithTimeInterval:0.25 repeats:YES block:^(NSTimer *t) { @try { SFLoadHUDTick(); } @catch (NSException *e) { SFLog(@"[load] loi: %@", e); } }];
    });
}
