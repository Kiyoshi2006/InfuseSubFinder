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

static UIWindow *SFKeyWindow(void) {
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
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSDate *> *lastPrepared;
@property (nonatomic, strong) NSMutableSet<NSString *> *inFlight;
@property (nonatomic, assign) NSInteger sourcesCrawled;
@property (nonatomic, assign) BOOL toastedIndex;
@property (nonatomic, copy) NSString *diagText;
@property (nonatomic, assign) BOOL diagShown;
@property (nonatomic, assign) NSInteger crawlRetries;
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
        _lastPrepared = [NSMutableDictionary dictionary];
        _inFlight = [NSMutableSet set];
        _index = @[];
        [self loadConfig];
    }
    return self;
}

// ---------------------------------------------------------------- cau hinh

- (NSDictionary *)defaultConfig {
    return @{
        @"enabled": @YES,
        @"showToast": @YES,               // hien thong bao trang thai tren man hinh
        @"diagnostic": @NO,               // true = luon hien bang chan doan sau moi lan quet
        @"minScore": @60,
        @"maxPerVideo": @4,
        @"languages": @[],                // vd ["Vietnamese","English"]; rong = tat ca
        @"extensions": @[ @"srt", @"ass", @"ssa", @"vtt" ],
        @"maxDepth": @8,
        @"maxFoldersPerSource": @4000,
        @"sourceBudgetSeconds": @180,
        @"rescanMinutes": @30,
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
    if (![self boolCfg:@"includeCloudSources"] && [SFArr0(mgr, @"cloudVFSs") containsObject:vfs]) return NO;
    NSString *name = [SFStr0(vfs, @"name") lowercaseString] ?: @"";
    for (id ex in (NSArray *)self.config[@"excludeSources"]) {
        if ([ex isKindOfClass:[NSString class]] && [name containsString:[ex lowercaseString]]) return NO;
    }
    return YES;
}

- (void)startIndexingIfNeeded {
    if (![self boolCfg:@"enabled"]) return;
    NSTimeInterval maxAge = MAX(1, [self intCfg:@"rescanMinutes"]) * 60.0;
    @synchronized(self) {
        if (self.crawling) return;
        if (self.lastCrawl && -[self.lastCrawl timeIntervalSinceNow] < maxAge) return;
        self.crawling = YES;
    }
    dispatch_group_enter(self.crawlGroup);
    dispatch_async(self.crawlQueue, ^{
        BOOL ok = NO;
        @try {
            @autoreleasepool { ok = [self crawlAll]; }
        } @catch (NSException *e) {
            SFLog(@"[index] loi: %@", e);
            self.diagText = [NSString stringWithFormat:@"Ngoai le khi quet: %@", e];
        }
        BOOL retry = NO;
        @synchronized(self) {
            self.crawling = NO;
            if (ok) { self.lastCrawl = [NSDate date]; self.crawlRetries = 0; }
            else if (self.crawlRetries < 6) { self.crawlRetries++; retry = YES; }
            else self.lastCrawl = [NSDate date];
        }
        dispatch_group_leave(self.crawlGroup);
        if (retry) {
            SFLog(@"[index] VFS chua san sang, thu lai sau 4s (lan %ld)", (long)self.crawlRetries);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(4 * NSEC_PER_SEC)), self.crawlQueue, ^{ [self startIndexingIfNeeded]; });
        }
    });
}

