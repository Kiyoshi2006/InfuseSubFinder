// SFEngine - quet nguon (VFS cua Infuse), khop ten, nhap sub vao cache cua Infuse.
//
// Khong import header nao cua Infuse: moi loi goi di qua objc runtime va kiem tra
// respondsToSelector, nen neu Infuse doi API thi chi mat tinh nang chu khong crash.

#import "SFEngine.h"

#import <UIKit/UIKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#import "SFMatch.h"

// ------------------------------------------------------------------ tien ich runtime

static id SFObj0(id obj, NSString *sel) {
    if (!obj) return nil;
    SEL s = NSSelectorFromString(sel);
    if (![obj respondsToSelector:s]) return nil;
    return ((id (*)(id, SEL))objc_msgSend)(obj, s);
}

static BOOL SFBool0(id obj, NSString *sel) {
    if (!obj) return NO;
    SEL s = NSSelectorFromString(sel);
    if (![obj respondsToSelector:s]) return NO;
    return ((BOOL (*)(id, SEL))objc_msgSend)(obj, s);
}

static NSString *SFStr0(id obj, NSString *sel) {
    id v = SFObj0(obj, sel);
    return [v isKindOfClass:[NSString class]] ? v : nil;
}

static NSArray *SFArr0(id obj, NSString *sel) {
    id v = SFObj0(obj, sel);
    return [v isKindOfClass:[NSArray class]] ? v : nil;
}

static NSString *SFTrimSlash(NSString *s) {
    while (s.length > 1 && [s hasSuffix:@"/"]) s = [s substringToIndex:s.length - 1];
    return s;
}

// ------------------------------------------------------------------ log

static NSString *SFSupportDir(void) {
    static NSString *dir;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        dir = [docs stringByAppendingPathComponent:@"InfuseSubFinder"];
        [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    });
    return dir;
}

void SFLog(NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *msg = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    static dispatch_queue_t q;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ q = dispatch_queue_create("isf.log", DISPATCH_QUEUE_SERIAL); });
    dispatch_async(q, ^{
        NSString *path = [SFSupportDir() stringByAppendingPathComponent:@"log.txt"];
        NSFileManager *fm = [NSFileManager defaultManager];
        NSDictionary *attrs = [fm attributesOfItemAtPath:path error:nil];
        if ([attrs fileSize] > 256 * 1024) {
            NSString *old = [path stringByAppendingString:@".old"];
            [fm removeItemAtPath:old error:nil];
            [fm moveItemAtPath:path toPath:old error:nil];
        }
        if (![fm fileExistsAtPath:path]) [fm createFileAtPath:path contents:nil attributes:nil];
        NSDateFormatter *df = [[NSDateFormatter alloc] init];
        df.dateFormat = @"yyyy-MM-dd HH:mm:ss";
        NSString *line = [NSString stringWithFormat:@"%@ %@\n", [df stringFromDate:[NSDate date]], msg];
        NSFileHandle *h = [NSFileHandle fileHandleForWritingAtPath:path];
        [h seekToEndOfFile];
        [h writeData:[line dataUsingEncoding:NSUTF8StringEncoding]];
        [h closeFile];
    });
}

// ------------------------------------------------------------------ thong bao tren man hinh

UIWindow *SFKeyWindow(void) {
    for (UIScene *sc in [UIApplication sharedApplication].connectedScenes) {
        if (![sc isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *w in ((UIWindowScene *)sc).windows) {
            if (w.isKeyWindow) return w;
        }
    }
    return nil;
}

void SFToast(NSString *text) {
    if (!text.length) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        UIWindow *win = SFKeyWindow();
        if (!win) return;
        UILabel *l = [[UILabel alloc] init];
        l.numberOfLines = 0;
        l.font = [UIFont systemFontOfSize:13 weight:UIFontWeightMedium];
        l.textColor = [UIColor whiteColor];
        l.backgroundColor = [UIColor colorWithWhite:0 alpha:0.78];
        l.layer.cornerRadius = 10;
        l.layer.masksToBounds = YES;
        l.textAlignment = NSTextAlignmentCenter;
        l.text = [NSString stringWithFormat:@"  %@  ", text];
        l.userInteractionEnabled = NO;
        CGFloat w = MIN(win.bounds.size.width - 32, 520);
        CGSize sz = [l sizeThatFits:CGSizeMake(w, CGFLOAT_MAX)];
        CGFloat h = sz.height + 12;
        l.frame = CGRectMake((win.bounds.size.width - w) / 2, win.safeAreaInsets.top + 8, w, h);
        l.alpha = 0;
        [win addSubview:l];
        [UIView animateWithDuration:0.2 animations:^{ l.alpha = 1; }];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [UIView animateWithDuration:0.4 animations:^{ l.alpha = 0; } completion:^(BOOL f) { [l removeFromSuperview]; }];
        });
    });
}

