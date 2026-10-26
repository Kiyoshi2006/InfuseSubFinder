// SFChapters.inc — dung chuong (chapter) trong file video lam doan intro/recap/preview/outro cho InfusePlus.
// Duoc #include o cuoi SFEngine.m.
//
// Cach hoat dong:
//  * InfusePlus giu du lieu skip trong [INFPIntroManager sharedManager].intros[key] = INFPMediaIntro
//    (4 o: intro / recap / preview / outro, moi o la INFPMediaIntroSegment {identifier, start, end} tinh bang giay).
//  * Moi nhip phat, InfusePlus goi -[INFPIntroController skipIntroIfNeeded:withAsset:]. Ta hook ham do:
//    doc danh sach chuong cua player, doi chieu ten chuong voi ten nguoi dung nhap, ghi vao INFPMediaIntro
//    roi bao InfusePlus ve lai thanh tien trinh. Phan nut / tu dong skip / hoi truoc khi skip la cua InfusePlus.
//  * Trang "Intros" cua InfusePlus duoc chen them 1 dong "Chapters" duoi moi loai de nhap ten chuong.

static NSArray<NSString *> *SFSlots(void) { return @[ @"intro", @"recap", @"preview", @"outro" ]; }

static NSString *SFDefaultNames(NSString *slot) {
    if ([slot isEqualToString:@"intro"])   return @"Opening, OP, Intro";
    if ([slot isEqualToString:@"recap"])   return @"Recap, Previously";
    if ([slot isEqualToString:@"preview"]) return @"Preview, Next";
    return @"Ending, ED, Credits, Outro";
}
static NSString *SFNamesFor(NSString *slot) {
    NSString *k = [@"sf.chapters." stringByAppendingString:slot];
    id v = [[NSUserDefaults standardUserDefaults] objectForKey:k];
    return [v isKindOfClass:[NSString class]] ? v : SFDefaultNames(slot);
}
static void SFSetNames(NSString *slot, NSString *v) {
    [[NSUserDefaults standardUserDefaults] setObject:v ?: @"" forKey:[@"sf.chapters." stringByAppendingString:slot]];
}
static NSArray<NSString *> *SFSplitNames(NSString *s) {
    NSMutableArray *out = [NSMutableArray array];
    NSCharacterSet *sep = [NSCharacterSet characterSetWithCharactersInString:@",;|\n"];
    for (NSString *p in [s componentsSeparatedByCharactersInSet:sep]) {
        NSString *t = [[p stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] lowercaseString];
        if (t.length) [out addObject:t];
    }
    return out;
}
// ten chuong khop neu bang ten da nhap, hoac bat dau bang ten do va ky tu ke tiep khong phai chu cai ("OP 2", "Opening Theme", "OP1")
static BOOL SFTitleMatches(NSString *title, NSArray<NSString *> *names) {
    NSString *t = [[title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] lowercaseString];
    if (!t.length) return NO;
    for (NSString *n in names) {
        if ([t isEqualToString:n]) return YES;
        if ([t hasPrefix:n] && t.length > n.length) {
            unichar c = [t characterAtIndex:n.length];
            if (![[NSCharacterSet letterCharacterSet] characterIsMember:c]) return YES;
        }
    }
    return NO;
}

static double SFD(id obj, NSString *sel) {
    SEL s = NSSelectorFromString(sel);
    if (!obj || ![obj respondsToSelector:s]) return 0;
    return ((double (*)(id, SEL))objc_msgSend)(obj, s);
}

static __weak id sf_lastHelper;
static NSArray *SFCurrentChapters(id *helperOut) {
    Class PH = NSClassFromString(@"PlayerHelper");
    id helper = PH && [PH respondsToSelector:NSSelectorFromString(@"currentPlayer")] ? ((id (*)(id, SEL))objc_msgSend)(PH, NSSelectorFromString(@"currentPlayer")) : nil;
    if (!helper) helper = sf_lastHelper;
    if (helperOut) *helperOut = helper;
    if (!helper) return nil;
    NSArray *list = nil;
    id ds = SFObj0(helper, @"settingsDataSource");
    if (ds && [ds respondsToSelector:NSSelectorFromString(@"chaptersList")]) list = SFObj0(ds, @"chaptersList");
    if (![list isKindOfClass:[NSArray class]] || !list.count) {
        id vc = SFObj0(helper, @"playbackController");
        id st = SFObj0(vc, @"playbackStrategy");
        if (!st) { id raw = [helper valueForKey:@"_player"]; st = SFObj0(raw, @"playbackStrategy"); }
        list = SFObj0(st, @"chaptersList");
    }
    return [list isKindOfClass:[NSArray class]] ? list : nil;
}

