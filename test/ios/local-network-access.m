// Harness for checkLocalNetworkAccess in RNZeroconf.m, run on macOS
#import "RNZeroconf.h"
@implementation RCTEventDispatcher
- (void)sendDeviceEventWithName:(NSString *)name body:(id)body {}
@end
@implementation RCTBridge
- (RCTEventDispatcher *)eventDispatcher { return nil; }
@end
@interface RNZeroconf (Test)
- (void)checkLocalNetworkAccess:(NSString *)type timeout:(double)timeout resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject;
@end
int main(void) {
  @autoreleasepool {
    RNZeroconf *z = [RNZeroconf new];
    __block id result = nil; __block NSString *rejected = nil;
    NSDate *t0 = [NSDate date];
    [z checkLocalNetworkAccess:@"_zctest._tcp." timeout:5 resolve:^(id r) { result = r; } reject:^(NSString *c, NSString *m, NSError *e) { rejected = c; }];
    while (!result && !rejected && -[t0 timeIntervalSinceNow] < 8) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    printf("with type: %s after %.2fs\n", [[result ?: rejected description] UTF8String], -[t0 timeIntervalSinceNow]);
    // The answer depends on the machine's Local Network settings, any of the three is valid
    BOOL typedOK = [@[@"granted", @"denied", @"unknown"] containsObject:result];
    result = nil; rejected = nil;
    [z checkLocalNetworkAccess:nil timeout:5 resolve:^(id r) { result = r; } reject:^(NSString *c, NSString *m, NSError *e) { rejected = c; }];
    printf("no type, no Info.plist: %s\n", [[result ?: rejected description] UTF8String]);
    BOOL missingOK = [rejected isEqualToString:@"MISSING_BONJOUR_SERVICES"];
    printf("%s\n", typedOK && missingOK ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return typedOK && missingOK ? 0 : 1;
  }
}
