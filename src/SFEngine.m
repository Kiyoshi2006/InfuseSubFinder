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

void SFToast(NSString *text) {
    if (!text.length) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        UIWindow *win = nil;
        for (UIScene *sc in [UIApplication sharedApplication].connectedScenes) {
            if (![sc isKindOfClass:[UIWindowScene class]]) continue;
            for (UIWindow *w in ((UIWindowScene *)sc).windows) {
                if (w.isKeyWindow) { win = w; break; }
            }
            if (win) break;
        }
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
        @"minScore": @60,
        @"maxPerVideo": @4,
        @"languages": @[],                // vd ["Vietnamese","English"]; rong = tat ca
        @"extensions": @[ @"srt", @"ass", @"ssa", @"vtt" ],
        @"maxDepth": @8,
        @"maxFoldersPerSource": @4000,
        @"sourceBudgetSeconds": @180,
        @"rescanMinutes": @30,
        @"waitIndexSeconds": @4,
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
        @try {
            @autoreleasepool { [self crawlAll]; }
        } @catch (NSException *e) {
            SFLog(@"[index] loi: %@", e);
        }
        @synchronized(self) {
            self.crawling = NO;
            self.lastCrawl = [NSDate date];
        }
        dispatch_group_leave(self.crawlGroup);
    });
}

- (void)crawlAll {
    Class mgrCls = NSClassFromString(@"VFSManager");
    id mgr = SFObj0(mgrCls, @"sharedManager");
    NSArray *all = SFArr0(mgr, @"allVFSs");
    if (!mgr || !all) {
        SFLog(@"[index] khong lay duoc VFSManager/allVFSs (Infuse doi API?)");
        SFToast(@"ISF: không lấy được danh sách nguồn của Infuse (VFSManager)");
        return;
    }
    [self loadConfig];
    NSDate *t0 = [NSDate date];
    NSMutableArray<SFSubEntry *> *result = [NSMutableArray array];
    NSSet *exts = [NSSet setWithArray:(NSArray *)self.config[@"extensions"]];
    NSSet *excludeFolders = [NSSet setWithArray:(NSArray *)self.config[@"excludeFolders"]];
    NSInteger maxDepth = [self intCfg:@"maxDepth"];
    NSInteger maxFolders = [self intCfg:@"maxFoldersPerSource"];
    NSTimeInterval budget = [self intCfg:@"sourceBudgetSeconds"];

    NSInteger sourcesDone = 0;
    for (id vfs in all) {
        if (![self shouldCrawlVFS:vfs manager:mgr]) continue;
        sourcesDone++;
        NSString *srcName = SFStr0(vfs, @"name") ?: NSStringFromClass([vfs class]);
        NSString *srcId = SFStr0(vfs, @"identifier") ?: srcName;
        id root = SFObj0(vfs, @"makeRootItem");
        id browser = SFObj0(vfs, @"browser");
        SEL fetchSel = NSSelectorFromString(@"fetchChildrenForItem:outError:");
        if (!root || !browser || ![browser respondsToSelector:fetchSel]) {
            SFLog(@"[index] bo qua nguon '%@' (khong co root/browser)", srcName);
            continue;
        }

        NSDate *srcStart = [NSDate date];
        NSMutableArray *queue = [NSMutableArray arrayWithObject:@[ root, @0 ]];
        NSUInteger head = 0;
        NSInteger folders = 0, files = 0, subs = 0, failed = 0;
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
            }
            if (!ok) { failed++; continue; }

            id lockObj = SFObj0(folder, @"childItemsLockObject");
            NSArray *children;
            @synchronized(lockObj ?: self) { children = [SFArr0(folder, @"childItems") copy]; }

            NSString *folderName = SFTrimSlash(SFStr0(folder, @"fileName") ?: @"");
            NSString *folderPath = SFStr0(folder, @"path") ?: @"";
            for (id child in children) {
                NSString *name = SFStr0(child, @"fileName");
                if (!name.length) continue;
                if ([self isFolderItem:child]) {
                    if (depth + 1 > maxDepth) continue;
                    if ([excludeFolders containsObject:SFTrimSlash(name)]) continue;
                    [queue addObject:@[ child, @(depth + 1) ]];
                    continue;
                }
                files++;
                NSString *ext = [(SFStr0(child, @"fileExtension") ?: name.pathExtension) lowercaseString];
                if (![exts containsObject:ext] || !sf_is_subtitle_ext(ext.UTF8String)) continue;
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
        SFLog(@"[index] '%@' (%@): %ld thu muc, %ld file, %ld sub, %ld loi, %.1fs", srcName, NSStringFromClass([vfs class]),
              (long)folders, (long)files, (long)subs, (long)failed, -[srcStart timeIntervalSinceNow]);
    }

    @synchronized(self) { self.index = result; self.sourcesCrawled = sourcesDone; }
    SFLog(@"[index] xong: %lu sub, %ld nguon, %.1fs", (unsigned long)result.count, (long)sourcesDone, -[t0 timeIntervalSinceNow]);
    if (!self.toastedIndex && [self boolCfg:@"showToast"]) {
        self.toastedIndex = YES;
        SFToast([NSString stringWithFormat:@"ISF: quét xong %lu sub từ %ld nguồn (tổng %lu VFS)", (unsigned long)result.count,
                 (long)sourcesDone, (unsigned long)all.count]);
    }
    if (result.count == 0) {
        SFLog(@"[index] khong thay sub nao. Neu m chac chan co sub trong nguon da them thi gui log nay cho tao.");
    }
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
        return [NSString stringWithFormat:@"ISF: '%@' — chưa có index sub (đang quét hoặc quét lỗi), xem log", vName];
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
    for (SFSubEntry *e in index) {
        if (skipSame && vParentPath && movieSource && [e.sourceId isEqualToString:movieSource] &&
            [e.folderPath isEqualToString:vParentPath]) {
            continue;
        }
        int sc = sf_score(vName.UTF8String, vFolder.UTF8String, e.name.UTF8String, e.folderName.UTF8String);
        if (sc < minScore) continue;
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
    if (cands.count == 0)
        return [NSString stringWithFormat:@"ISF: '%@' — index %lu sub, không sub nào khớp tên", vName, (unsigned long)index.count];

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