static const void *kSFChapSigKey = &kSFChapSigKey;
static const void *kSFDBKey = &kSFDBKey;
static const void *kSFSlotKey = &kSFSlotKey;

// 0 = uu tien chuong trong video (local), 1 = uu tien du lieu mang
static NSInteger SFPriority(void) { return [[NSUserDefaults standardUserDefaults] integerForKey:@"sf.chapters.priority"]; }

// ---- video chua khop metadata: tao "asset" gia de InfusePlus van chay
@interface SFFakeAsset : NSObject
@property (nonatomic, copy) NSString *tmdbID, *imdbID, *tvdbID;
@property (nonatomic, strong) NSNumber *seasonNumber, *episodeNumber;
@end
@implementation SFFakeAsset
@end
static NSString *sf_fakeKey;          // khoa dang dung cho video khong co metadata (nil neu video co metadata)
static id sf_fakeAsset;

// Ghi doan tu chuong vao INFPMediaIntro cua video hien tai, gop voi du lieu mang theo muc uu tien.
static void SFApplyChapters(id asset) {
    if (!asset) return;
    static NSString *lastLogged;
    Class MgrC = NSClassFromString(@"INFPIntroManager"), IntroC = NSClassFromString(@"INFPMediaIntro"), SegC = NSClassFromString(@"INFPMediaIntroSegment");
    if (!MgrC || !IntroC || !SegC) return;
    id mgr = ((id (*)(id, SEL))objc_msgSend)(MgrC, NSSelectorFromString(@"sharedManager"));
    NSString *key = ((id (*)(id, SEL, id))objc_msgSend)(mgr, NSSelectorFromString(@"keyForAsset:"), asset);
    if (!key.length) return;
    id helper = nil;
    NSArray *chs = SFCurrentChapters(&helper);
    if (!chs.count) {
        static NSString *noChapLogged;
        if (![noChapLogged isEqualToString:key]) { noChapLogged = [key copy]; SFLog(@"[chap] khong doc duoc chuong (key=%@, helper=%@)", key, helper ? @"co" : @"khong"); }
        return;
    }          // tam thoi khong doc duoc chuong -> giu nguyen, khong xoa doan da ap dung (tranh nhap nhay)
    double duration = SFD(helper, @"duration");

    NSMutableDictionary<NSString *, NSArray *> *found = [NSMutableDictionary dictionary];   // slot -> @[start, end, title]
    NSMutableString *dump = [NSMutableString string], *times = [NSMutableString string];
    for (NSUInteger i = 0; i < chs.count; i++) {
        id c = chs[i];
        NSString *title = SFStr0(c, @"title") ?: @"";
        double st = SFD(c, @"startTime"), en = SFD(c, @"endTime");
        if (en <= st) en = (i + 1 < chs.count) ? SFD(chs[i + 1], @"startTime") : duration;
        [dump appendFormat:@"[%@ %.0f-%.0f] ", title, st, en];
        [times appendFormat:@"%.0f,", st];
        if (en <= st) continue;
        for (NSString *slot in SFSlots()) {
            if (found[slot]) continue;
            if (SFTitleMatches(title, SFSplitNames(SFNamesFor(slot)))) found[slot] = @[ @(st), @(en), title ];
        }
    }

    NSMutableDictionary *intros = SFObj0(mgr, @"intros");
    if (![intros respondsToSelector:@selector(objectForKey:)]) return;
    id cur = intros[key];
    NSString *curSig = cur ? objc_getAssociatedObject(cur, kSFChapSigKey) : nil;
    id db = curSig ? objc_getAssociatedObject(cur, kSFDBKey) : cur;       // du lieu mang goc (neu co)
    if ([db isKindOfClass:[NSNull class]]) db = nil;

    NSMutableString *names = [NSMutableString string];
    for (NSString *s in SFSlots()) [names appendFormat:@"%@=%@;", s, SFNamesFor(s)];
    NSString *sig = [NSString stringWithFormat:@"%@|%@|%@|p%ld|%p|%d", key, times, names, (long)SFPriority(), db, db ? (int)SFBool0(db, @"hasData") : -1];
    if (curSig && [curSig isEqualToString:sig]) return;                    // da ap dung, khong doi gi

    if (![lastLogged isEqualToString:sig]) {
        lastLogged = [sig copy];
        SFLog(@"[chap] %lu chuong: %@ => khop: %@ | uu tien %@ | du lieu mang: %@", (unsigned long)chs.count, dump, found.count ? found : @"(khong)",
              SFPriority() ? @"mang" : @"local", db ? @"co" : @"khong");
    }

    BOOL changed = NO;
    if (!found.count) {
        if (curSig) { if (db) intros[key] = db; else [intros removeObjectForKey:key]; changed = YES; }   // bo phan chuong da ghi truoc do
    } else {
        id merged = ((id (*)(id, SEL))objc_msgSend)([IntroC alloc], @selector(init));
        NSDictionary *getters = @{ @"intro": @"intro", @"recap": @"recap", @"preview": @"preview", @"outro": @"outro" };
        NSDictionary *setters = @{ @"intro": @"setIntro:", @"recap": @"setRecap:", @"preview": @"setPreview:", @"outro": @"setOutro:" };
        BOOL any = NO;
        for (NSString *slot in SFSlots()) {
            id dbSeg = db ? SFObj0(db, getters[slot]) : nil;
            id pick = nil;
            if (found[slot] && !(SFPriority() == 1 && dbSeg)) {
                pick = ((id (*)(id, SEL))objc_msgSend)([SegC alloc], @selector(init));
                ((void (*)(id, SEL, double))objc_msgSend)(pick, NSSelectorFromString(@"setStart:"), [found[slot][0] doubleValue]);
                ((void (*)(id, SEL, double))objc_msgSend)(pick, NSSelectorFromString(@"setEnd:"), [found[slot][1] doubleValue]);
                ((void (*)(id, SEL, id))objc_msgSend)(pick, NSSelectorFromString(@"setIdentifier:"), [@"segment_" stringByAppendingString:slot]);
            } else pick = dbSeg;
            if (pick) { ((void (*)(id, SEL, id))objc_msgSend)(merged, NSSelectorFromString(setters[slot]), pick); any = YES; }
        }
        if (db) ((void (*)(id, SEL, unsigned long))objc_msgSend)(merged, NSSelectorFromString(@"setSource:"), ((unsigned long (*)(id, SEL))objc_msgSend)(db, NSSelectorFromString(@"source")));
        ((void (*)(id, SEL, BOOL))objc_msgSend)(merged, NSSelectorFromString(@"setHasData:"), any);
        objc_setAssociatedObject(merged, kSFChapSigKey, sig, OBJC_ASSOCIATION_COPY_NONATOMIC);
        objc_setAssociatedObject(merged, kSFDBKey, db ?: (id)[NSNull null], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        intros[key] = merged;
        changed = YES;
    }
    if (changed) dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:@"INFPIntroSegmentsDidLoad" object:nil];
        [[NSNotificationCenter defaultCenter] postNotificationName:@"INFPIntroSegmentsDraw" object:nil];   // thanh tien trinh ve lai
    });
}

