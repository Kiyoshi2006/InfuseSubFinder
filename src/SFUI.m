// Nut "ISF" noi tren man hinh + bang cai dat tim sub.
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import "SFEngine.h"

extern UIWindow *SFKeyWindow(void);

static const NSInteger kBtnTag = 0x15F2, kPanelTag = 0x15F3;

@interface SFPanel : UIView
@property (nonatomic, strong) UILabel *status;
@property (nonatomic, strong) UIProgressView *bar;
@property (nonatomic, strong) UIScrollView *scroll;
@property (nonatomic, strong) UISegmentedControl *seg;
@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic, strong) NSArray *rows;
@property (nonatomic, assign) NSUInteger lastSig;
@end

static NSArray *SFModes(void) { return @[ @"manual", @"launch", @"hourly", @"12h" ]; }

@implementation SFPanel

- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) {
        self.tag = kPanelTag;
        self.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.98];
        self.layer.cornerRadius = 14;
        self.layer.masksToBounds = YES;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        CGFloat w = f.size.width;

        UILabel *t = [[UILabel alloc] initWithFrame:CGRectMake(14, 10, w - 100, 26)];
        t.text = @"Tìm sub (InfuseSubFinder)";
        t.textColor = UIColor.whiteColor;
        t.font = [UIFont boldSystemFontOfSize:16];
        [self addSubview:t];

        UIButton *close = [UIButton buttonWithType:UIButtonTypeSystem];
        [close setTitle:@"Đóng" forState:UIControlStateNormal];
        close.frame = CGRectMake(w - 74, 8, 64, 30);
        close.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        [close addTarget:self action:@selector(closeTap) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:close];

        self.status = [[UILabel alloc] initWithFrame:CGRectMake(14, 42, w - 28, 34)];
        self.status.textColor = [UIColor colorWithWhite:0.8 alpha:1];
        self.status.font = [UIFont systemFontOfSize:12];
        self.status.numberOfLines = 2;
        self.status.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        [self addSubview:self.status];

        self.bar = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
        self.bar.frame = CGRectMake(14, 80, w - 28, 4);
        self.bar.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        [self addSubview:self.bar];

        UIButton *all = [UIButton buttonWithType:UIButtonTypeSystem];
        [all setTitle:@"Làm mới tất cả" forState:UIControlStateNormal];
        all.frame = CGRectMake(10, 90, 160, 34);
        all.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
        [all addTarget:self action:@selector(refreshAllTap) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:all];

        UILabel *am = [[UILabel alloc] initWithFrame:CGRectMake(14, 128, w - 28, 18)];
        am.text = @"Tự động làm mới:";
        am.textColor = [UIColor colorWithWhite:0.7 alpha:1];
        am.font = [UIFont systemFontOfSize:12];
        [self addSubview:am];
        self.seg = [[UISegmentedControl alloc] initWithItems:@[ @"Thủ công", @"Mỗi lần mở", @"Mỗi giờ", @"12 giờ" ]];
        self.seg.frame = CGRectMake(14, 148, w - 28, 30);
        self.seg.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        NSUInteger mi = [SFModes() indexOfObject:[[SFEngine shared] autoMode]];
        self.seg.selectedSegmentIndex = mi == NSNotFound ? 3 : (NSInteger)mi;
        [self.seg addTarget:self action:@selector(modeChanged) forControlEvents:UIControlEventValueChanged];
        [self addSubview:self.seg];

        self.scroll = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 188, w, f.size.height - 188)];
        self.scroll.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        [self addSubview:self.scroll];

        [self tick];
        self.timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(tick) userInfo:nil repeats:YES];
    }
    return self;
}

- (void)removeFromSuperview { [self.timer invalidate]; [super removeFromSuperview]; }
- (void)closeTap { [self removeFromSuperview]; }
- (void)refreshAllTap { [[SFEngine shared] refreshAll]; SFToast(@"ISF: đang làm mới tất cả nguồn..."); }
- (void)modeChanged { [[SFEngine shared] setAutoMode:SFModes()[(NSUInteger)self.seg.selectedSegmentIndex]]; }
- (void)switchChanged:(UISwitch *)sw { [[SFEngine shared] setSource:self.rows[(NSUInteger)sw.tag][@"id"] enabled:sw.isOn]; }
- (void)refreshRow:(UIButton *)b {
    [[SFEngine shared] refreshSource:self.rows[(NSUInteger)b.tag][@"id"]];
    SFToast([NSString stringWithFormat:@"ISF: làm mới '%@'", self.rows[(NSUInteger)b.tag][@"name"]]);
}