// Tra ve NO neu VFS chua san sang (se thu lai).
- (BOOL)crawlAll {
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

    if ([self boolCfg:@"showToast"] && !self.toastedIndex) SFToast([NSString stringWithFormat:@"ISF: đang quét %lu nguồn...", (unsigned long)all.count]);

    NSInteger sourcesDone = 0;
    for (id vfs in all) {
        NSString *srcName = SFStr0(vfs, @"name") ?: @"?";
        NSString *cls = NSStringFromClass([vfs class]);
        long proto = -1;
        if ([vfs respondsToSelector:NSSelectorFromString(@"protocol")]) proto = ((long (*)(id, SEL))objc_msgSend)(vfs, NSSelectorFromString(@"protocol"));
        if (![self shouldCrawlVFS:vfs manager:mgr]) {
            [diag appendFormat:@"- %@ [%@ p%ld]: BO QUA\n", srcName, cls, proto];
            continue;
        }
        sourcesDone++;
        NSString *srcId = SFStr0(vfs, @"identifier") ?: srcName;
        id root = SFObj0(vfs, @"makeRootItem");
        id browser = SFObj0(vfs, @"browser");
        if (!root || !browser || ![browser respondsToSelector:fetchSel]) {
            SFLog(@"[index] bo qua nguon '%@' (root=%d browser=%d)", srcName, root != nil, browser != nil);
            [diag appendFormat:@"- %@ [%@ p%ld]: khong co root(%d)/browser(%d)\n", srcName, cls, proto, root != nil, browser != nil];
            continue;
        }

        NSDate *srcStart = [NSDate date];
        {   // nguon chi dinh thang mot file sub: root chinh la file do
            NSString *rn = SFStr0(root, @"fileName");
            NSString *re = [(SFStr0(root, @"fileExtension") ?: rn.pathExtension) lowercaseString];
            if (rn.length && [exts containsObject:re] && sf_is_subtitle_ext(re.UTF8String)) {
                SFSubEntry *e = [[SFSubEntry alloc] init];
                e.item = root; e.name = rn; e.ext = re;
                e.folderName = SFTrimSlash(SFStr0(SFObj0(root, @"parent"), @"fileName") ?: srcName);
                e.folderPath = SFStr0(SFObj0(root, @"parent"), @"path") ?: @"";
                e.sourceId = srcId; e.sourceName = srcName;
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
            if (-[srcStart timeIntervalSinceNow] > budget || folders >= maxFolders) {
                SFLog(@"[index] '%@' dat gioi han (folders=%ld, %.0fs), dung som", srcName, (long)folders,
                      -[srcStart timeIntervalSinceNow]);
                break;
            }
            NSArray *entry = queue[head++];
            id folder = entry[0];
            NSInteger depth = [entry[1] integerValue];
            folders++;

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
    }

    @synchronized(self) { self.index = result; self.sourcesCrawled = sourcesDone; }
    [diag appendFormat:@"=> %lu sub, %ld nguon duoc quet, %.1fs\n", (unsigned long)result.count, (long)sourcesDone, -[t0 timeIntervalSinceNow]];
    for (NSUInteger i = 0; i < MIN((NSUInteger)5, result.count); i++) [diag appendFormat:@"   vd: %@ (thu muc: %@)\n", result[i].name, result[i].folderName];
    self.diagText = diag;
    SFLog(@"[index] xong: %lu sub, %ld nguon, %.1fs", (unsigned long)result.count, (long)sourcesDone, -[t0 timeIntervalSinceNow]);

    BOOL show = [self boolCfg:@"showToast"] && (!self.diagShown) && (result.count == 0 || [self boolCfg:@"diagnostic"]);
    if (show) {
        self.diagShown = YES;
        SFShowText(@"ISF chẩn đoán: quét nguồn (chụp màn hình gửi tao)", diag);
    } else if (!self.toastedIndex && [self boolCfg:@"showToast"]) {
        SFToast([NSString stringWithFormat:@"ISF: quét xong %lu sub từ %ld nguồn", (unsigned long)result.count, (long)sourcesDone]);
    }
    self.toastedIndex = YES;
    return YES;
}

// ---------------------------------------------------------------- khop + nhap

- (void)prepareSubtitlesForMediaFile:(id)mf {
    NSString *status = [self prepareInternal:mf];
    if (status.length) {
        SFLog(@"[status] %@", status);
        if ([self boolCfg:@"showToast"]) SFToast(status);
    }
}

// Tra ve dong trang thai de hien cho nguoi dung (nil = im lang).
- (NSString *)prepareInternal:(id)mf {
    if (![self boolCfg:@"enabled"]) return nil;
    if (!mf) return @"ISF: hook chạy nhưng không có FCMediaFile";
    if (!SFBool0(mf, @"canHaveExternalSubtitles")) return @"ISF: video này không nhận sub ngoài (canHaveExternalSubtitles = NO)";

    id movie = SFObj0(mf, @"movieItem");
    id itemID = SFObj0(movie, @"itemIDObj");
    NSString *vName = SFStr0(movie, @"fileName");
    if (!movie || !itemID || !vName.length)
        return [NSString stringWithFormat:@"ISF: thiếu thông tin video (movieItem=%d itemID=%d tên=%d)", movie != nil, itemID != nil, vName.length > 0];
    NSString *key = SFStr0(itemID, @"hashString") ?: [itemID description];

    // Infuse goi ham nay nhieu lan khi mo mot video: chi lam 1 lan / 20 giay.
    @synchronized(self) {
        NSDate *last = self.lastPrepared[key];
        if (last && -[last timeIntervalSinceNow] < 20) return nil;
        self.lastPrepared[key] = [NSDate date];
        if (self.lastPrepared.count > 200) [self.lastPrepared removeAllObjects];
    }

    BOOL onMain = [NSThread isMainThread];
    [self startIndexingIfNeeded];
    NSArray<SFSubEntry *> *index;
    @synchronized(self) { index = self.index; }
    if (index.count == 0 && !onMain) {
        NSTimeInterval wait = [self intCfg:@"waitIndexSeconds"];
        dispatch_group_wait(self.crawlGroup, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(wait * NSEC_PER_SEC)));
        @synchronized(self) { index = self.index; }
    }
    if (index.count == 0) {
    {
        BOOL stillCrawling;
        @synchronized(self) { stillCrawling = self.crawling; [self.lastPrepared removeObjectForKey:key]; }
        if (!stillCrawling && self.diagText.length && !self.diagShown) {
            self.diagShown = YES;
            SFShowText(@"ISF chẩn đoán: index sub trống (chụp màn hình gửi tao)", self.diagText);
        }
        return [NSString stringWithFormat:@"ISF: '%@' — index sub trống (%@)", vName, stillCrawling ? @"đang quét, thử lại sau ít giây" : @"quét xong nhưng không thấy sub"];
    }
    }

    id movieParent = SFObj0(movie, @"parent");
    NSString *vFolder = SFTrimSlash(SFStr0(movieParent, @"fileName") ?: @"");
    NSString *vParentPath = SFStr0(movieParent, @"path");
    id movieVFS = SFObj0(movie, @"VFS");
    NSString *movieSource = SFStr0(movieVFS, @"identifier");
    BOOL skipSame = [self boolCfg:@"skipSameFolder"];
    int minScore = (int)[self intCfg:@"minScore"];
    NSArray *langFilter = self.config[@"languages"];

    NSMutableArray<SFCandidate *> *cands = [NSMutableArray array];
    NSDate *t0 = [NSDate date];
    NSInteger nSkipSame = 0, nLow = 0;
    for (SFSubEntry *e in index) {
        if (skipSame && vParentPath && movieSource && [e.sourceId isEqualToString:movieSource] &&
            [e.folderPath isEqualToString:vParentPath]) {
            nSkipSame++;
            continue;
        }
        int sc = sf_score(vName.UTF8String, vFolder.UTF8String, e.name.UTF8String, e.folderName.UTF8String);
        if (sc < minScore) { nLow++; continue; }
        const char *lang = sf_language_from_name(e.name.UTF8String);
        NSString *language = lang ? [NSString stringWithUTF8String:lang] : nil;
        if (langFilter.count > 0 && language && ![langFilter containsObject:language]) continue;
        SFCandidate *c = [[SFCandidate alloc] init];
        c.entry = e;
        c.score = sc;
        c.language = language;
        [cands addObject:c];
    }
    SFLog(@"[match] '%@' (thu muc '%@'): %lu ung vien / %lu sub, %.0fms", vName, vFolder, (unsigned long)cands.count,
          (unsigned long)index.count, -[t0 timeIntervalSinceNow] * 1000);
    if (cands.count == 0) {
        [self showNoMatchDiagForVideo:vName folder:vFolder parentPath:vParentPath source:movieSource index:index skipSame:nSkipSame low:nLow];
        return [NSString stringWithFormat:@"ISF: '%@' — index %lu sub, không sub nào khớp (cùng thư mục: %ld, điểm thấp: %ld)", vName,
                (unsigned long)index.count, (long)nSkipSame, (long)nLow];
    }

    [cands sortUsingComparator:^NSComparisonResult(SFCandidate *a, SFCandidate *b) {
        if (a.score != b.score) return a.score > b.score ? NSOrderedAscending : NSOrderedDescending;
        // cung diem: uu tien ngon ngu nam som trong danh sach `languages`
        NSUInteger ia = a.language ? [langFilter indexOfObject:a.language] : NSNotFound;
        NSUInteger ib = b.language ? [langFilter indexOfObject:b.language] : NSNotFound;
        if (ia != ib) return ia < ib ? NSOrderedAscending : NSOrderedDescending;
        return [a.entry.name compare:b.entry.name];
    }];

    NSInteger maxPer = MAX(1, [self intCfg:@"maxPerVideo"]);
    if ((NSInteger)cands.count > maxPer) [cands removeObjectsInRange:NSMakeRange((NSUInteger)maxPer, cands.count - (NSUInteger)maxPer)];

    id cache = SFObj0(NSClassFromString(@"FCSubtitlesCache"), @"shared");
    if (!cache) {
        return @"ISF: không tìm thấy FCSubtitlesCache (Infuse đổi API?)";
    }

    // Sub da co san trong cache cua video nay (theo ten hien thi) thi bo qua.
    NSMutableSet *existing = [NSMutableSet set];
    SEL cachedSel = NSSelectorFromString(@"cachedSubtitlesForItemID:");
    if ([cache respondsToSelector:cachedSel]) {
        id arr = ((id (*)(id, SEL, id))objc_msgSend)(cache, cachedSel, itemID);
        if ([arr isKindOfClass:[NSArray class]]) {
            for (id it in (NSArray *)arr) {
                NSString *n = SFStr0(it, @"userFriendlyName");
                if (n) [existing addObject:n];
            }
        }
    }

    dispatch_group_t done = dispatch_group_create();
    __block NSInteger imported = 0;
    for (SFCandidate *c in cands) {
        if ([existing containsObject:c.entry.name]) continue;
        NSString *flightKey = [NSString stringWithFormat:@"%@|%@|%@", key, c.entry.sourceId, c.entry.name];
        @synchronized(self) {
            if ([self.inFlight containsObject:flightKey]) continue;
            [self.inFlight addObject:flightKey];
        }
        dispatch_group_enter(done);
        dispatch_async(self.importQueue, ^{
            BOOL ok = NO;
            @try {
                @autoreleasepool { ok = [self importCandidate:c itemID:itemID cache:cache]; }
            } @catch (NSException *e) {
                SFLog(@"[import] loi: %@", e);
            }
            @synchronized(self) {
                [self.inFlight removeObject:flightKey];
                if (ok) imported++;
            }
            dispatch_group_leave(done);
        });
    }

    // Tren main thread thi khong cho (tranh treo UI); sub se co o lan mo sau.
    if (!onMain) {
        NSTimeInterval to = [self intCfg:@"downloadTimeoutSeconds"];
        long r = dispatch_group_wait(done, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(to * NSEC_PER_SEC)));
        if (r != 0) SFLog(@"[import] het %.0fs, tiep tuc phat; phan con lai tai nen", to);
    }
    NSInteger n;
    @synchronized(self) { n = imported; }
    if (n > 0) {
        SEL inv = NSSelectorFromString(@"invalidateExternalSubtitlesCache");
        if ([mf respondsToSelector:inv]) ((void (*)(id, SEL))objc_msgSend)(mf, inv);
        SFLog(@"[import] da nap %ld sub cho '%@'", (long)n, vName);
        return [NSString stringWithFormat:@"ISF: đã nạp %ld sub cho '%@' (khớp %lu)", (long)n, vName, (unsigned long)cands.count];
    }
    return [NSString stringWithFormat:@"ISF: '%@' khớp %lu sub nhưng nạp 0 (đã có sẵn hoặc tải lỗi), xem log", vName, (unsigned long)cands.count];
}