// Bang chan doan: noi dung cuon duoc, giu nguyen den khi bam "Dong" (de chup man hinh).
void SFShowText(NSString *title, NSString *text) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIWindow *win = SFKeyWindow();
        if (!win) return;
        [[win viewWithTag:0x15F1] removeFromSuperview];
        CGFloat top = MAX(win.safeAreaInsets.top, 12) + 4;
        UIView *box = [[UIView alloc] initWithFrame:CGRectMake(12, top, win.bounds.size.width - 24, win.bounds.size.height - top - 12)];
        box.tag = 0x15F1;
        box.backgroundColor = [UIColor colorWithWhite:0.07 alpha:0.97];
        box.layer.cornerRadius = 12;
        box.layer.masksToBounds = YES;
        box.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

        UILabel *t = [[UILabel alloc] initWithFrame:CGRectMake(12, 8, box.bounds.size.width - 100, 28)];
        t.text = title;
        t.textColor = [UIColor whiteColor];
        t.font = [UIFont boldSystemFontOfSize:15];
        t.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        [box addSubview:t];

        UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:@"Đóng" forState:UIControlStateNormal];
        b.frame = CGRectMake(box.bounds.size.width - 80, 6, 70, 32);
        b.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        __weak UIView *weakBox = box;
        [b addAction:[UIAction actionWithHandler:^(UIAction *a) { [weakBox removeFromSuperview]; }]
            forControlEvents:UIControlEventTouchUpInside];
        [box addSubview:b];

        UITextView *tv = [[UITextView alloc] initWithFrame:CGRectMake(8, 42, box.bounds.size.width - 16, box.bounds.size.height - 50)];
        tv.editable = NO;
        tv.backgroundColor = [UIColor clearColor];
        tv.textColor = [UIColor colorWithWhite:0.92 alpha:1];
        tv.font = [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];
        tv.text = text;
        tv.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        [box addSubview:tv];
        [win addSubview:box];
    });
}

// ------------------------------------------------------------------ du lieu

@interface SFSubEntry : NSObject
@property (nonatomic, strong) id item;  // VFSItem cua file sub
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *ext;
@property (nonatomic, copy) NSString *folderName;
@property (nonatomic, copy) NSString *folderPath;
@property (nonatomic, copy) NSString *sourceId;
@property (nonatomic, copy) NSString *sourceName;
@property (nonatomic, copy) NSString *itemClass;
@property (nonatomic, copy) NSString *itemPath;
@end
@implementation SFSubEntry
@end

@interface SFCandidate : NSObject
@property (nonatomic, strong) SFSubEntry *entry;
@property (nonatomic, assign) int score;
@property (nonatomic, copy) NSString *language;
@end
@implementation SFCandidate
@end

// ------------------------------------------------------------------ engine

@interface SFEngine ()
@property (nonatomic, strong) NSDictionary *config;
@property (nonatomic, strong) NSArray<SFSubEntry *> *index;
@property (nonatomic, strong) NSDate *lastCrawl;
@property (nonatomic, assign) BOOL crawling;
@property (nonatomic, strong) dispatch_queue_t crawlQueue;
@property (nonatomic, strong) dispatch_queue_t importQueue;
@property (nonatomic, strong) dispatch_group_t crawlGroup;
@property (nonatomic, strong) NSMutableSet *pendingRetry;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSDate *> *lastPrepared;
@property (nonatomic, strong) NSMutableSet<NSString *> *inFlight;
@property (nonatomic, assign) NSInteger sourcesCrawled;
@property (nonatomic, assign) BOOL toastedIndex;
@property (nonatomic, copy) NSString *diagText;
@property (nonatomic, assign) BOOL diagShown;
@property (nonatomic, assign) NSInteger crawlRetries;
@property (nonatomic, strong) NSMutableDictionary *settings;
@property (nonatomic, copy) NSString *progSource;
@property (nonatomic, assign) double progFraction;
@property (nonatomic, assign) NSInteger progFolders, progSubs;
@property (nonatomic, assign) BOOL launchCrawlDone;
@property (nonatomic, assign) BOOL cancelRequested;
@property (nonatomic, copy) NSString *activeKey;   // nil = tat ca, "sid" = nguon, "sid|path" = thu muc
@property (nonatomic, copy) NSString *subSid, *subPath;
@end

@implementation SFEngine

+ (instancetype)shared {
    static SFEngine *e;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ e = [[SFEngine alloc] init]; });
    return e;
}

- (instancetype)init {
    if ((self = [super init])) {
        _crawlQueue = dispatch_queue_create("isf.crawl", DISPATCH_QUEUE_SERIAL);
        _importQueue = dispatch_queue_create("isf.import", DISPATCH_QUEUE_SERIAL);
        _crawlGroup = dispatch_group_create();
        _pendingRetry = [NSMutableSet set];
        _lastPrepared = [NSMutableDictionary dictionary];
        _inFlight = [NSMutableSet set];
        _index = @[];
        [self loadConfig];
        [self loadSettings];
        [self loadIndexFromDisk];
    }
    return self;
}

// ---------------------------------------------------------------- cau hinh

- (NSDictionary *)defaultConfig {
    return @{
        @"enabled": @YES,
        @"floatingButton": @NO,           // true = nut ISF noi tren man hinh (mac dinh dung dong trong Cai dat)
        @"showToast": @YES,               // hien thong bao trang thai tren man hinh
        @"diagnostic": @NO,               // true = luon hien bang chan doan sau moi lan quet
        @"minScore": @60,
        @"maxPerVideo": @4,
        @"languages": @[],                // vd ["Vietnamese","English"]; rong = tat ca
        @"extensions": @[ @"srt", @"ass", @"ssa", @"vtt" ],
        @"maxDepth": @8,
        @"maxFoldersPerSource": @4000,
        @"sourceBudgetSeconds": @180,
        @"rescanHours": @12,
        @"waitIndexSeconds": @12,
        @"downloadTimeoutSeconds": @15,
        @"includeCloudSources": @NO,
        @"excludeSources": @[],           // ten nguon (so khop mot phan, khong phan biet hoa thuong)
        @"excludeFolders": @[ @"@eaDir", @"$RECYCLE.BIN", @"System Volume Information", @".Trash" ],
        @"skipSameFolder": @YES,          // Infuse da tu thay sub cung thu muc
        @"aliases": @[],                  // vd [["danmachi", "dungeon ni deai wo motomeru no wa ..."]]
    };
}

