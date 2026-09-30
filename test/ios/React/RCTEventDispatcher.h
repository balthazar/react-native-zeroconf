#import <Foundation/Foundation.h>
@interface RCTEventDispatcher : NSObject
- (void)sendDeviceEventWithName:(NSString *)name body:(id)body;
@end