- (void)showNoMatchDiagForVideo:(NSString *)vName folder:(NSString *)vFolder parentPath:(NSString *)vParentPath
                        source:(NSString *)movieSource index:(NSArray<SFSubEntry *> *)index skipSame:(NSInteger)nSkip low:(NSInteger)nLow {
    static NSInteger shownCount = 0;
    if (shownCount >= 3 && ![self boolCfg:@"diagnostic"]) return;
    shownCount++;
    char buf[320];
    int vs = -1, ve = -1;
    sf_describe(vName.UTF8String, buf, sizeof buf);
    sf_parse_se(vName.UTF8String, &vs, &ve);
    NSMutableString *d = [NSMutableString string];
    [d appendFormat:@"VIDEO: %@\n  thu muc: %@\n  duong dan thu muc: %@\n  nguon: %@\n  phan tich: %s\n\n", vName, vFolder, vParentPath, movieSource, buf];
    [d appendFormat:@"INDEX %lu sub | bo qua vi cung thu muc: %ld | diem < %ld: %ld\n\n", (unsigned long)index.count, (long)nSkip,
     (long)[self intCfg:@"minScore"], (long)nLow];
    // uu tien liet ke sub cung tap voi video, roi toi cac sub dau tien
    NSMutableArray *ordered = [NSMutableArray array], *rest = [NSMutableArray array];
    for (SFSubEntry *e in index) {
        int s = -1, ep = -1;
        sf_parse_se(e.name.UTF8String, &s, &ep);
        if (ep >= 0 && ep == ve) [ordered addObject:e]; else [rest addObject:e];
    }
    [ordered addObjectsFromArray:rest];
    NSUInteger shown = 0;
    for (SFSubEntry *e in ordered) {
        if (shown++ >= 10) break;
        sf_describe(e.name.UTF8String, buf, sizeof buf);
        [d appendFormat:@"- %@\n    thu muc: %@ | nguon: %@ (id %@)\n    duong dan thu muc: %@\n    phan tich: %s | diem: %d\n", e.name, e.folderName,
         e.sourceName, e.sourceId, e.folderPath, buf, sf_score(vName.UTF8String, vFolder.UTF8String, e.name.UTF8String, e.folderName.UTF8String)];
    }
    SFLog(@"[diag] khong khop:\n%@", d);
    if ([self boolCfg:@"showToast"]) SFShowText(@"ISF chẩn đoán: không sub nào khớp (chụp màn hình gửi tao)", d);
}