static void (*sf_orig_skip)(id, SEL, double, id);
static float sf_askOverride = 0;
static BOOL sf_inAsk = NO;
static __weak id sf_heldToast;
static double sf_heldStart, sf_heldEnd;
static void (*sf_orig_toast)(id, SEL, id, double, id, id);
static void sf_hooked_toast(id self, SEL _cmd, id text, double dur, id title, id block) {
    sf_orig_toast(self, _cmd, text, dur, title, block);
    if (sf_inAsk) sf_heldToast = self;
}
static void SFHideHeldToast(void) {
    id t = sf_heldToast; sf_heldToast = nil;
    if (t && [t respondsToSelector:NSSelectorFromString(@"hideImmediately")]) {
        dispatch_async(dispatch_get_main_queue(), ^{ ((void (*)(id, SEL))objc_msgSend)(t, NSSelectorFromString(@"hideImmediately")); });
    }
}

// Tua lai vao doan da di qua: xoa danh dau "da xu ly" khi thoi diem roi khoi doan -> lan vao lai se hien nut skip tiep.
static NSMutableSet *sf_inside;
static __weak id sf_lastItem;
static BOOL sf_lastOn = YES;
static void SFReshowTick(id ctl, double t, id asset) {
    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"sf.reshow"]) return;
    Class MgrC = NSClassFromString(@"INFPIntroManager");
    id mgr = MgrC ? ((id (*)(id, SEL))objc_msgSend)(MgrC, NSSelectorFromString(@"sharedManager")) : nil;
    id intro = mgr ? ((id (*)(id, SEL, id))objc_msgSend)(mgr, NSSelectorFromString(@"introForAsset:"), asset) : nil;
    NSArray *segs = intro ? SFObj0(intro, @"segments") : nil;
    NSMutableSet *skipped = [ctl valueForKey:@"_skippedSegments"];
    if (![segs isKindOfClass:[NSArray class]] || ![skipped isKindOfClass:[NSMutableSet class]]) return;
    if (!sf_inside) sf_inside = [NSMutableSet set];
    for (id seg in segs) {
        NSString *uuid = SFStr0(seg, @"uuid");
        if (!uuid.length) continue;
        double st = SFD(seg, @"start");
        double en = ((double (*)(id, SEL, id))objc_msgSend)(ctl, NSSelectorFromString(@"effectiveEndForSegment:"), seg);
        BOOL inside = t >= st && t < en;
        if (inside) [sf_inside addObject:uuid];
        else if ([sf_inside containsObject:uuid]) { [sf_inside removeObject:uuid]; [skipped removeObject:uuid]; }
    }
}

