// SFEngine - quet nguon (VFS cua Infuse), khop ten, nhap sub vao cache cua Infuse.
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif
void SFLog(NSString *format, ...) NS_FORMAT_FUNCTION(1, 2);
void SFToast(NSString *text);
void SFShowText(NSString *title, NSString *text);  // thong bao nho tren man hinh (main thread tu dispatch)
#ifdef __cplusplus
}
#endif

@interface SFEngine : NSObject

+ (instancetype)shared;

// Bat dau quet nen neu chua quet hoac da cu (theo cau hinh rescanMinutes).
- (void)startIndexingIfNeeded;

// Goi tu hook -[FCMediaFile relatedItems]: them sub khop (o thu muc/nguon khac) vao danh sach file lien quan.
- (NSArray *)relatedItemsForMediaFile:(id)mediaFile original:(NSArray *)orig;

@end
