// Giao dien cai dat tim sub: man hinh day vao tu Cai dat cua Infuse (giong cac muc khac).
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import "SFEngine.h"

static UIColor *SFCellBG(void) { return [UIColor colorWithWhite:0.11 alpha:1]; }

static UITableViewCell *SFSimpleCell(NSString *title, NSString *detail) {
    UITableViewCell *c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    c.backgroundColor = SFCellBG();
    c.textLabel.text = title;
    c.textLabel.textColor = UIColor.whiteColor;
    c.textLabel.numberOfLines = 0;
    c.detailTextLabel.text = detail;
    c.detailTextLabel.textColor = [UIColor colorWithWhite:0.6 alpha:1];
    c.detailTextLabel.numberOfLines = 0;
    return c;
}

static NSString *SFDate(double t) {
    if (t <= 0) return @"chưa quét";
    static NSDateFormatter *df;
    if (!df) { df = [[NSDateFormatter alloc] init]; df.dateFormat = @"dd/MM HH:mm"; }
    return [df stringFromDate:[NSDate dateWithTimeIntervalSince1970:t]];
}

// ---- dong co: cong tac (trai) | ten + so sub + tien trinh | nut lam moi/huy (phai)

@interface SFRowCell : UITableViewCell
@property (nonatomic, strong) UISwitch *sw;
@property (nonatomic, strong) UILabel *titleL, *detailL;
@property (nonatomic, strong) UIProgressView *bar;
@property (nonatomic, strong) UIButton *btn;
@property (nonatomic, copy) NSString *sid, *path, *key;   // path = nil: nguon
@property (nonatomic, copy) void (^onSwitch)(BOOL on);
@property (nonatomic, copy) void (^onRefresh)(BOOL running);
- (void)updateFromEngine;
@end

@implementation SFRowCell
- (instancetype)init {
    if ((self = [super initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil])) {
        self.backgroundColor = SFCellBG();
        self.sw = [[UISwitch alloc] init];
        [self.sw addTarget:self action:@selector(swChanged) forControlEvents:UIControlEventValueChanged];
        self.titleL = [[UILabel alloc] init];
        self.titleL.textColor = UIColor.whiteColor;
        self.titleL.numberOfLines = 0;
        self.detailL = [[UILabel alloc] init];
        self.detailL.textColor = [UIColor colorWithWhite:0.6 alpha:1];
        self.detailL.font = [UIFont systemFontOfSize:13];
        self.bar = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
        UIStackView *st = [[UIStackView alloc] initWithArrangedSubviews:@[ self.titleL, self.detailL, self.bar ]];
        st.axis = UILayoutConstraintAxisVertical;
        st.spacing = 3;
        self.btn = [UIButton buttonWithType:UIButtonTypeSystem];
        [self.btn addTarget:self action:@selector(btnTap) forControlEvents:UIControlEventTouchUpInside];
        for (UIView *v in @[ self.sw, st, self.btn ]) { v.translatesAutoresizingMaskIntoConstraints = NO; [self.contentView addSubview:v]; }
        UILayoutGuide *m = self.contentView.layoutMarginsGuide;
        [NSLayoutConstraint activateConstraints:@[
            [self.sw.leadingAnchor constraintEqualToAnchor:m.leadingAnchor],
            [self.sw.centerYAnchor constraintEqualToAnchor:self.contentView.centerYAnchor],
            [st.leadingAnchor constraintEqualToAnchor:self.sw.trailingAnchor constant:14],
            [st.trailingAnchor constraintEqualToAnchor:self.btn.leadingAnchor constant:-6],
            [st.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:10],
            [st.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-10],
            [self.btn.trailingAnchor constraintEqualToAnchor:m.trailingAnchor],
            [self.btn.centerYAnchor constraintEqualToAnchor:self.contentView.centerYAnchor],
            [self.btn.widthAnchor constraintEqualToConstant:44],
            [self.btn.heightAnchor constraintEqualToConstant:44],
        ]];
    }
    return self;
}
- (void)swChanged { if (self.onSwitch) self.onSwitch(self.sw.isOn); }
- (void)btnTap {
    BOOL running = [[SFEngine shared] jobInfoForKey:self.key] != nil;
    if (self.onRefresh) self.onRefresh(running);
    [self updateFromEngine];
}
- (void)updateFromEngine {
    SFEngine *e = [SFEngine shared];
    NSDictionary *info = [e jobInfoForKey:self.key];
    if (info) {
        self.detailL.text = [NSString stringWithFormat:@"Đang quét… %@ thư mục · %@ sub (%.0f%%)", info[@"folders"], info[@"subs"], [info[@"progress"] doubleValue] * 100];
        self.bar.hidden = NO;
        self.bar.progress = (float)[info[@"progress"] doubleValue];
    } else {
        NSString *when = self.path ? @"" : [NSString stringWithFormat:@" · %@", SFDate([e lastForKey:self.key])];
        self.detailL.text = [NSString stringWithFormat:@"%ld sub%@", (long)[e subCountForSource:self.sid pathPrefix:self.path], when];
        self.bar.hidden = YES;
    }
    [self.btn setImage:[UIImage systemImageNamed:info ? @"xmark.circle" : @"arrow.clockwise"] forState:UIControlStateNormal];
    self.btn.tintColor = info ? UIColor.systemRedColor : nil;
}
@end

