#import <Foundation/Foundation.h>
@class RCTBridge;
#define RCT_EXPORT_MODULE() + (NSString *)moduleName { return @""; }
#define RCT_EXPORT_METHOD(method) - (void)method
@protocol RCTBridgeModule <NSObject>
@property (nonatomic, weak, readonly) RCTBridge *bridge;
@optional
- (dispatch_queue_t)methodQueue;
+ (BOOL)requiresMainQueueSetup;
@end
typedef void (^RCTPromiseResolveBlock)(id result);
typedef void (^RCTPromiseRejectBlock)(NSString *code, NSString *message, NSError *error);
