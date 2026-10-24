// Giao dien cai dat tim sub: man hinh day vao tu Cai dat cua Infuse (giong cac muc khac).
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import "SFEngine.h"

static UIColor *SFCellBG(void) { return [UIColor colorWithWhite:0.11 alpha:1]; }

static UITableViewCell *SFMakeCell(UITableView *tv, NSString *title, NSString *detail) {
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

// ---- lop co so: bang toi, tu cap nhat moi 0.5s khi dang hien

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
}
- (void)viewWillDisappear:(BOOL)a { [super viewWillDisappear:a]; [self.timer invalidate]; self.timer = nil; }
- (NSUInteger)dataSignature { return 0; }
- (void)tickExtra {}
- (void)tick {
    SFEngine *e = [SFEngine shared];
    if ([e isCrawling]) {
        self.bar.progress = (float)[e progress];
        self.statusLabel.text = [NSString stringWithFormat:@"Đang quét: %@ — %ld thư mục, tổng %ld sub (%.0f%%)", [e progressSource] ?: @"...",
                                 (long)[e progressFolders], (long)[e totalSubs], [e progress] * 100];
    } else {
        self.bar.progress = 1;
        self.statusLabel.text = [NSString stringWithFormat:@"Sẵn sàng — %ld sub trong chỉ mục.", (long)[e totalSubs]];
    }
    [self tickExtra];
    NSUInteger s = [self dataSignature] ^ ([e activeKeyValue] ?: @"").hash ^ ([e isCrawling] ? 7 : 0);
    if (s != self.sig) { self.sig = s; [self.tableView reloadData]; }
}
// o trang thai + thanh tien trinh
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
@property (nonatomic, strong) NSArray *folders;
- (instancetype)initWithSource:(NSString *)sid item:(id)item name:(NSString *)name;
@end

// ---- man hinh chinh: Tim sub

@interface SFSettingsVC : SFBaseVC
@property (nonatomic, strong) NSArray *sources;
@end

static NSArray *SFModes(void) { return @[ @"manual", @"launch", @"hourly", @"12h" ]; }

@implementation SFSettingsVC
- (void)viewDidLoad { [super viewDidLoad]; self.title = @"Tìm sub"; self.sources = [[SFEngine shared] sourceList]; }
- (NSUInteger)dataSignature { self.sources = [[SFEngine shared] sourceList]; return [self.sources description].hash; }
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv { return 3; }
- (NSInteger)tableView:(UITableView *)tv numberOfRowsInSection:(NSInteger)s { return s == 0 ? 2 : (s == 1 ? 1 : (NSInteger)self.sources.count); }
- (NSString *)tableView:(UITableView *)tv titleForHeaderInSection:(NSInteger)s { return s == 1 ? @"TỰ ĐỘNG LÀM MỚI" : (s == 2 ? @"NGUỒN (BẤM VÀO ĐỂ XEM THƯ MỤC)" : nil); }
- (NSString *)tableView:(UITableView *)tv titleForFooterInSection:(NSInteger)s {
    return s == 2 ? @"Công tắc: bật/tắt tìm sub ở nguồn đó. Nguồn đám mây (Drive...) mặc định tắt vì quét chậm." : nil;
}
- (UITableViewCell *)tableView:(UITableView *)tv cellForRowAtIndexPath:(NSIndexPath *)ip {
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 0) return [self statusCell];
    if (ip.section == 0) {
        BOOL active = [e isCrawling];
        UITableViewCell *c = SFMakeCell(tv, active ? @"Hủy quét" : @"Làm mới tất cả", nil);
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
    NSString *title = [r[@"cloud"] boolValue] ? [NSString stringWithFormat:@"%@  ☁︎", r[@"name"]] : r[@"name"];
    NSString *detail = [NSString stringWithFormat:@"%@ sub · %@", r[@"subs"], SFDate([r[@"last"] doubleValue])];
    UITableViewCell *c = SFMakeCell(tv, title, detail);
    c.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    UISwitch *sw = [[UISwitch alloc] init];
    sw.on = [r[@"enabled"] boolValue];
    sw.tag = ip.row;
    [sw addTarget:self action:@selector(switchChanged:) forControlEvents:UIControlEventValueChanged];
    c.editingAccessoryView = nil;
    UIStackView *st = [[UIStackView alloc] initWithArrangedSubviews:@[ sw ]];
    st.frame = CGRectMake(0, 0, 60, 31);
    c.accessoryView = st;     // cong tac thay mui ten; bam vao dong de vao thu muc
    return c;
}
- (void)modeChanged:(UISegmentedControl *)seg { [[SFEngine shared] setAutoMode:SFModes()[(NSUInteger)seg.selectedSegmentIndex]]; }
- (void)switchChanged:(UISwitch *)sw {
    NSDictionary *r = self.sources[(NSUInteger)sw.tag];
    [[SFEngine shared] setSource:r[@"id"] enabled:sw.isOn];
    self.sig = 0;
}
- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)ip {
    [tv deselectRowAtIndexPath:ip animated:YES];
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 1) { if ([e isCrawling]) [e cancelCrawl]; else [e refreshAll]; self.sig = 0; return; }
    if (ip.section == 2) {
        NSDictionary *r = self.sources[(NSUInteger)ip.row];
        SFFolderVC *vc = [[SFFolderVC alloc] initWithSource:r[@"id"] item:nil name:r[@"name"]];
        [self.navigationController pushViewController:vc animated:YES];
    }
}
@end

// ---- man hinh thu muc cua mot nguon (hoac thu muc con)