static void sf_hooked_skip(id self, SEL _cmd, double t, id asset) {
    @try { SFApplyChapters(asset); } @catch (NSException *e) { SFLog(@"[chap] loi: %@", e); }
    @try {
        if (sf_heldToast && (t < sf_heldStart - 0.5 || t >= sf_heldEnd)) SFHideHeldToast();   // roi khoi doan -> bo nut skip dang giu
        SFReshowTick(self, t, asset);
    } @catch (NSException *e) { SFLog(@"[chap] loi reshow: %@", e); }
    sf_orig_skip(self, _cmd, t, asset);
}

// "Skip Alert: giu den het doan": thoi luong thong bao Ask = phan con lai cua doan
static float (*sf_orig_floatForKey)(id, SEL, id);
static float sf_hooked_floatForKey(id self, SEL _cmd, id key) {
    if (sf_askOverride > 0 && [key isKindOfClass:[NSString class]] && [(NSString *)key isEqualToString:@"skipAlertDuration"]) return sf_askOverride;
    return sf_orig_floatForKey(self, _cmd, key);
}

// Sua loi cua InfusePlus: menu luu Ask = 3, Auto-skip = 2 nhung ham xu ly lai hieu 3 = tu dong skip, 2 = hoi.
static void (*sf_orig_handle)(id, SEL, id, double);
static void sf_hooked_handle(id self, SEL _cmd, id seg, double t) {
    @try {
        Class UD = NSClassFromString(@"INFUserDefaults");
        NSString *ident = SFStr0(seg, @"identifier");
        if (UD && ident.length) {
            long mode = ((long (*)(id, SEL, id))objc_msgSend)(UD, NSSelectorFromString(@"intForKey:"), ident);
            if (mode == 2) { ((void (*)(id, SEL, id))objc_msgSend)(self, NSSelectorFromString(@"autoSkip:"), seg); return; }
            if (mode == 3) {
                if (sf_orig_floatForKey && [[NSUserDefaults standardUserDefaults] boolForKey:@"sf.skipinf"]) {
                    double end = SFD(seg, @"end");
                    if (end <= 0) { id h = sf_lastHelper; end = SFD(h, @"duration"); }
                    sf_askOverride = (float)MAX(2.0, end - t);
                }
                sf_inAsk = YES;
                sf_heldStart = SFD(seg, @"start");
                { double e2 = SFD(seg, @"end"); if (e2 <= 0) e2 = SFD(sf_lastHelper, @"duration"); sf_heldEnd = e2; }
                ((void (*)(id, SEL, id))objc_msgSend)(self, NSSelectorFromString(@"askBeforeSkipping:"), seg);
                sf_inAsk = NO;
                sf_askOverride = 0;
                return;
            }
        }
    } @catch (NSException *e) { SFLog(@"[chap] loi mode: %@", e); }
    sf_orig_handle(self, _cmd, seg, t);
}