- (void)loadConfig {
    NSString *path = [SFSupportDir() stringByAppendingPathComponent:@"config.json"];
    NSMutableDictionary *cfg = [[self defaultConfig] mutableCopy];
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data) {
        id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if ([obj isKindOfClass:[NSDictionary class]]) [cfg addEntriesFromDictionary:obj];
        else SFLog(@"[config] config.json khong hop le, dung mac dinh");
    } else {
        NSData *out = [NSJSONSerialization dataWithJSONObject:cfg options:NSJSONWritingPrettyPrinted error:nil];
        [out writeToFile:path atomically:YES];
    }
    self.config = cfg;

    sf_clear_aliases();
    for (id pair in (NSArray *)cfg[@"aliases"]) {
        if ([pair isKindOfClass:[NSArray class]] && [pair count] == 2 && [pair[0] isKindOfClass:[NSString class]] &&
            [pair[1] isKindOfClass:[NSString class]]) {
            sf_add_alias([pair[0] UTF8String], [pair[1] UTF8String]);
        }
    }
}

- (NSInteger)intCfg:(NSString *)key {
    return [self.config[key] integerValue];
}

- (BOOL)boolCfg:(NSString *)key {
    return [self.config[key] boolValue];
}

// ---------------------------------------------------------------- quet nguon

- (BOOL)isFolderItem:(id)item {
    SEL ens = NSSelectorFromString(@"ensureThatFileTypeIsCached");
    if ([item respondsToSelector:ens]) ((void (*)(id, SEL))objc_msgSend)(item, ens);
    if (SFBool0(item, @"isDVDFolder") || SFBool0(item, @"isBlurayFolder") || SFBool0(item, @"isPlayableFolder")) return NO;
    return SFBool0(item, @"isRegularFolder") || SFBool0(item, @"isSeasonFolder") || SFBool0(item, @"isTVShowFolder") ||
           SFBool0(item, @"isMovieFolder") || SFBool0(item, @"isCollectionFolder") || SFBool0(item, @"isAllSeasonsFolder");
}

- (BOOL)shouldCrawlVFS:(id)vfs manager:(id)mgr {
    if (!vfs) return NO;
    NSString *cls = NSStringFromClass([vfs class]);
    for (NSString *skip in @[ @"VFSLibrary", @"VFSISOImage", @"VFSUDFImage", @"VFSITunes", @"VFSSandisk", @"VFSLinks" ]) {
        if ([cls isEqualToString:skip]) return NO;
    }
    if ([vfs respondsToSelector:NSSelectorFromString(@"isEnabled")] && !SFBool0(vfs, @"isEnabled")) return NO;
    if (![vfs respondsToSelector:NSSelectorFromString(@"makeRootItem")]) return NO;
    if ([SFArr0(mgr, @"mediaServerVFSs") containsObject:vfs]) return NO;   // Plex / Emby / Jellyfin co sub rieng
    if ([SFArr0(mgr, @"embyJellyfinVFSs") containsObject:vfs]) return NO;
    NSString *name = [SFStr0(vfs, @"name") lowercaseString] ?: @"";
    for (id ex in (NSArray *)self.config[@"excludeSources"]) {
        if ([ex isKindOfClass:[NSString class]] && [name containsString:[ex lowercaseString]]) return NO;
    }
    return YES;
}

- (NSString *)autoMode { return self.settings[@"auto"] ?: @"12h"; }

- (void)startIndexingIfNeeded {
    NSString *mode = [self autoMode];
    BOOL force = NO;
    NSTimeInterval maxAge = 12 * 3600.0;
    @synchronized(self) {
        if (!self.lastCrawl) force = YES;                              // chua tung quet
        else if ([mode isEqualToString:@"launch"]) { if (!self.launchCrawlDone) force = YES; }
        else if ([mode isEqualToString:@"hourly"]) maxAge = 3600.0;
        else if ([mode isEqualToString:@"manual"]) maxAge = 1e12;
        if (!force && self.lastCrawl && -[self.lastCrawl timeIntervalSinceNow] < maxAge) return;
        self.launchCrawlDone = YES;
    }
    [self startCrawlOnly:nil];
}

- (BOOL)floatingButton { return [self.config[@"floatingButton"] boolValue]; }

- (BOOL)isCrawling { @synchronized(self) { return self.crawling; } }

// Quet mot so nguon (only = nil: tat ca nguon dang bat). Chay nen.
- (void)startCrawlOnly:(NSSet *)only { [self startCrawlOnly:only startItem:nil key:nil]; }

- (void)startCrawlOnly:(NSSet *)only startItem:(id)startItem key:(NSString *)key {
    if (![self boolCfg:@"enabled"]) return;
    @synchronized(self) {
        if (self.crawling) return;
        self.crawling = YES;
        self.cancelRequested = NO;
        self.activeKey = key;
        if (startItem) { self.subSid = [key componentsSeparatedByString:@"|"].firstObject; self.subPath = SFStr0(startItem, @"path") ?: @""; }
        else { self.subSid = nil; self.subPath = nil; }
    }
    self.progFraction = 0; self.progSource = nil;
    dispatch_group_enter(self.crawlGroup);
    dispatch_async(self.crawlQueue, ^{
        BOOL ok = NO;
        @try {
            @autoreleasepool { ok = [self crawlOnly:only startItem:startItem]; }
        } @catch (NSException *e) {
            SFLog(@"[index] loi: %@", e);
            self.diagText = [NSString stringWithFormat:@"Ngoai le khi quet: %@", e];
        }
        BOOL retry = NO;
        @synchronized(self) {
            self.crawling = NO;
            self.subSid = nil; self.subPath = nil; self.activeKey = nil;
            if (ok) { self.lastCrawl = [NSDate date]; self.crawlRetries = 0; }
            else if (self.crawlRetries < 6 && !self.cancelRequested) { self.crawlRetries++; retry = YES; }
            else self.lastCrawl = [NSDate date];
        }
        [self saveIndex];
        dispatch_group_leave(self.crawlGroup);
        if (retry) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(4 * NSEC_PER_SEC)), self.crawlQueue, ^{ [self startCrawlOnly:only startItem:startItem key:key]; });
        }
    });
}