@implementation SFFolderVC
- (instancetype)initWithSource:(NSString *)sid item:(id)item name:(NSString *)name {
    if ((self = [super init])) {
        _sid = sid; _item = item; self.title = name;
        id pathObj = item ? ([item respondsToSelector:NSSelectorFromString(@"path")] ? ((id (*)(id, SEL))objc_msgSend)(item, NSSelectorFromString(@"path")) : nil) : nil;
        _path = [pathObj isKindOfClass:[NSString class]] ? pathObj : @"";
        _key = item ? [NSString stringWithFormat:@"%@|%@", sid, _path] : sid;
        _folders = nil;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    __weak SFFolderVC *w = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        SFEngine *e = [SFEngine shared];
        id it = w.item ?: [e rootItemForSource:w.sid];
        NSArray *f = [e childFoldersOfItem:it source:w.sid];
        dispatch_async(dispatch_get_main_queue(), ^{ SFFolderVC *s = w; if (!s) return; s.item = it; s.folders = f; s.sig = 0; [s tick]; });
    });
}
- (NSUInteger)dataSignature {
    NSInteger n = [[SFEngine shared] subCountForSource:self.sid pathPrefix:self.path];
    NSUInteger h = (NSUInteger)n * 31 + self.folders.count;
    for (NSDictionary *d in self.folders) h = h * 31 + (NSUInteger)[[SFEngine shared] subCountForSource:self.sid pathPrefix:d[@"path"]];
    return h;
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tv { return 2; }
- (NSInteger)tableView:(UITableView *)tv numberOfRowsInSection:(NSInteger)s {
    if (s == 0) return 2;
    return self.folders ? (NSInteger)MAX((NSUInteger)1, self.folders.count) : 1;
}
- (NSString *)tableView:(UITableView *)tv titleForHeaderInSection:(NSInteger)s { return s == 1 ? @"THƯ MỤC CON" : nil; }
- (BOOL)thisActive { NSString *k = [[SFEngine shared] activeKeyValue]; return k && ([k isEqualToString:self.key] || [k isEqualToString:@"*"]); }
- (UITableViewCell *)tableView:(UITableView *)tv cellForRowAtIndexPath:(NSIndexPath *)ip {
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 0) return [self statusCell];
    if (ip.section == 0) {
        BOOL busy = [e isCrawling], mine = [self thisActive];
        NSString *t = mine ? @"Hủy quét" : (busy ? @"Đang quét việc khác..." : (self.item ? @"Làm mới thư mục này" : @"Làm mới nguồn này"));
        NSString *d = [NSString stringWithFormat:@"%ld sub trong mục này", (long)[e subCountForSource:self.sid pathPrefix:self.path]];
        UITableViewCell *c = SFMakeCell(tv, t, d);
        c.textLabel.textColor = mine ? UIColor.systemRedColor : (busy ? [UIColor grayColor] : tv.tintColor);
        return c;
    }
    if (!self.folders) return SFMakeCell(tv, @"Đang tải danh sách thư mục...", nil);
    if (self.folders.count == 0) return SFMakeCell(tv, @"(không có thư mục con)", nil);
    NSDictionary *f = self.folders[(NSUInteger)ip.row];
    NSString *k = [NSString stringWithFormat:@"%@|%@", self.sid, f[@"path"]];
    NSString *ak = [e activeKeyValue];
    BOOL running = ak && [ak isEqualToString:k];
    UITableViewCell *c = SFMakeCell(tv, f[@"name"], [NSString stringWithFormat:@"%ld sub", (long)[e subCountForSource:self.sid pathPrefix:f[@"path"]]]);
    c.accessoryType = UITableViewCellAccessoryNone;
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setImage:[UIImage systemImageNamed:running ? @"xmark.circle" : @"arrow.clockwise"] forState:UIControlStateNormal];
    b.tintColor = running ? UIColor.systemRedColor : nil;
    b.frame = CGRectMake(0, 0, 44, 44);
    b.tag = ip.row;
    [b addTarget:self action:@selector(rowRefresh:) forControlEvents:UIControlEventTouchUpInside];
    c.accessoryView = b;
    return c;
}
- (void)rowRefresh:(UIButton *)b {
    NSDictionary *f = self.folders[(NSUInteger)b.tag];
    SFEngine *e = [SFEngine shared];
    NSString *k = [NSString stringWithFormat:@"%@|%@", self.sid, f[@"path"]];
    NSString *ak = [e activeKeyValue];
    if (ak && [ak isEqualToString:k]) { [e cancelCrawl]; }
    else if ([e isCrawling]) { SFToast(@"ISF: đang quét việc khác, hủy trước đã"); }
    else { [e refreshFolderItem:f[@"item"] source:self.sid]; }
    self.sig = 0;
}
- (void)tableView:(UITableView *)tv didSelectRowAtIndexPath:(NSIndexPath *)ip {
    [tv deselectRowAtIndexPath:ip animated:YES];
    SFEngine *e = [SFEngine shared];
    if (ip.section == 0 && ip.row == 1) {
        if ([self thisActive]) [e cancelCrawl];
        else if ([e isCrawling]) SFToast(@"ISF: đang quét việc khác, hủy trước đã");
        else if (self.item) [e refreshFolderItem:self.item source:self.sid];
        else [e refreshSource:self.sid];
        self.sig = 0;
        return;
    }
    if (ip.section == 1 && self.folders.count > (NSUInteger)ip.row) {
        NSDictionary *f = self.folders[(NSUInteger)ip.row];
        SFFolderVC *vc = [[SFFolderVC alloc] initWithSource:self.sid item:f[@"item"] name:f[@"name"]];
        [self.navigationController pushViewController:vc animated:YES];
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