// Video chua khop metadata: PlayerHelper bo qua het -> tu goi controller voi asset gia.
static void (*sf_orig_ph_skip)(id, SEL, double);
static id (*sf_orig_keyfor)(id, SEL, id);
static void sf_hooked_ph_skip(id self, SEL _cmd, double t) {
    sf_lastHelper = self;
    {   // doi tap (bam "tap tiep theo" / het video) -> bo nut skip cu + reset trang thai
        id mi0 = SFObj0(self, @"mediaItem");
        if (mi0 != sf_lastItem) {
            sf_lastItem = mi0;
            SFHideHeldToast();
            [sf_inside removeAllObjects];
        }
        // nut (i) bat lai -> cho phep hien lai nut skip cua doan dang o trong
        Class UD1 = NSClassFromString(@"INFUserDefaults");
        BOOL on = UD1 ? ((BOOL (*)(id, SEL, id))objc_msgSend)(UD1, NSSelectorFromString(@"boolForKey:"), @"skipIntros") : YES;
        if (on && !sf_lastOn) {
            id ic0 = SFObj0(self, @"introController");
            NSMutableSet *sk = ic0 ? [ic0 valueForKey:@"_skippedSegments"] : nil;
            if ([sk isKindOfClass:[NSMutableSet class]]) [sk removeAllObjects];
        }
        sf_lastOn = on;
    }
    {   // nut (i) tat "Skip Intros" -> an luon nut skip dang giu tren man hinh
        Class UD0 = NSClassFromString(@"INFUserDefaults");
        if (sf_heldToast && UD0 && !((BOOL (*)(id, SEL, id))objc_msgSend)(UD0, NSSelectorFromString(@"boolForKey:"), @"skipIntros")) SFHideHeldToast();
    }
    sf_orig_ph_skip(self, _cmd, t);
    @try {
        id mi = SFObj0(self, @"mediaItem");
        if (!mi) { sf_fakeKey = nil; return; }
        id md = SFObj0(mi, @"metadata");
        Class MgrC = NSClassFromString(@"INFPIntroManager");
        id mgr = MgrC ? ((id (*)(id, SEL))objc_msgSend)(MgrC, NSSelectorFromString(@"sharedManager")) : nil;
        NSString *realKey = (md && mgr && sf_orig_keyfor) ? sf_orig_keyfor(mgr, NSSelectorFromString(@"keyForAsset:"), md) : nil;
        if (realKey.length) { sf_fakeKey = nil; return; }           // co ID metadata -> InfusePlus tu goi, hook skip lo phan chuong
        // khong co metadata, hoac metadata khong co tmdb/imdb/tvdb ID -> InfusePlus bo qua; ta tu goi voi asset gia
        Class UD = NSClassFromString(@"INFUserDefaults");
        if (!UD || !((BOOL (*)(id, SEL, id))objc_msgSend)(UD, NSSelectorFromString(@"boolForKey:"), @"skipIntros")) return;
        id ic = SFObj0(self, @"introController");
        if (!ic) return;
        NSString *fid = [NSString stringWithFormat:@"sf-%p", mi];
        if (!sf_fakeAsset || ![[(SFFakeAsset *)sf_fakeAsset tmdbID] isEqualToString:fid]) {
            SFFakeAsset *fa = [SFFakeAsset new];
            fa.tmdbID = fid;
            sf_fakeAsset = fa;
        }
        if (![sf_fakeKey isEqualToString:fid]) SFLog(@"[chap] video khong co ID metadata (metadata=%@) -> dung asset gia %@", md ? @"co, thieu ID" : @"khong", fid);
        sf_fakeKey = fid;
        ((void (*)(id, SEL, double, id))objc_msgSend)(ic, NSSelectorFromString(@"skipIntroIfNeeded:withAsset:"), t, sf_fakeAsset);
    } @catch (NSException *e) { SFLog(@"[chap] loi asset gia: %@", e); }
}