// ---- lop co so: bang toi, cap nhat moi 0.5s

@interface SFBaseVC : UITableViewController
@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic, assign) NSUInteger sig;
@property (nonatomic, strong) UILabel *statusLabel;
@property (nonatomic, strong) UIProgressView *bar;
- (NSUInteger)dataSignature;
- (void)tickExtra;
@end

@implementation SFBaseVC
- (instancetype)init { return [super initWithStyle:UITableViewStyleInsetGrouped]; }
- (void)viewDidLoad {
    [super viewDidLoad];
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    self.tableView.backgroundColor = UIColor.blackColor;
    self.tableView.separatorColor = [UIColor colorWithWhite:0.25 alpha:1];
}
- (void)viewWillAppear:(BOOL)a {
    [super viewWillAppear:a];
    __weak SFBaseVC *w = self;
    self.timer = [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *t) { [w tick]; }];
    self.sig = 0;
}
- (void)viewWillDisappear:(BOOL)a { [super viewWillDisappear:a]; [self.timer invalidate]; self.timer = nil; }
- (NSUInteger)dataSignature { return 0; }
- (void)tickExtra {}
- (void)tick {
    SFEngine *e = [SFEngine shared];
    NSInteger n = [e runningCount];
    self.bar.progress = (float)[e overallProgress];
    self.statusLabel.text = n > 0 ? [NSString stringWithFormat:@"Đang quét %ld tác vụ — tổng %ld sub trong chỉ mục", (long)n, (long)[e totalSubs]]
                                  : [NSString stringWithFormat:@"Sẵn sàng — %ld sub trong chỉ mục", (long)[e totalSubs]];
    for (UITableViewCell *c in self.tableView.visibleCells) if ([c isKindOfClass:[SFRowCell class]]) [(SFRowCell *)c updateFromEngine];
    [self tickExtra];
    NSUInteger s = [self dataSignature];
    if (s != self.sig) { self.sig = s; [self.tableView reloadData]; }
}
- (UITableViewCell *)statusCell {
    UITableViewCell *c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
    c.backgroundColor = SFCellBG();
    c.selectionStyle = UITableViewCellSelectionStyleNone;
    UILabel *l = [[UILabel alloc] init];
    l.font = [UIFont systemFontOfSize:13];
    l.textColor = [UIColor colorWithWhite:0.8 alpha:1];
    l.numberOfLines = 2;
    l.translatesAutoresizingMaskIntoConstraints = NO;
    UIProgressView *b = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
    b.translatesAutoresizingMaskIntoConstraints = NO;
    [c.contentView addSubview:l];
    [c.contentView addSubview:b];
    [NSLayoutConstraint activateConstraints:@[
        [l.leadingAnchor constraintEqualToAnchor:c.contentView.layoutMarginsGuide.leadingAnchor],
        [l.trailingAnchor constraintEqualToAnchor:c.contentView.layoutMarginsGuide.trailingAnchor],
        [l.topAnchor constraintEqualToAnchor:c.contentView.topAnchor constant:10],
        [b.leadingAnchor constraintEqualToAnchor:l.leadingAnchor],
        [b.trailingAnchor constraintEqualToAnchor:l.trailingAnchor],
        [b.topAnchor constraintEqualToAnchor:l.bottomAnchor constant:8],
        [b.bottomAnchor constraintEqualToAnchor:c.contentView.bottomAnchor constant:-12],
    ]];
    self.statusLabel = l; self.bar = b;
    [self tick];
    return c;
}
@end