- (void)refreshAll { [self startCrawlOnly:nil startItem:nil key:nil]; }
- (void)refreshSource:(NSString *)sid { if (sid.length) [self startCrawlOnly:[NSSet setWithObject:sid] startItem:nil key:sid]; }
- (void)refreshFolderItem:(id)item source:(NSString *)sid {
    NSString *path = SFStr0(item, @"path") ?: @"";
    [self startCrawlOnly:[NSSet setWithObject:sid] startItem:item key:[NSString stringWithFormat:@"%@|%@", sid, path]];
}
- (void)cancelCrawl { self.cancelRequested = YES; }
- (NSString *)activeKeyValue { @synchronized(self) { return self.crawling ? (self.activeKey ?: @"*") : nil; } }

// Thu muc con (chi thu muc) cua mot item: goi tu luong nen. Moi phan tu: name, path, item, subs.
- (NSArray<NSDictionary *> *)childFoldersOfItem:(id)item source:(NSString *)sid {
    id vfs = [self vfsForSourceId:sid];
    id browser = SFObj0(vfs, @"browser");
    SEL fs = NSSelectorFromString(@"fetchChildrenForItem:outError:");
    if (!item || ![browser respondsToSelector:fs]) return @[];
    NSError *__autoreleasing err = nil;
    BOOL ok = NO;
    @try { ok = ((BOOL (*)(id, SEL, id, NSError **))objc_msgSend)(browser, fs, item, &err); } @catch (NSException *e) { ok = NO; }
    if (!ok) return @[];
    id lock = SFObj0(item, @"childItemsLockObject");
    NSArray *children;
    @synchronized(lock ?: self) { children = [SFArr0(item, @"childItems") copy]; }
    NSMutableArray *out = [NSMutableArray array];
    NSSet *exts = [NSSet setWithArray:(NSArray *)self.config[@"extensions"]];
    for (id c in children) {
        NSString *name = SFStr0(c, @"fileName");
        if (!name.length) continue;
        NSString *ext = [(SFStr0(c, @"fileExtension") ?: name.pathExtension) lowercaseString];
        if ([exts containsObject:ext] || ![self isFolderItem:c]) continue;
        NSString *p = SFStr0(c, @"path") ?: @"";
        [out addObject:@{ @"name": SFTrimSlash(name), @"path": p, @"item": c, @"subs": @([self subCountForSource:sid pathPrefix:p]) }];
    }
    [out sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) { return [a[@"name"] localizedStandardCompare:b[@"name"]]; }];
    return out;
}
- (id)rootItemForSource:(NSString *)sid { return SFObj0([self vfsForSourceId:sid], @"makeRootItem"); }
- (NSInteger)subCountForSource:(NSString *)sid pathPrefix:(NSString *)p {
    NSArray<SFSubEntry *> *idx;
    @synchronized(self) { idx = self.index; }
    NSInteger n = 0;
    NSString *pp = [p stringByAppendingString:@"/"];
    for (SFSubEntry *e in idx) {
        if (![e.sourceId isEqualToString:sid]) continue;
        if (!p.length || [e.folderPath isEqualToString:p] || [e.folderPath hasPrefix:pp]) n++;
    }
    return n;
}

// ---- cai dat nguoi dung (settings.json)

- (NSString *)settingsPath { return [SFSupportDir() stringByAppendingPathComponent:@"settings.json"]; }
- (void)loadSettings {
    NSData *d = [NSData dataWithContentsOfFile:[self settingsPath]];
    id o = d ? [NSJSONSerialization JSONObjectWithData:d options:NSJSONReadingMutableContainers error:nil] : nil;
    self.settings = [o isKindOfClass:[NSMutableDictionary class]] ? o : [NSMutableDictionary dictionary];
}
- (void)saveSettings {
    NSData *d;
    @synchronized(self) { d = [NSJSONSerialization dataWithJSONObject:self.settings options:0 error:nil]; }
    if (d) [d writeToFile:[self settingsPath] atomically:YES];
}
- (id)vfsForSourceId:(NSString *)sid {
    id mgr = SFObj0(NSClassFromString(@"VFSManager"), @"sharedManager");
    for (id v in SFArr0(mgr, @"allVFSs")) {
        if ([(SFStr0(v, @"identifier") ?: SFStr0(v, @"name")) isEqualToString:sid]) return v;
    }
    return nil;
}
- (BOOL)isCloudSource:(NSString *)sid {
    id mgr = SFObj0(NSClassFromString(@"VFSManager"), @"sharedManager");
    id v = [self vfsForSourceId:sid];
    return v && [SFArr0(mgr, @"cloudVFSs") containsObject:v];
}
- (BOOL)isSourceDisabled:(NSString *)sid {
    @synchronized(self) { if ([(NSArray *)self.settings[@"disabled"] containsObject:sid]) return YES; }
    if ([self isCloudSource:sid]) @synchronized(self) { return ![(NSArray *)self.settings[@"cloudOn"] containsObject:sid]; }
    return NO;
}
- (void)setSource:(NSString *)sid enabled:(BOOL)on {
    @synchronized(self) {
        NSMutableArray *a = [(NSArray *)self.settings[@"disabled"] mutableCopy] ?: [NSMutableArray array];
        [a removeObject:sid];
        if (!on) [a addObject:sid];
        self.settings[@"disabled"] = a;
        NSMutableArray *c = [(NSArray *)self.settings[@"cloudOn"] mutableCopy] ?: [NSMutableArray array];
        [c removeObject:sid];
        if (on) [c addObject:sid];
        self.settings[@"cloudOn"] = c;
    }
    [self saveSettings];
}
- (void)setAutoMode:(NSString *)mode {
    @synchronized(self) { self.settings[@"auto"] = mode; }
    [self saveSettings];
}