// Ve thanh tien trinh cung hoi metadata (nil) -> tra khoa gia
static id sf_hooked_keyfor(id self, SEL _cmd, id asset) {
    id r = sf_orig_keyfor(self, _cmd, asset);
    if (![(NSString *)r length] && sf_fakeKey) return sf_fakeKey;
    return r;
}

// ---- dong nhap ten chuong trong trang Intros cua InfusePlus

@interface SFChapField : NSObject <UITextFieldDelegate>
@end
@implementation SFChapField
+ (instancetype)shared { static SFChapField *s; static dispatch_once_t o; dispatch_once(&o, ^{ s = [SFChapField new]; }); return s; }
- (void)changed:(UITextField *)tf { NSString *slot = objc_getAssociatedObject(tf, kSFSlotKey); if (slot) SFSetNames(slot, tf.text); }
- (void)toggled:(UISwitch *)sw { NSString *k = objc_getAssociatedObject(sw, kSFSlotKey); if (k) [[NSUserDefaults standardUserDefaults] setBool:sw.on forKey:k]; }
- (BOOL)textFieldShouldReturn:(UITextField *)tf { [tf resignFirstResponder]; return YES; }
@end

static UITableViewCell *(*sf_orig_ics_cell)(id, SEL, UITableView *, NSIndexPath *);
static UITableViewCell *sf_hooked_ics_cell(id self, SEL _cmd, UITableView *tv, NSIndexPath *ip) {
    UITableViewCell *cell = sf_orig_ics_cell(self, _cmd, tv, ip);
    @try {
        NSArray *secs = [self valueForKey:@"_sections"];
        id row = (ip.section < (NSInteger)secs.count && ip.row < (NSInteger)[secs[ip.section] count]) ? secs[ip.section][ip.row] : nil;
        NSString *slot = [row isKindOfClass:[NSDictionary class]] ? row[@"sfslot"] : nil;
        BOOL isPrio = [row isKindOfClass:[NSDictionary class]] && [row[@"sfprio"] boolValue];
        NSString *swKey = [row isKindOfClass:[NSDictionary class]] ? row[@"sfswitchkey"] : nil;
        if (swKey) {
            UISwitch *sw = [[UISwitch alloc] init];
            sw.on = [[NSUserDefaults standardUserDefaults] boolForKey:swKey];
            sw.onTintColor = tv.tintColor;
            objc_setAssociatedObject(sw, kSFSlotKey, swKey, OBJC_ASSOCIATION_COPY_NONATOMIC);
            [sw addTarget:[SFChapField shared] action:@selector(toggled:) forControlEvents:UIControlEventValueChanged];
            sw.tag = 0x5F1;
            cell.accessoryView = sw;
            cell.selectionStyle = UITableViewCellSelectionStyleNone;
        } else if (isPrio) {
            UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
            void (^refresh)(UIButton *) = ^(UIButton *btn) {
                NSString *t = SFPriority() == 1 ? @"Network " : @"Local ";
                [btn setTitle:t forState:UIControlStateNormal];
                [btn setImage:[UIImage systemImageNamed:@"chevron.up.chevron.down"] forState:UIControlStateNormal];
                btn.semanticContentAttribute = UISemanticContentAttributeForceRightToLeft;
                btn.titleLabel.font = [UIFont systemFontOfSize:17];
                [btn sizeToFit];
            };
            __weak UIButton *wb = b;
            NSArray *titles = @[ @"Local (video chapters)", @"Network (TheIntroDB)" ];
            NSMutableArray *acts = [NSMutableArray array];
            for (NSInteger i = 0; i < 2; i++) {
                UIAction *a = [UIAction actionWithTitle:titles[i] image:nil identifier:nil handler:^(UIAction *act) {
                    [[NSUserDefaults standardUserDefaults] setInteger:i forKey:@"sf.chapters.priority"];
                    UIButton *bb = wb; if (bb) refresh(bb);
                }];
                a.state = (SFPriority() == i) ? UIMenuElementStateOn : UIMenuElementStateOff;
                [acts addObject:a];
            }
            b.tag = 0x5F1;
            b.menu = [UIMenu menuWithTitle:@"" children:acts];
            b.showsMenuAsPrimaryAction = YES;
            refresh(b);
            cell.accessoryView = b;
            cell.selectionStyle = UITableViewCellSelectionStyleNone;
        } else if (slot) {
            UITextField *tf = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, MIN(240, tv.bounds.size.width * 0.55), 36)];
            tf.text = SFNamesFor(slot);
            tf.placeholder = SFDefaultNames(slot);
            tf.textAlignment = NSTextAlignmentRight;
            tf.textColor = tv.tintColor;
            tf.font = [UIFont systemFontOfSize:15];
            tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
            tf.autocorrectionType = UITextAutocorrectionTypeNo;
            tf.returnKeyType = UIReturnKeyDone;
            tf.clearButtonMode = UITextFieldViewModeWhileEditing;
            tf.delegate = [SFChapField shared];
            tf.tag = 0x5F1;
            objc_setAssociatedObject(tf, kSFSlotKey, slot, OBJC_ASSOCIATION_COPY_NONATOMIC);
            [tf addTarget:[SFChapField shared] action:@selector(changed:) forControlEvents:UIControlEventEditingChanged];
            cell.accessoryView = tf;
            cell.selectionStyle = UITableViewCellSelectionStyleNone;
        } else if (cell.accessoryView.tag == 0x5F1) {
            cell.accessoryView = nil;
        }
    } @catch (NSException *e) { SFLog(@"[chap] loi dong cai dat: %@", e); }
    return cell;
}