@interface SFFolderVC : SFBaseVC
@property (nonatomic, copy) NSString *sid, *path, *key;
@property (nonatomic, strong) id item;
@property (nonatomic, strong) NSArray *folders, *files;
@property (nonatomic, assign) BOOL loaded, wasRunning;
- (instancetype)initWithSource:(NSString *)sid item:(id)item name:(NSString *)name;
- (void)loadListing;
@end

// ---- man hinh chinh: Tim sub

@interface SFSettingsVC : SFBaseVC
@property (nonatomic, strong) NSArray *sources;
@end

static NSArray *SFModes(void) { return @[ @"manual", @"launch", @"hourly", @"12h" ]; }

@implementation SFSettingsVC
- (void)viewDidLoad { [super viewDidLoad]; self.title = @"Tìm sub"; self.sources = [[SFEngine shared] sourceList]; }
- (NSUInteger)dataSignature { self.sources = [[SFEngine shared] sourceList]; return [self.sources description].hash ^ (NSUInteger)[[SFEngine shared] runningCount]; }
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv { return 3; }
- (NSInteger)tableView:(UITableView *)tv numberOfRowsInSection:(NSInteger)s { return s == 0 ? 2 : (s == 1 ? 1 : (NSInteger)self.sources.count); }
- (NSString *)tableView:(UITableView *)tv titleForHeaderInSection:(NSInteger)s { return s == 1 ? @"TỰ ĐỘNG LÀM MỚI" : (s == 2 ? @"NGUỒN (BẤM VÀO ĐỂ XEM THƯ MỤC)" : nil); }
- (NSString *)tableView:(UITableView *)tv titleForFooterInSection:(NSInteger)s {
    return s == 2 ? @"Công tắc: bật/tắt tìm sub ở nguồn đó. Nút bên phải: làm mới riêng nguồn đó (bấm lại để hủy). Nguồn đám mây (Drive...) mặc định tắt vì quét chậm." : nil;
}
- (UITableViewCell *)tableView:(UITableView *)tv cellForRowAtIndexPath:(NSIndexPath *)ip {
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 0) return [self statusCell];
    if (ip.section == 0) {
        BOOL active = [e isCrawling];
        UITableViewCell *c = SFSimpleCell(active ? @"Hủy tất cả" : @"Làm mới tất cả", nil);
        c.textLabel.textColor = active ? UIColor.systemRedColor : tv.tintColor;
        return c;
    }
    if (ip.section == 1) {
        UITableViewCell *c = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
        c.backgroundColor = SFCellBG();
        c.selectionStyle = UITableViewCellSelectionStyleNone;
        UISegmentedControl *seg = [[UISegmentedControl alloc] initWithItems:@[ @"Thủ công", @"Mỗi lần mở", @"Mỗi giờ", @"12 giờ" ]];
        NSUInteger mi = [SFModes() indexOfObject:[e autoMode]];
        seg.selectedSegmentIndex = mi == NSNotFound ? 3 : (NSInteger)mi;
        [seg addTarget:self action:@selector(modeChanged:) forControlEvents:UIControlEventValueChanged];
        seg.translatesAutoresizingMaskIntoConstraints = NO;
        [c.contentView addSubview:seg];
        [NSLayoutConstraint activateConstraints:@[
            [seg.leadingAnchor constraintEqualToAnchor:c.contentView.layoutMarginsGuide.leadingAnchor],
            [seg.trailingAnchor constraintEqualToAnchor:c.contentView.layoutMarginsGuide.trailingAnchor],
            [seg.topAnchor constraintEqualToAnchor:c.contentView.topAnchor constant:8],
            [seg.bottomAnchor constraintEqualToAnchor:c.contentView.bottomAnchor constant:-8],
        ]];
        return c;
    }
    NSDictionary *r = self.sources[(NSUInteger)ip.row];
    SFRowCell *c = [[SFRowCell alloc] init];
    c.sid = r[@"id"]; c.path = nil; c.key = r[@"id"];
    c.titleL.text = [r[@"cloud"] boolValue] ? [NSString stringWithFormat:@"%@  ☁︎", r[@"name"]] : r[@"name"];
    c.sw.on = [r[@"enabled"] boolValue];
    NSString *sid = r[@"id"];
    c.onSwitch = ^(BOOL on) { [[SFEngine shared] setSource:sid enabled:on]; };
    c.onRefresh = ^(BOOL running) { if (running) [[SFEngine shared] cancelKey:sid]; else [[SFEngine shared] refreshSource:sid]; };
    [c updateFromEngine];
    return c;
}
- (void)modeChanged:(UISegmentedControl *)seg { [[SFEngine shared] setAutoMode:SFModes()[(NSUInteger)seg.selectedSegmentIndex]]; }
- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)ip {
    [tv deselectRowAtIndexPath:ip animated:YES];
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 1) { if ([e isCrawling]) [e cancelAll]; else [e refreshAll]; self.sig = 0; return; }
    if (ip.section == 2) {
        NSDictionary *r = self.sources[(NSUInteger)ip.row];
        [self.navigationController pushViewController:[[SFFolderVC alloc] initWithSource:r[@"id"] item:nil name:r[@"name"]] animated:YES];
    }
}
@end