// Danh sach nguon cho giao dien: id, name, subs, enabled, last (giay).
- (NSArray<NSDictionary *> *)sourceList {
    id mgr = SFObj0(NSClassFromString(@"VFSManager"), @"sharedManager");
    NSMutableArray *out = [NSMutableArray array];
    NSArray<SFSubEntry *> *idx;
    @synchronized(self) { idx = self.index; }
    for (id v in SFArr0(mgr, @"allVFSs")) {
        NSString *name = SFStr0(v, @"name") ?: @"?";
        if ([name isEqualToString:@"vfLocalFiles"] || ![self shouldCrawlVFS:v manager:mgr]) continue;
        NSString *sid = SFStr0(v, @"identifier") ?: name;
        NSInteger n = 0;
        for (SFSubEntry *e in idx) if ([e.sourceId isEqualToString:sid]) n++;
        NSNumber *last = ((NSDictionary *)self.settings[@"last"])[sid];
        [out addObject:@{ @"id": sid, @"name": name, @"subs": @(n), @"enabled": @(![self isSourceDisabled:sid]), @"last": last ?: @0,
                          @"kind": NSStringFromClass([v class]), @"cloud": @([self isCloudSource:sid]),
                          @"root": SFStr0(v, @"rootPath") ?: @"" }];
    }
    return out;
}

- (NSInteger)totalSubs { @synchronized(self) { return (NSInteger)self.index.count; } }
- (NSString *)progressSource { return self.progSource; }
- (double)progress { return self.progFraction; }
- (NSInteger)progressFolders { return self.progFolders; }

// Cong bo ket qua tam: ket qua moi + muc cu cua nguon chua quet (va con ton tai).
- (void)publishResult:(NSArray *)result crawled:(NSSet *)crawled including:(NSString *)cur allIds:(NSSet *)allIds {
    @synchronized(self) {
        NSMutableArray *merged = [result mutableCopy];
        for (SFSubEntry *old in self.index) {
            if (self.subSid) {   // lam moi mot thu muc: chi thay phan thuoc thu muc do
                BOOL inSub = [old.sourceId isEqualToString:self.subSid] &&
                    ([old.folderPath isEqualToString:self.subPath] || [old.folderPath hasPrefix:[self.subPath stringByAppendingString:@"/"]]);
                if (!inSub) [merged addObject:old];
                continue;
            }
            if ([crawled containsObject:old.sourceId] || [cur isEqualToString:old.sourceId]) continue;
            if (![allIds containsObject:old.sourceId]) continue;
            [merged addObject:old];
        }
        self.index = merged;
    }
}

// Tra ve NO neu VFS chua san sang (se thu lai).
- (BOOL)crawlAll { return [self crawlOnly:nil]; }

- (BOOL)crawlOnly:(NSSet *)only { return [self crawlOnly:only startItem:nil]; }

