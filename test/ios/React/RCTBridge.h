#import <React/RCTEventDispatcher.h>
@interface RCTBridge : NSObject
@property (nonatomic, readonly) RCTEventDispatcher *eventDispatcher;
@end