// ---- man hinh thu muc: thu muc con (cong tac + lam moi) va file sub (da them / chua them)

@implementation SFFolderVC
- (instancetype)initWithSource:(NSString *)sid item:(id)item name:(NSString *)name {
    if ((self = [super init])) {
        _sid = sid; _item = item; self.title = name;
        id p = item && [item respondsToSelector:NSSelectorFromString(@"path")] ? ((id (*)(id, SEL))objc_msgSend)(item, NSSelectorFromString(@"path")) : nil;
        _path = item ? ([p isKindOfClass:[NSString class]] ? p : @"") : nil;
        _key = [[SFEngine shared] keyForSource:sid path:_path];
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"arrow.clockwise"] style:UIBarButtonItemStylePlain target:self action:@selector(topRefresh)];
    [self loadListing];
}
- (void)loadListing {
    __weak SFFolderVC *w = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        SFFolderVC *s0 = w; if (!s0) return;
        SFEngine *e = [SFEngine shared];
        id it = s0.item ?: [e rootItemForSource:s0.sid];
        NSDictionary *d = [e listItem:it source:s0.sid];
        dispatch_async(dispatch_get_main_queue(), ^{
            SFFolderVC *s = w; if (!s) return;
            s.item = it; s.folders = d[@"folders"]; s.files = d[@"files"]; s.loaded = YES; s.sig = 0;
        });
    });
}
- (void)topRefresh {
    SFEngine *e = [SFEngine shared];
    if ([e jobInfoForKey:self.key]) [e cancelKey:self.key];
    else if (self.path) [e refreshFolderItem:self.item source:self.sid];
    else [e refreshSource:self.sid];
}
- (void)tickExtra {
    SFEngine *e = [SFEngine shared];
    BOOL running = [e jobInfoForKey:self.key] != nil;
    [self.navigationItem.rightBarButtonItem setImage:[UIImage systemImageNamed:running ? @"xmark.circle" : @"arrow.clockwise"]];
    self.navigationItem.rightBarButtonItem.tintColor = running ? UIColor.systemRedColor : nil;
    if (self.wasRunning && !running) [self loadListing];   // quet xong: doc lai de cap nhat "da them / chua them"
    self.wasRunning = running;
}
- (NSUInteger)dataSignature {
    NSUInteger h = self.folders.count * 7 + self.files.count * 13 + (self.loaded ? 1 : 0);
    for (NSDictionary *f in self.files) h = h * 31 + [f[@"indexed"] unsignedIntegerValue];
    return h;
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv { return 3; }
- (NSInteger)tableView:(UITableView *)tv numberOfRowsInSection:(NSInteger)s {
    if (s == 0) return 1;
    NSArray *a = s == 1 ? self.folders : self.files;
    return self.loaded ? (NSInteger)MAX((NSUInteger)1, a.count) : 1;
}
- (NSString *)tableView:(UITableView *)tv titleForHeaderInSection:(NSInteger)s {
    if (s == 1) return @"THƯ MỤC CON";
    if (s == 2) {
        NSInteger done = 0; for (NSDictionary *f in self.files) if ([f[@"indexed"] boolValue]) done++;
        NSInteger fresh = (NSInteger)self.files.count - done;
        return self.loaded ? [NSString stringWithFormat:@"SUB TRONG THƯ MỤC NÀY — %ld ĐÃ THÊM, %ld CHƯA THÊM", (long)done, (long)fresh] : @"SUB TRONG THƯ MỤC NÀY";
    }
    return nil;
}
- (NSString *)tableView:(UITableView *)tv titleForFooterInSection:(NSInteger)s {
    return s == 2 ? @"Dấu tích xanh: đã có trong chỉ mục. Chưa thêm: có trên ổ nhưng chưa quét — bấm nút làm mới ở trên cùng để thêm." : nil;
}
- (UITableViewCell *)tableView:(UITableView *)tv cellForRowAtIndexPath:(NSIndexPath *)ip {
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0) return [self statusCell];
    if (!self.loaded) return SFSimpleCell(@"Đang tải danh sách...", nil);
    if (ip.section == 1) {
        if (self.folders.count == 0) return SFSimpleCell(@"(không có thư mục con)", nil);
        NSDictionary *f = self.folders[(NSUInteger)ip.row];
        NSString *sid = self.sid, *p = f[@"path"];
        SFRowCell *c = [[SFRowCell alloc] init];
        c.sid = sid; c.path = p; c.key = [e keyForSource:sid path:p];
        c.titleL.text = f[@"name"];
        c.sw.on = ![e isPathItselfDisabled:sid path:p];
        c.onSwitch = ^(BOOL on) { [[SFEngine shared] setPath:p source:sid enabled:on]; };
        id item = f[@"item"];
        NSString *key = c.key;
        c.onRefresh = ^(BOOL running) { if (running) [[SFEngine shared] cancelKey:key]; else [[SFEngine shared] refreshFolderItem:item source:sid]; };
        [c updateFromEngine];
        return c;
    }
    if (self.files.count == 0) return SFSimpleCell(@"(không có file sub)", nil);
    NSDictionary *f = self.files[(NSUInteger)ip.row];
    BOOL indexed = [f[@"indexed"] boolValue];
    UITableViewCell *c = SFSimpleCell(f[@"name"], indexed ? @"Đã thêm" : @"Chưa thêm");
    c.selectionStyle = UITableViewCellSelectionStyleNone;
    c.imageView.image = [UIImage systemImageNamed:indexed ? @"checkmark.circle.fill" : @"circle"];
    c.imageView.tintColor = indexed ? UIColor.systemGreenColor : [UIColor grayColor];
    return c;
}
- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)ip {
    [tv deselectRowAtIndexPath:ip animated:YES];
    if (ip.section == 1 && self.loaded && self.folders.count > (NSUInteger)ip.row) {
        NSDictionary *f = self.folders[(NSUInteger)ip.row];
        [self.navigationController pushViewController:[[SFFolderVC alloc] initWithSource:self.sid item:f[@"item"] name:f[@"name"]] animated:YES];
    }
}
@end

void SFOpenSettings(id ctl) {
    UIViewController *vc = ctl;
    SFSettingsVC *page = [[SFSettingsVC alloc] init];
    if (vc.navigationController) [vc.navigationController pushViewController:page animated:YES];
    else {
        UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:page];
        page.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:nav action:@selector(dismissViewControllerAnimated:completion:)];
        [vc presentViewController:nav animated:YES completion:nil];
    }
}

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
        SFOpenSettings(c);
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

void SFInstallUI(void) {}