- (BOOL)crawlOnly:(NSSet *)only startItem:(id)startItem {
    Class mgrCls = NSClassFromString(@"VFSManager");
    id mgr = SFObj0(mgrCls, @"sharedManager");
    NSArray *all = SFArr0(mgr, @"allVFSs");
    if (!mgr || all.count == 0) {
        SFLog(@"[index] chua co VFSManager/allVFSs (mgr=%d, so nguon=%lu)", mgr != nil, (unsigned long)all.count);
        self.diagText = [NSString stringWithFormat:@"VFSManager=%d, allVFSs=%lu (rong/chua san sang)", mgr != nil, (unsigned long)all.count];
        return NO;
    }
    [self loadConfig];
    NSDate *t0 = [NSDate date];
    NSMutableArray<SFSubEntry *> *result = [NSMutableArray array];
    NSMutableString *diag = [NSMutableString string];
    [diag appendFormat:@"Infuse %@ | %lu nguon\n", [NSBundle mainBundle].infoDictionary[@"CFBundleShortVersionString"], (unsigned long)all.count];
    NSSet *exts = [NSSet setWithArray:(NSArray *)self.config[@"extensions"]];
    NSSet *excludeFolders = [NSSet setWithArray:(NSArray *)self.config[@"excludeFolders"]];
    NSInteger maxDepth = [self intCfg:@"maxDepth"];
    NSInteger maxFolders = [self intCfg:@"maxFoldersPerSource"];
    NSTimeInterval budget = [self intCfg:@"sourceBudgetSeconds"];
    SEL fetchSel = NSSelectorFromString(@"fetchChildrenForItem:outError:");


    // Uu tien nguon nhanh/noi bo (iPhone, bookmark) truoc, mang (WebDAV/FTP/SMB) sau.
    all = [all sortedArrayUsingComparator:^NSComparisonResult(id x, id y) {
        NSString *cx = NSStringFromClass([x class]), *cy = NSStringFromClass([y class]);
        int rx = [cx isEqualToString:@"VFSBookmarks"] ? 0 : ([cx isEqualToString:@"VFSLocal"] ? 1 : 2);
        int ry = [cy isEqualToString:@"VFSBookmarks"] ? 0 : ([cy isEqualToString:@"VFSLocal"] ? 1 : 2);
        if (rx != ry) return rx < ry ? NSOrderedAscending : NSOrderedDescending;
        return NSOrderedSame;
    }];
    NSMutableSet *crawledIds = [NSMutableSet set];
    NSMutableSet *allIds = [NSMutableSet set];
    NSInteger totalSources = 0;
    for (id v in all) {
        NSString *i0 = SFStr0(v, @"identifier") ?: (SFStr0(v, @"name") ?: @"?");
        [allIds addObject:i0];
        if ([[SFStr0(v, @"name") ?: @"" description] isEqualToString:@"vfLocalFiles"]) continue;
        if ([self shouldCrawlVFS:v manager:mgr] && (only ? [only containsObject:i0] : ![self isSourceDisabled:i0])) totalSources++;
    }
    totalSources = MAX(1, totalSources);
    self.progFraction = 0;
    NSInteger sourcesDone = 0;
    for (id vfs in all) {
        if ([(SFStr0(vfs, @"name") ?: @"") isEqualToString:@"vfLocalFiles"]) continue;  // thu muc noi bo cua app, 16000 file khong co sub
        NSString *srcName = SFStr0(vfs, @"name") ?: @"?";
        NSString *cls = NSStringFromClass([vfs class]);
        long proto = -1;
        if ([vfs respondsToSelector:NSSelectorFromString(@"protocol")]) proto = ((long (*)(id, SEL))objc_msgSend)(vfs, NSSelectorFromString(@"protocol"));
        if (![self shouldCrawlVFS:vfs manager:mgr]) {
            [diag appendFormat:@"- %@ [%@ p%ld]: BO QUA\n", srcName, cls, proto];
            continue;
        }
        NSString *srcId = SFStr0(vfs, @"identifier") ?: srcName;
        if (self.cancelRequested) break;
        if (only ? ![only containsObject:srcId] : [self isSourceDisabled:srcId]) continue;
        sourcesDone++;
        self.progSource = srcName;
        self.progFolders = 0;
        id root = startItem ?: SFObj0(vfs, @"makeRootItem");
        id browser = SFObj0(vfs, @"browser");
        if (!root || !browser || ![browser respondsToSelector:fetchSel]) {
            SFLog(@"[index] bo qua nguon '%@' (root=%d browser=%d)", srcName, root != nil, browser != nil);
            [diag appendFormat:@"- %@ [%@ p%ld]: khong co root(%d)/browser(%d)\n", srcName, cls, proto, root != nil, browser != nil];
            continue;
        }

        NSDate *srcStart = [NSDate date];
        if (!startItem) {   // nguon chi dinh thang mot file sub: root chinh la file do
            NSString *rn = SFStr0(root, @"fileName");
            NSString *re = [(SFStr0(root, @"fileExtension") ?: rn.pathExtension) lowercaseString];
            if (rn.length && [exts containsObject:re] && sf_is_subtitle_ext(re.UTF8String)) {
                SFSubEntry *e = [[SFSubEntry alloc] init];
                e.item = root; e.name = rn; e.ext = re;
                e.folderName = SFTrimSlash(SFStr0(SFObj0(root, @"parent"), @"fileName") ?: srcName);
                e.folderPath = SFStr0(SFObj0(root, @"parent"), @"path") ?: @"";
                e.sourceId = srcId; e.sourceName = srcName;
                e.itemClass = NSStringFromClass([root class]); e.itemPath = SFStr0(root, @"path");
                [result addObject:e];
                [diag appendFormat:@"  (root la file sub: %@)\n", rn];
            }
        }
        NSMutableArray *queue = [NSMutableArray arrayWithObject:@[ root, @0 ]];
        NSUInteger head = 0;
        NSInteger folders = 0, files = 0, subs = 0, failed = 0;
        NSMutableDictionary<NSString *, NSNumber *> *extCount = [NSMutableDictionary dictionary];
        NSMutableArray *sampleNames = [NSMutableArray array];
        NSString *firstErr = nil;
        while (head < queue.count) {
            if (self.cancelRequested) { SFLog(@"[index] da huy"); break; }
            if (-[srcStart timeIntervalSinceNow] > budget || folders >= maxFolders) {
                SFLog(@"[index] '%@' dat gioi han (folders=%ld, %.0fs), dung som", srcName, (long)folders,
                      -[srcStart timeIntervalSinceNow]);
                break;
            }
            NSArray *entry = queue[head++];
            id folder = entry[0];
            NSInteger depth = [entry[1] integerValue];
            folders++;
            self.progFolders = folders;
            self.progSubs = (NSInteger)result.count;
            self.progFraction = ((double)sourcesDone - 1 + (double)folders / (double)(folders + (NSInteger)(queue.count - head))) / (double)totalSources;
            if (folders % 25 == 0) [self publishResult:result crawled:crawledIds including:srcId allIds:allIds];

            NSError *__autoreleasing err = nil;
            BOOL ok = NO;
            @try {
                ok = ((BOOL (*)(id, SEL, id, NSError **))objc_msgSend)(browser, fetchSel, folder, &err);
            } @catch (NSException *e) {
                ok = NO;
                if (!firstErr) firstErr = [NSString stringWithFormat:@"exception %@", e.name];
            }
            if (!ok) {
                failed++;
                if (!firstErr) firstErr = err ? err.localizedDescription : @"fetch tra ve NO";
                continue;
            }

            id lockObj = SFObj0(folder, @"childItemsLockObject");
            NSArray *children;
            @synchronized(lockObj ?: self) { children = [SFArr0(folder, @"childItems") copy]; }

            NSString *folderName = SFTrimSlash(SFStr0(folder, @"fileName") ?: @"");
            NSString *folderPath = SFStr0(folder, @"path") ?: @"";
            for (id child in children) {
                NSString *name = SFStr0(child, @"fileName");
                if (!name.length) continue;
                if (sampleNames.count < 6) [sampleNames addObject:name];
                NSString *ext = [(SFStr0(child, @"fileExtension") ?: name.pathExtension) lowercaseString];
                BOOL isSubExt = [exts containsObject:ext] && sf_is_subtitle_ext(ext.UTF8String);
                if (!isSubExt && [self isFolderItem:child]) {
                    if (depth + 1 > maxDepth) continue;
                    if ([excludeFolders containsObject:SFTrimSlash(name)]) continue;
                    [queue addObject:@[ child, @(depth + 1) ]];
                    continue;
                }
                files++;
                NSString *ek = ext.length ? ext : @"(none)";
                extCount[ek] = @([extCount[ek] integerValue] + 1);
                if (!isSubExt) continue;
                SFSubEntry *e = [[SFSubEntry alloc] init];
                e.item = child;
                e.name = name;
                e.ext = ext;
                e.folderName = folderName;
                e.folderPath = folderPath;
                e.sourceId = srcId;
                e.sourceName = srcName;
                e.itemClass = NSStringFromClass([child class]);
                e.itemPath = SFStr0(child, @"path");
                [result addObject:e];
                subs++;
            }
        }
        NSArray *topExt = [extCount keysSortedByValueUsingComparator:^NSComparisonResult(NSNumber *a, NSNumber *b) { return [b compare:a]; }];
        NSMutableArray *extParts = [NSMutableArray array];
        for (NSString *k in topExt) { if (extParts.count >= 6) break; [extParts addObject:[NSString stringWithFormat:@"%@:%@", k, extCount[k]]]; }
        SFLog(@"[index] '%@' (%@): %ld thu muc, %ld file, %ld sub, %ld loi, %.1fs", srcName, cls, (long)folders, (long)files,
              (long)subs, (long)failed, -[srcStart timeIntervalSinceNow]);
        [diag appendFormat:@"- %@ [%@ p%ld]: %ld thu muc, %ld file, %ld SUB, %ld loi, %.1fs\n    duoi: %@\n    mau: %@\n",
         srcName, cls, proto, (long)folders, (long)files, (long)subs, (long)failed, -[srcStart timeIntervalSinceNow],
         [extParts componentsJoinedByString:@" "], [sampleNames componentsJoinedByString:@" | "]];
        if (firstErr) [diag appendFormat:@"    loi dau tien: %@\n", firstErr];
        [crawledIds addObject:srcId];
        [self publishResult:result crawled:crawledIds including:nil allIds:allIds];
        @synchronized(self) { self.settings[@"last"] = [self.settings[@"last"] mutableCopy] ?: [NSMutableDictionary dictionary]; ((NSMutableDictionary *)self.settings[@"last"])[srcId] = @([[NSDate date] timeIntervalSince1970]); }
        [self saveSettings];
    }

    [self publishResult:result crawled:crawledIds including:nil allIds:allIds];
    self.sourcesCrawled = sourcesDone;
    self.progFraction = 1;
    [self saveIndex];
    [diag appendFormat:@"=> %lu sub, %ld nguon duoc quet, %.1fs\n", (unsigned long)result.count, (long)sourcesDone, -[t0 timeIntervalSinceNow]];
    for (NSUInteger i = 0; i < MIN((NSUInteger)5, result.count); i++) [diag appendFormat:@"   vd: %@ (thu muc: %@)\n", result[i].name, result[i].folderName];
    self.diagText = diag;
    SFLog(@"[index] xong: %lu sub, %ld nguon, %.1fs", (unsigned long)result.count, (long)sourcesDone, -[t0 timeIntervalSinceNow]);

    BOOL show = [self boolCfg:@"showToast"] && (!self.diagShown) && (result.count == 0 || [self boolCfg:@"diagnostic"]);
    if (show) {
        self.diagShown = YES;
        SFShowText(@"ISF chẩn đoán: quét nguồn (chụp màn hình gửi tao)", diag);
    }
    self.toastedIndex = YES;
    return YES;
}

