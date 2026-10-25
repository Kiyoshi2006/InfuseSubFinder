// SFEngine - quet nguon (VFS cua Infuse), khop ten, nhap sub vao cache cua Infuse.
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif
void SFLog(NSString *format, ...) NS_FORMAT_FUNCTION(1, 2);
void SFToast(NSString *text);
void SFShowText(NSString *title, NSString *text);
void SFInstallUI(void);
BOOL SFInstallSettingsHook(void);
void SFOpenSettings(id settingsController);   // nut ISF noi + bang cai dat (SFUI.m)  // thong bao nho tren man hinh (main thread tu dispatch)
#ifdef __cplusplus
}
#endif

@interface SFEngine : NSObject

+ (instancetype)shared;

// Bat dau quet nen neu chua quet hoac da cu (theo cau hinh rescanMinutes).
- (void)startIndexingIfNeeded;

// Goi tu hook -[FCMediaFile relatedItems]: them sub khop (o thu muc/nguon khac) vao danh sach file lien quan.
- (NSArray *)relatedItemsForMediaFile:(id)mediaFile original:(NSArray *)orig;

- (BOOL)isCrawling;
- (void)refreshAll;
- (void)refreshSource:(NSString *)sourceId;
- (void)refreshFolderItem:(id)item source:(NSString *)sourceId;
- (void)cancelKey:(NSString *)key;
- (void)cancelAll;
- (NSDictionary *)jobInfoForKey:(NSString *)key;   // progress, folders, subs, source (nil = khong chay)
- (NSInteger)runningCount;
- (double)overallProgress;
- (NSString *)keyForSource:(NSString *)sourceId path:(NSString *)path;   // path = nil -> key cua nguon
- (NSDictionary *)listItem:(id)item source:(NSString *)sourceId;         // folders, files, ok
- (id)rootItemForSource:(NSString *)sourceId;
- (NSInteger)subCountForSource:(NSString *)sourceId pathPrefix:(NSString *)path;
- (double)lastForKey:(NSString *)key;
- (NSArray<NSDictionary *> *)sourceList;
- (void)setSource:(NSString *)sourceId enabled:(BOOL)on;
- (BOOL)isPathItselfDisabled:(NSString *)sourceId path:(NSString *)path;
- (void)setPath:(NSString *)path source:(NSString *)sourceId enabled:(BOOL)on;
- (NSString *)autoMode;                      // manual | launch | hourly | 12h
- (void)setAutoMode:(NSString *)mode;
- (NSInteger)totalSubs;

@end