static id (*sf_orig_ics_init)(id, SEL, NSInteger);
static id sf_hooked_ics_init(id self, SEL _cmd, NSInteger style) {
    id me = sf_orig_ics_init(self, _cmd, style);
    @try {
        NSArray *intros = [me valueForKey:@"_intros"];
        if ([intros isKindOfClass:[NSArray class]] && intros.count) {
            NSMutableArray *n = [NSMutableArray array];
            [n addObject:@{ @"type": @"sfpriority", @"title": @"Priority", @"id": @"sfpriority", @"style": @0, @"sfprio": @YES }];
            for (id row in intros) {
                [n addObject:row];
                NSString *k = [row isKindOfClass:[NSDictionary class]] ? row[@"key"] : nil;
                for (NSString *slot in SFSlots()) {
                    if ([k isEqualToString:[NSString stringWithFormat:@"segment_%@_color", slot]])
                        [n addObject:@{ @"type": @"sfchapters", @"title": @"Chapters", @"id": @"sfchapters", @"style": @0, @"sfslot": slot }];
                }
            }
            NSArray *main = [me valueForKey:@"_main"];
            NSMutableArray *nm = nil;
            if ([main isKindOfClass:[NSArray class]]) {
                nm = [NSMutableArray array];
                for (id row in main) {
                    [nm addObject:row];
                    if ([row isKindOfClass:[NSDictionary class]] && [row[@"key"] isEqualToString:@"skipAlertDuration"])
                    {
                        [nm addObject:@{ @"type": @"sfswitch", @"title": @"Skip Alert: hold until segment ends", @"id": @"sfswitch", @"style": @0, @"image": @"timer", @"sfswitchkey": @"sf.skipinf" }];
                        [nm addObject:@{ @"type": @"sfswitch", @"title": @"Show Skip again when seeking back", @"id": @"sfswitch", @"style": @0, @"image": @"backward.end.alt", @"sfswitchkey": @"sf.reshow" }];
                    }
                }
                if (nm.count == main.count) nm = nil;
            }
            if (n.count != intros.count || nm) {
                NSArray *oldSecs = [me valueForKey:@"_sections"];
                NSMutableArray *newSecs = [NSMutableArray array];
                for (id s in oldSecs) [newSecs addObject:(s == intros ? [n copy] : (nm && s == main ? [nm copy] : s))];
                [me setValue:[n copy] forKey:@"_intros"];
                if (nm) [me setValue:[nm copy] forKey:@"_main"];
                if (newSecs.count) [me setValue:newSecs forKey:@"_sections"];
            }
        }
    } @catch (NSException *e) { SFLog(@"[chap] loi chen dong: %@", e); }
    return me;
}