// ---------------------------------------------------------------- chi muc tren dia

- (NSString *)indexPath { return [SFSupportDir() stringByAppendingPathComponent:@"index.json"]; }

- (void)saveIndex {
    NSArray *idx;
    @synchronized(self) { idx = self.index; }
    NSMutableArray *arr = [NSMutableArray array];
    for (SFSubEntry *e in idx) {
        if (!e.itemPath.length) continue;
        [arr addObject:@{ @"sid": e.sourceId ?: @"", @"sn": e.sourceName ?: @"", @"cls": e.itemClass ?: @"", @"p": e.itemPath,
                          @"n": e.name ?: @"", @"x": e.ext ?: @"", @"fn": e.folderName ?: @"", @"fp": e.folderPath ?: @"" }];
    }
    NSData *d = [NSJSONSerialization dataWithJSONObject:arr options:0 error:nil];
    if (d) [d writeToFile:[self indexPath] atomically:YES];
}

- (void)loadIndexFromDisk {
    NSData *d = [NSData dataWithContentsOfFile:[self indexPath]];
    if (!d) return;
    NSArray *arr = [NSJSONSerialization JSONObjectWithData:d options:0 error:nil];
    if (![arr isKindOfClass:[NSArray class]]) return;
    NSMutableArray *out = [NSMutableArray array];
    for (NSDictionary *r in arr) {
        if (![r isKindOfClass:[NSDictionary class]]) continue;
        SFSubEntry *e = [[SFSubEntry alloc] init];
        e.sourceId = r[@"sid"]; e.sourceName = r[@"sn"]; e.itemClass = r[@"cls"]; e.itemPath = r[@"p"];
        e.name = r[@"n"]; e.ext = r[@"x"]; e.folderName = r[@"fn"]; e.folderPath = r[@"fp"];
        [out addObject:e];
    }
    NSDate *mt = [[[NSFileManager defaultManager] attributesOfItemAtPath:[self indexPath] error:nil] fileModificationDate];
    @synchronized(self) { self.index = out; self.lastCrawl = mt; }
    SFLog(@"[index] nap %lu sub tu dia (quet lan cuoi: %@)", (unsigned long)out.count, mt);
}