- (BOOL)importCandidate:(SFCandidate *)c itemID:(id)itemID cache:(id)cache {
    NSString *dir = SFStr0(cache, @"cachePath");
    if (!dir.length) { SFLog(@"[import] cache khong co cachePath"); return NO; }
    NSFileManager *fm = [NSFileManager defaultManager];
    [fm createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *local = [dir stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.%@", [NSUUID UUID].UUIDString, c.entry.ext]];

    Class net = NSClassFromString(@"FCNetworkUtils");
    SEL copySel = NSSelectorFromString(@"copyRemoteItem:toLocalFilePath:");
    if (!net || ![net respondsToSelector:copySel]) { SFLog(@"[import] khong co FCNetworkUtils copyRemoteItem"); return NO; }
    NSDate *t0 = [NSDate date];
    BOOL ok = ((BOOL (*)(id, SEL, id, id))objc_msgSend)(net, copySel, c.entry.item, local);
    unsigned long long size = [[fm attributesOfItemAtPath:local error:nil] fileSize];
    if (!ok || size == 0 || size > 8 * 1024 * 1024) {
        SFLog(@"[import] tai '%@' that bai (ok=%d, size=%llu)", c.entry.name, ok, size);
        [fm removeItemAtPath:local error:nil];
        return NO;
    }

    // Y het luong "chia se file sub" cua Infuse (language = nil).
    SEL saveSel = NSSelectorFromString(@"saveSubtitlesForItemID:subtitlesPath:language:name:date:");
    if (![cache respondsToSelector:saveSel]) { SFLog(@"[import] cache khong co saveSubtitles..."); return NO; }
    ((id (*)(id, SEL, id, id, id, id, id))objc_msgSend)(cache, saveSel, itemID, local, nil, c.entry.name, [NSDate date]);
    SFLog(@"[import] + %@ [%@] diem=%d %llu byte, %.1fs (nguon '%@')", c.entry.name, c.language ?: @"?", c.score, size,
          -[t0 timeIntervalSinceNow], c.entry.sourceName);
    return YES;
}

@end