- (void)tick {
    SFEngine *e = [SFEngine shared];
    BOOL crawling = [e isCrawling];
    self.bar.progress = crawling ? (float)[e progress] : 1.0f;
    if (crawling)
        self.status.text = [NSString stringWithFormat:@"Đang quét: %@ — %ld thư mục, tổng %ld sub (%.0f%%)", [e progressSource] ?: @"...",
                            (long)[e progressFolders], (long)[e totalSubs], [e progress] * 100];
    else
        self.status.text = [NSString stringWithFormat:@"Sẵn sàng — %ld sub trong chỉ mục. Sub để trên iPhone: bấm Làm mới là quét.", (long)[e totalSubs]];

    NSArray *list = [e sourceList];
    NSUInteger sig = [list description].hash;
    if (sig == self.lastSig) return;
    self.lastSig = sig;
    self.rows = list;
    for (UIView *v in [self.scroll.subviews copy]) [v removeFromSuperview];
    CGFloat w = self.bounds.size.width, y = 4;
    NSDateFormatter *df = [[NSDateFormatter alloc] init];
    df.dateFormat = @"dd/MM HH:mm";
    for (NSUInteger i = 0; i < list.count; i++) {
        NSDictionary *r = list[i];
        UISwitch *sw = [[UISwitch alloc] init];
        sw.on = [r[@"enabled"] boolValue];
        sw.tag = (NSInteger)i;
        sw.transform = CGAffineTransformMakeScale(0.8, 0.8);
        sw.frame = CGRectMake(10, y + 8, sw.frame.size.width, sw.frame.size.height);
        [sw addTarget:self action:@selector(switchChanged:) forControlEvents:UIControlEventValueChanged];
        [self.scroll addSubview:sw];

        UILabel *n = [[UILabel alloc] initWithFrame:CGRectMake(70, y + 4, w - 70 - 90, 20)];
        n.text = r[@"name"];
        n.textColor = UIColor.whiteColor;
        n.font = [UIFont systemFontOfSize:14];
        n.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        [self.scroll addSubview:n];
        double last = [r[@"last"] doubleValue];
        UILabel *d = [[UILabel alloc] initWithFrame:CGRectMake(70, y + 24, w - 70 - 90, 16)];
        d.text = [NSString stringWithFormat:@"%@ sub · %@", r[@"subs"], last > 0 ? [df stringFromDate:[NSDate dateWithTimeIntervalSince1970:last]] : @"chưa quét"];
        d.textColor = [UIColor colorWithWhite:0.6 alpha:1];
        d.font = [UIFont systemFontOfSize:11];
        [self.scroll addSubview:d];

        UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:@"Làm mới" forState:UIControlStateNormal];
        b.frame = CGRectMake(w - 84, y + 8, 76, 30);
        b.tag = (NSInteger)i;
        b.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        [b addTarget:self action:@selector(refreshRow:) forControlEvents:UIControlEventTouchUpInside];
        [self.scroll addSubview:b];
        y += 48;
    }
    self.scroll.contentSize = CGSizeMake(w, y + 10);
}
@end

void SFTogglePanel(void) {
    UIWindow *win = SFKeyWindow();
    if (!win) return;
    UIView *old = [win viewWithTag:kPanelTag];
    if (old) { [old removeFromSuperview]; return; }
    CGFloat top = MAX(win.safeAreaInsets.top, 12) + 4;
    SFPanel *p = [[SFPanel alloc] initWithFrame:CGRectMake(12, top, MIN(win.bounds.size.width - 24, 560), win.bounds.size.height - top - 12)];
    [win addSubview:p];
}

@interface SFBtnTarget : NSObject
+ (instancetype)shared;
- (void)tap:(UIButton *)b;
- (void)pan:(UIPanGestureRecognizer *)g;
@end
@implementation SFBtnTarget
+ (instancetype)shared { static SFBtnTarget *t; static dispatch_once_t o; dispatch_once(&o, ^{ t = [SFBtnTarget new]; }); return t; }
- (void)tap:(UIButton *)b { SFTogglePanel(); }
- (void)pan:(UIPanGestureRecognizer *)g {
    UIView *v = g.view;
    CGPoint t = [g translationInView:v.superview];
    v.center = CGPointMake(v.center.x + t.x, v.center.y + t.y);
    [g setTranslation:CGPointZero inView:v.superview];
}
@end

static BOOL SFFloatingButtonEnabled(void) { return [[SFEngine shared] floatingButton]; }

// ---- dong "Tim sub" trong man Cai dat cua Infuse (ngay duoi "Infuse Plus")

static void (*sf_orig_vwa)(id, SEL, BOOL);
static const void *kSFRowKey = &kSFRowKey;