// Dung lai VFSItem cua file sub (khong can quet lai nguon).
- (id)itemForEntry:(SFSubEntry *)e {
    if (e.item) return e.item;
    id mgr = SFObj0(NSClassFromString(@"VFSManager"), @"sharedManager");
    id vfs = nil;
    for (id v in SFArr0(mgr, @"allVFSs")) {
        if ([(SFStr0(v, @"identifier") ?: SFStr0(v, @"name")) isEqualToString:e.sourceId]) { vfs = v; break; }
    }
    Class cls = NSClassFromString(e.itemClass) ?: NSClassFromString(@"VFSItem");
    SEL initSel = NSSelectorFromString(@"initWithVFS:path:");
    if (!vfs || !cls || !e.itemPath.length || ![cls instancesRespondToSelector:initSel]) return nil;
    id item = ((id (*)(id, SEL, id, id))objc_msgSend)([cls alloc], initSel, vfs, e.itemPath);
    e.item = item;
    return item;
}

// ---------------------------------------------------------------- gan sub vao danh sach "cung thu muc" cua Infuse

static const void *kSFAttachedKey = &kSFAttachedKey;

// Goi tu hook -[FCMediaFile relatedItems]: tra ve danh sach goc + sub khop o noi khac.
// Infuse xu ly cac muc them vao nhu sub nam chung thu muc voi video.
- (NSArray *)relatedItemsForMediaFile:(id)mf original:(NSArray *)orig {
    if (![self boolCfg:@"enabled"] || !mf) return orig;
    NSArray *cached = objc_getAssociatedObject(mf, kSFAttachedKey);
    if (cached) return cached;
    NSArray *result = orig;
    @try { result = [self buildRelatedItemsForMediaFile:mf original:orig] ?: orig; }
    @catch (NSException *e) { SFLog(@"[related] ngoai le: %@", e); }
    objc_setAssociatedObject(mf, kSFAttachedKey, result, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return result;
}

- (NSArray *)buildRelatedItemsForMediaFile:(id)mf original:(NSArray *)orig {
    if (!SFBool0(mf, @"canHaveExternalSubtitles")) return orig;
    id movie = SFObj0(mf, @"movieItem");
    NSString *vName = SFStr0(movie, @"fileName");
    if (!movie || !vName.length) return orig;

    id parent = SFObj0(movie, @"parent");
    NSString *vFolder = SFTrimSlash(SFStr0(parent, @"fileName") ?: @"");
    NSString *vParentPath = SFStr0(parent, @"path");
    NSString *movieSource = SFStr0(SFObj0(movie, @"VFS"), @"identifier");
    BOOL skipSame = [self boolCfg:@"skipSameFolder"];
    int minScore = (int)[self intCfg:@"minScore"];
    NSArray *langFilter = self.config[@"languages"];
    NSArray<SFSubEntry *> *index;
    @synchronized(self) { index = self.index; }
    if (index.count == 0) { SFLog(@"[related] '%@': chua co chi muc sub", vName); return orig; }

    NSMutableArray<SFCandidate *> *cands = [NSMutableArray array];
    for (SFSubEntry *e in index) {
        if ([self isSourceDisabled:e.sourceId]) continue;
        if (skipSame && vParentPath && movieSource && [e.sourceId isEqualToString:movieSource] && [e.folderPath isEqualToString:vParentPath]) continue;
        int sc = sf_score(vName.UTF8String, vFolder.UTF8String, e.name.UTF8String, e.folderName.UTF8String);
        if (sc < minScore) continue;
        const char *lang = sf_language_from_name(e.name.UTF8String);
        NSString *language = lang ? [NSString stringWithUTF8String:lang] : nil;
        if (langFilter.count > 0 && language && ![langFilter containsObject:language]) continue;
        SFCandidate *c = [[SFCandidate alloc] init];
        c.entry = e; c.score = sc; c.language = language;
        [cands addObject:c];
    }
    [cands sortUsingComparator:^NSComparisonResult(SFCandidate *a, SFCandidate *b) {
        if (a.score != b.score) return a.score > b.score ? NSOrderedAscending : NSOrderedDescending;
        return [a.entry.name compare:b.entry.name];
    }];
    NSInteger maxPer = MAX(1, [self intCfg:@"maxPerVideo"]);
    NSMutableArray *added = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];
    for (SFCandidate *c in cands) {
        if ((NSInteger)added.count >= maxPer) break;
        NSString *k = [NSString stringWithFormat:@"%@|%@", c.entry.sourceId, c.entry.itemPath];
        if ([seen containsObject:k]) continue;
        [seen addObject:k];
        id item = [self itemForEntry:c.entry];
        if (!item) { SFLog(@"[related] khong dung lai duoc item cho '%@'", c.entry.name); continue; }
        [added addObject:item];
        SFLog(@"[related] + %@ diem=%d (nguon '%@')", c.entry.name, c.score, c.entry.sourceName);
    }
    SFLog(@"[related] '%@': %lu ung vien, gan %lu sub", vName, (unsigned long)cands.count, (unsigned long)added.count);
    if (added.count == 0) return orig;
    if ([self boolCfg:@"showToast"]) SFToast([NSString stringWithFormat:@"ISF: gắn %lu sub cho '%@'", (unsigned long)added.count, vName]);
    NSMutableArray *out = [NSMutableArray arrayWithArray:orig ?: @[]];
    [out addObjectsFromArray:added];
    return out;
}

@end

// Giao dien (gop vao cung file bien dich de workflow cu van build duoc)
#include "SFUI.inc"