// InfusePlus nap sau/truoc dylib nay khong ro -> thu lai den khi co lop
static BOOL SFTryInstallChapters(void) {
    static BOOL h1, h2, h3;
    if (!h1) {
        Class c = NSClassFromString(@"INFPIntroController");
        Method m = c ? class_getInstanceMethod(c, NSSelectorFromString(@"skipIntroIfNeeded:withAsset:")) : NULL;
        Method mh = c ? class_getInstanceMethod(c, NSSelectorFromString(@"handleSegment:currentTime:")) : NULL;
        if (m && mh) {
            sf_orig_skip = (void (*)(id, SEL, double, id))method_getImplementation(m); method_setImplementation(m, (IMP)sf_hooked_skip);
            sf_orig_handle = (void (*)(id, SEL, id, double))method_getImplementation(mh); method_setImplementation(mh, (IMP)sf_hooked_handle);
            Class UD = NSClassFromString(@"INFUserDefaults");
            Method mf = UD ? class_getClassMethod(UD, NSSelectorFromString(@"floatForKey:")) : NULL;
            if (mf) { sf_orig_floatForKey = (float (*)(id, SEL, id))method_getImplementation(mf); method_setImplementation(mf, (IMP)sf_hooked_floatForKey); }
            Class TV = NSClassFromString(@"ToastView");
            Method mt = TV ? class_getInstanceMethod(TV, NSSelectorFromString(@"showProgressiveToastWithText:duration:actionTitle:actionCompletion:")) : NULL;
            if (mt) { sf_orig_toast = (void (*)(id, SEL, id, double, id, id))method_getImplementation(mt); method_setImplementation(mt, (IMP)sf_hooked_toast); }
            h1 = YES; SFLog(@"[chap] da hook INFPIntroController skipIntroIfNeeded:withAsset: + handleSegment:");
        }
    }
    if (!h3) {
        Class ph = NSClassFromString(@"PlayerHelper"), mg = NSClassFromString(@"INFPIntroManager");
        Method m1 = ph ? class_getInstanceMethod(ph, NSSelectorFromString(@"skipIntroIfNeeded:")) : NULL;
        Method m2 = mg ? class_getInstanceMethod(mg, NSSelectorFromString(@"keyForAsset:")) : NULL;
        if (m1 && m2) {
            sf_orig_ph_skip = (void (*)(id, SEL, double))method_getImplementation(m1); method_setImplementation(m1, (IMP)sf_hooked_ph_skip);
            sf_orig_keyfor = (id (*)(id, SEL, id))method_getImplementation(m2); method_setImplementation(m2, (IMP)sf_hooked_keyfor);
            h3 = YES; SFLog(@"[chap] da hook PlayerHelper skipIntroIfNeeded: + keyForAsset: (video chua khop metadata)");
        }
    }
    if (!h2) {
        Class c = NSClassFromString(@"IntroSettings");
        Method m1 = c ? class_getInstanceMethod(c, @selector(initWithStyle:)) : NULL;
        Method m2 = c ? class_getInstanceMethod(c, @selector(tableView:cellForRowAtIndexPath:)) : NULL;
        if (m1 && m2) {
            sf_orig_ics_init = (id (*)(id, SEL, NSInteger))method_getImplementation(m1); method_setImplementation(m1, (IMP)sf_hooked_ics_init);
            sf_orig_ics_cell = (UITableViewCell *(*)(id, SEL, UITableView *, NSIndexPath *))method_getImplementation(m2); method_setImplementation(m2, (IMP)sf_hooked_ics_cell);
            h2 = YES; SFLog(@"[chap] da hook trang Intros cua InfusePlus");
        }
    }
    return h1 && h2 && h3;
}

void SFInstallChapters(void) {
    if (SFTryInstallChapters()) return;
    __block int tries = 0;
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSTimer scheduledTimerWithTimeInterval:1.0 repeats:YES block:^(NSTimer *t) {
            if (SFTryInstallChapters() || ++tries > 120) {
                if (tries > 120) SFLog(@"[chap] khong thay lop InfusePlus (INFPIntroController / IntroSettings)");
                [t invalidate];
            }
        }];
    });
}