static void SFAddSettingsRow(id ctl) {
    id th = [ctl respondsToSelector:NSSelectorFromString(@"tableHandler")] ? ((id (*)(id, SEL))objc_msgSend)(ctl, NSSelectorFromString(@"tableHandler")) : nil;
    NSArray *secs = [th respondsToSelector:NSSelectorFromString(@"sections")] ? ((id (*)(id, SEL))objc_msgSend)(th, NSSelectorFromString(@"sections")) : nil;
    id sec = secs.firstObject;
    SEL insSel = NSSelectorFromString(@"insertCellConstructor:handler:atIndex:");
    if (!sec || ![sec respondsToSelector:insSel] || objc_getAssociatedObject(sec, kSFRowKey)) return;
    objc_setAssociatedObject(sec, kSFRowKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    __weak id weakCtl = ctl;
    UITableViewCell *(^ctor)(void) = ^UITableViewCell *{
        id c = weakCtl;
        SEL mk = NSSelectorFromString(@"subsectionCellWithTitle:imageName:");
        UITableViewCell *cell = [c respondsToSelector:mk] ? ((id (*)(id, SEL, id, id))objc_msgSend)(c, mk, @"Tìm sub (InfuseSubFinder)", @"") : nil;
        if (!cell) { cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil]; cell.textLabel.text = @"Tìm sub (InfuseSubFinder)"; cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator; }
        cell.imageView.image = [[UIImage systemImageNamed:@"captions.bubble"] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
        return cell;
    };
    void (^handler)(id) = ^(id cell) {
        id c = weakCtl;
        UITableView *tv = [c respondsToSelector:@selector(tableView)] ? ((id (*)(id, SEL))objc_msgSend)(c, @selector(tableView)) : nil;
        NSIndexPath *ip = tv.indexPathForSelectedRow;
        if (ip) [tv deselectRowAtIndexPath:ip animated:YES];
        SFTogglePanel();
    };
    NSUInteger rows = [[sec valueForKey:@"rowsCount"] unsignedIntegerValue];
    ((void (*)(id, SEL, id, id, NSUInteger))objc_msgSend)(sec, insSel, [ctor copy], [handler copy], MIN((NSUInteger)1, rows));
    UITableView *tv = [ctl respondsToSelector:@selector(tableView)] ? ((id (*)(id, SEL))objc_msgSend)(ctl, @selector(tableView)) : nil;
    [tv reloadData];
}

static void sf_hooked_vwa(id self, SEL _cmd, BOOL animated) {
    sf_orig_vwa(self, _cmd, animated);
    @try { SFAddSettingsRow(self); } @catch (NSException *e) { SFLog(@"[ui] loi them dong cai dat: %@", e); }
}

BOOL SFInstallSettingsHook(void) {
    static BOOL done = NO;
    if (done) return YES;
    Class cls = NSClassFromString(@"FCMainUserSettingsController");
    Method m = cls ? class_getInstanceMethod(cls, @selector(viewWillAppear:)) : NULL;
    if (!m) return NO;
    sf_orig_vwa = (void (*)(id, SEL, BOOL))method_getImplementation(m);
    method_setImplementation(m, (IMP)sf_hooked_vwa);
    done = YES;
    SFLog(@"[ui] da hook FCMainUserSettingsController viewWillAppear:");
    return YES;
}

void SFInstallUI(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        void (^ensure)(void) = ^{
            UIWindow *win = SFKeyWindow();
            if (!win) return;
            UIView *btn = [win viewWithTag:kBtnTag];
            if (!btn && SFFloatingButtonEnabled()) {
                UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
                b.tag = kBtnTag;
                b.frame = CGRectMake(win.bounds.size.width - 52, win.bounds.size.height * 0.4, 40, 40);
                b.backgroundColor = [UIColor colorWithWhite:0 alpha:0.55];
                b.layer.cornerRadius = 20;
                b.alpha = 0.5;
                [b setTitle:@"ISF" forState:UIControlStateNormal];
                [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
                b.titleLabel.font = [UIFont boldSystemFontOfSize:11];
                [b addTarget:[SFBtnTarget shared] action:@selector(tap:) forControlEvents:UIControlEventTouchUpInside];
                [b addGestureRecognizer:[[UIPanGestureRecognizer alloc] initWithTarget:[SFBtnTarget shared] action:@selector(pan:)]];
                [win addSubview:b];
                btn = b;
            }
            if (btn) [win bringSubviewToFront:btn];
            UIView *p = [win viewWithTag:kPanelTag];
            if (p) [win bringSubviewToFront:p];
        };
        ensure();
        [NSTimer scheduledTimerWithTimeInterval:2 repeats:YES block:^(NSTimer *t) { ensure(); }];
    });
}
