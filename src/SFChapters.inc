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

static NSArray *SFCurrentChapters(id *helperOut) {
    Class PH = NSClassFromString(@"PlayerHelper");
    id helper = PH && [PH respondsToSelector:NSSelectorFromString(@"currentPlayer")] ? ((id (*)(id, SEL))objc_msgSend)(PH, NSSelectorFromString(@"currentPlayer")) : nil;
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
static const void *kSFSlotKey = &kSFSlotKey;

// Ghi doan tu chuong vao INFPMediaIntro cua video hien tai (neu co chuong khop).
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
    if (!chs.count) return;

    NSMutableString *names = [NSMutableString string];
    for (NSString *s in SFSlots()) [names appendFormat:@"%@=%@;", s, SFNamesFor(s)];
    NSMutableString *sig = [NSMutableString stringWithFormat:@"%@|%lu|%@", key, (unsigned long)chs.count, names];
    NSMutableDictionary *intros = SFObj0(mgr, @"intros");
    id cur = [intros respondsToSelector:@selector(objectForKey:)] ? intros[key] : nil;
    NSString *curSig = cur ? objc_getAssociatedObject(cur, kSFChapSigKey) : nil;
    if (cur && [curSig isEqualToString:sig]) return;                       // da ap dung, khong doi gi

    double duration = SFD(helper, @"duration");
    NSMutableDictionary<NSString *, NSArray *> *found = [NSMutableDictionary dictionary];   // slot -> @[start, end, title]
    NSMutableString *dump = [NSMutableString string];
    for (NSUInteger i = 0; i < chs.count; i++) {
        id c = chs[i];
        NSString *title = SFStr0(c, @"title") ?: @"";
        double st = SFD(c, @"startTime"), en = SFD(c, @"endTime");
        if (en <= st) en = (i + 1 < chs.count) ? SFD(chs[i + 1], @"startTime") : duration;
        [dump appendFormat:@"[%@ %.0f-%.0f] ", title, st, en];
        if (en <= st) continue;
        for (NSString *slot in SFSlots()) {
            if (found[slot]) continue;
            if (SFTitleMatches(title, SFSplitNames(SFNamesFor(slot)))) found[slot] = @[ @(st), @(en), title ];
        }
    }
    if (![lastLogged isEqualToString:sig]) {
        lastLogged = [sig copy];
        SFLog(@"[chap] %lu chuong: %@ => khop: %@", (unsigned long)chs.count, dump, found.count ? found : @"(khong)");
    }
    if (!found.count) return;

    id intro = cur ?: ((id (*)(id, SEL))objc_msgSend)([IntroC alloc], @selector(init));
    NSDictionary *setters = @{ @"intro": @"setIntro:", @"recap": @"setRecap:", @"preview": @"setPreview:", @"outro": @"setOutro:" };
    for (NSString *slot in found) {
        id seg = ((id (*)(id, SEL))objc_msgSend)([SegC alloc], @selector(init));
        ((void (*)(id, SEL, double))objc_msgSend)(seg, NSSelectorFromString(@"setStart:"), [found[slot][0] doubleValue]);
        ((void (*)(id, SEL, double))objc_msgSend)(seg, NSSelectorFromString(@"setEnd:"), [found[slot][1] doubleValue]);
        ((void (*)(id, SEL, id))objc_msgSend)(seg, NSSelectorFromString(@"setIdentifier:"), [@"segment_" stringByAppendingString:slot]);
        ((void (*)(id, SEL, id))objc_msgSend)(intro, NSSelectorFromString(setters[slot]), seg);
    }
    ((void (*)(id, SEL, BOOL))objc_msgSend)(intro, NSSelectorFromString(@"setHasData:"), YES);
    objc_setAssociatedObject(intro, kSFChapSigKey, sig, OBJC_ASSOCIATION_COPY_NONATOMIC);
    if (!cur && [intros respondsToSelector:@selector(setObject:forKeyedSubscript:)]) intros[key] = intro;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:@"INFPIntroSegmentsDidLoad" object:nil];
    });
}

static void (*sf_orig_skip)(id, SEL, double, id);
static void sf_hooked_skip(id self, SEL _cmd, double t, id asset) {
    @try { SFApplyChapters(asset); } @catch (NSException *e) { SFLog(@"[chap] loi: %@", e); }
    sf_orig_skip(self, _cmd, t, asset);
}

// ---- dong nhap ten chuong trong trang Intros cua InfusePlus

@interface SFChapField : NSObject <UITextFieldDelegate>
@end
@implementation SFChapField
+ (instancetype)shared { static SFChapField *s; static dispatch_once_t o; dispatch_once(&o, ^{ s = [SFChapField new]; }); return s; }
- (void)changed:(UITextField *)tf { NSString *slot = objc_getAssociatedObject(tf, kSFSlotKey); if (slot) SFSetNames(slot, tf.text); }
- (BOOL)textFieldShouldReturn:(UITextField *)tf { [tf resignFirstResponder]; return YES; }
@end

static UITableViewCell *(*sf_orig_ics_cell)(id, SEL, UITableView *, NSIndexPath *);
static UITableViewCell *sf_hooked_ics_cell(id self, SEL _cmd, UITableView *tv, NSIndexPath *ip) {
    UITableViewCell *cell = sf_orig_ics_cell(self, _cmd, tv, ip);
    @try {
        NSArray *secs = [self valueForKey:@"_sections"];
        id row = (ip.section < (NSInteger)secs.count && ip.row < (NSInteger)[secs[ip.section] count]) ? secs[ip.section][ip.row] : nil;
        NSString *slot = [row isKindOfClass:[NSDictionary class]] ? row[@"sfslot"] : nil;
        if (slot) {
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
        } else if ([cell.accessoryView isKindOfClass:[UITextField class]] && cell.accessoryView.tag == 0x5F1) {
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
            for (id row in intros) {
                [n addObject:row];
                NSString *k = [row isKindOfClass:[NSDictionary class]] ? row[@"key"] : nil;
                for (NSString *slot in SFSlots()) {
                    if ([k isEqualToString:[NSString stringWithFormat:@"segment_%@_color", slot]])
                        [n addObject:@{ @"type": @"sfchapters", @"title": @"Chapters", @"id": @"sfchapters", @"style": @0, @"sfslot": slot }];
                }
            }
            if (n.count != intros.count) {
                NSArray *oldSecs = [me valueForKey:@"_sections"];
                NSMutableArray *newSecs = [NSMutableArray array];
                for (id s in oldSecs) [newSecs addObject:(s == intros ? [n copy] : s)];
                [me setValue:[n copy] forKey:@"_intros"];
                if (newSecs.count) [me setValue:newSecs forKey:@"_sections"];
            }
        }
    } @catch (NSException *e) { SFLog(@"[chap] loi chen dong: %@", e); }
    return me;
}

// InfusePlus nap sau/truoc dylib nay khong ro -> thu lai den khi co lop
static BOOL SFTryInstallChapters(void) {
    static BOOL h1, h2;
    if (!h1) {
        Class c = NSClassFromString(@"INFPIntroController");
        Method m = c ? class_getInstanceMethod(c, NSSelectorFromString(@"skipIntroIfNeeded:withAsset:")) : NULL;
        if (m) { sf_orig_skip = (void (*)(id, SEL, double, id))method_getImplementation(m); method_setImplementation(m, (IMP)sf_hooked_skip); h1 = YES; SFLog(@"[chap] da hook INFPIntroController skipIntroIfNeeded:withAsset:"); }
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
    return h1 && h2;
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
