// SFEngine - quet nguon (VFS cua Infuse), khop ten, nhap sub vao cache cua Infuse.
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif
void SFLog(NSString *format, ...) NS_FORMAT_FUNCTION(1, 2);
#ifdef __cplusplus
}
#endif

@interface SFEngine : NSObject

+ (instancetype)shared;

// Bat dau quet nen neu chua quet hoac da cu (theo cau hinh rescanMinutes).
- (void)startIndexingIfNeeded;

// Goi tu hook: tim sub khop voi video cua `mediaFile` (FCMediaFile) va nap vao cache cua Infuse.
- (void)prepareSubtitlesForMediaFile:(id)mediaFile;

@end
