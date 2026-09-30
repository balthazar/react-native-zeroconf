// Harness for the dns_sd implementation of RNZeroconf.m, run on macOS
#import "RNZeroconf.h"
#import <objc/runtime.h>
#include <stdlib.h>

static NSMutableArray<NSArray *> *events;

@implementation RCTEventDispatcher
- (void)sendDeviceEventWithName:(NSString *)name body:(id)body { [events addObject:@[name, body ?: [NSNull null]]]; }
@end
@implementation RCTBridge { RCTEventDispatcher *_d; }
- (RCTEventDispatcher *)eventDispatcher { if (!_d) _d = [RCTEventDispatcher new]; return _d; }
@end

@interface RNZeroconf (Test)
- (void)scan:(NSString *)scanId type:(NSString *)type protocol:(NSString *)protocol domain:(NSString *)domain resolveTimeout:(double)resolveTimeout;
- (void)stop:(NSString *)scanId;
- (void)startResolve:(NSString *)name regtype:(NSString *)regtype domain:(NSString *)domain scan:(id)scan;
@property (nonatomic, strong, readonly) NSMutableDictionary *scans;
- (void)registerService:(NSString *)type protocol:(NSString *)protocol domain:(NSString *)domain name:(NSString *)name port:(int)port txt:(NSArray *)txt resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject;
- (void)unregisterService:(NSString *)serviceName resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject;
@end

// Live instance counters via swizzled init/dealloc
static long liveResolves = 0, livePublications = 0, liveModules = 0;
static void countInstances(Class cls, long *live) {
  Method init = class_getInstanceMethod(cls, @selector(init));
  IMP origInit = method_getImplementation(init);
  class_replaceMethod(cls, @selector(init), imp_implementationWithBlock(^id(id obj) {
    id r = ((id (*)(id, SEL))origInit)(obj, @selector(init));
    if (r) (*live)++;
    return r;
  }), method_getTypeEncoding(init));
  SEL deallocSel = sel_registerName("dealloc");
  Method dealloc = class_getInstanceMethod(cls, deallocSel);
  IMP origDealloc = method_getImplementation(dealloc);
  class_replaceMethod(cls, deallocSel, imp_implementationWithBlock(^(__unsafe_unretained id obj) {
    (*live)--;
    ((void (*)(id, SEL))origDealloc)(obj, deallocSel);
  }), method_getTypeEncoding(dealloc));
}

static void spin(double seconds) { CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false); }
static NSArray *eventsNamed(NSString *name) {
  return [events filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSArray *e, id b) { return [e[0] isEqualToString:name]; }]];
}
static int failures = 0;
static void check(BOOL ok, NSString *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what.UTF8String); if (!ok) failures++; }

int main(void) {
  @autoreleasepool {
    events = [NSMutableArray array];
    countInstances(NSClassFromString(@"RNZResolve"), &liveResolves);
    countInstances(NSClassFromString(@"RNZPublication"), &livePublications);
    countInstances([RNZeroconf class], &liveModules);
    RCTBridge *bridge = [RCTBridge new];

    @autoreleasepool {
      RNZeroconf *z = [RNZeroconf new];
      [z setValue:bridge forKey:@"bridge"];

      // Publish, with ordered TXT
      __block NSDictionary *published = nil; __block NSString *pubErr = nil;
      [z registerService:@"zcdns" protocol:@"tcp" domain:@"local." name:@"zc-dnssd" port:45690 txt:@[@[@"b", @"2"], @[@"a", @"1"], @[@"u", @"café"]]
                 resolve:^(id r) { published = r; } reject:^(NSString *c, NSString *m, NSError *e) { pubErr = c; }];
      for (int i = 0; i < 100 && !published && !pubErr; i++) spin(0.05);
      check([published[@"name"] isEqualToString:@"zc-dnssd"], [NSString stringWithFormat:@"publish resolves with name (%@ %@)", published[@"name"], pubErr]);
      check(eventsNamed(@"RNZeroconfServiceRegistered").count == 1, @"published event emitted once");

      // Same name again: registered under another name
      __block NSDictionary *dup = nil;
      [z registerService:@"zcdns" protocol:@"tcp" domain:@"local." name:@"zc-dnssd" port:45691 txt:@[]
                 resolve:^(id r) { dup = r; } reject:^(NSString *c, NSString *m, NSError *e) { dup = @{@"name": [@"REJECTED " stringByAppendingString:c]}; }];
      for (int i = 0; i < 100 && !dup; i++) spin(0.05);
      check(dup && ![dup[@"name"] isEqualToString:@"zc-dnssd"] && ![dup[@"name"] hasPrefix:@"REJECTED"], [NSString stringWithFormat:@"duplicate name is renamed (%@)", dup[@"name"]]);

      // Scan: found once per service, resolved with host/port/addresses/txt
      [events removeAllObjects];
      [z scan:@"A" type:@"zcdns" protocol:@"tcp" domain:@"local." resolveTimeout:5];
      for (int i = 0; i < 100 && eventsNamed(@"RNZeroconfResolved").count < 2; i++) spin(0.05);
      spin(1.0);
      NSMutableArray *foundNames = [NSMutableArray array];
      for (NSArray *e in eventsNamed(@"RNZeroconfFound")) [foundNames addObject:e[1][@"name"]];
      check(eventsNamed(@"RNZeroconfStart").count == 1, @"start emitted");
      check([foundNames containsObject:@"zc-dnssd"] && [[NSCountedSet setWithArray:foundNames] countForObject:@"zc-dnssd"] == 1, [NSString stringWithFormat:@"found once per service (%@)", foundNames]);
      NSDictionary *resolved = nil;
      for (NSArray *e in eventsNamed(@"RNZeroconfResolved")) if ([e[1][@"name"] isEqualToString:@"zc-dnssd"]) resolved = e[1];
      check(resolved != nil, @"resolved emitted");
      check([resolved[@"port"] intValue] == 45690, [NSString stringWithFormat:@"resolved port (%@)", resolved[@"port"]]);
      check([resolved[@"host"] hasSuffix:@".local."], [NSString stringWithFormat:@"resolved host (%@)", resolved[@"host"]]);
      check([resolved[@"addresses"] count] > 0, [NSString stringWithFormat:@"resolved addresses (%@)", [resolved[@"addresses"] componentsJoinedByString:@","]]);
      check([resolved[@"txt"][@"a"] isEqualToString:@"1"] && [resolved[@"txt"][@"u"] isEqualToString:@"café"], [NSString stringWithFormat:@"resolved txt (%@)", resolved[@"txt"]]);

      // Unpublish the duplicate while scanning: remove emitted for it
      __block NSDictionary *gone = nil;
      [z unregisterService:dup[@"name"] resolve:^(id r) { gone = r; } reject:^(NSString *c, NSString *m, NSError *e) {}];
      check([gone[@"name"] isEqualToString:dup[@"name"]], @"unpublish resolves");
      for (int i = 0; i < 100 && eventsNamed(@"RNZeroconfRemove").count == 0; i++) spin(0.05);
      BOOL removed = NO;
      for (NSArray *e in eventsNamed(@"RNZeroconfRemove")) if ([e[1][@"name"] isEqualToString:dup[@"name"]]) removed = YES;
      check(removed, @"remove emitted for unpublished service");

      // Unknown and bad publishes
      __block NSString *unknown = nil;
      [z unregisterService:@"never" resolve:^(id r) { unknown = @"RESOLVED"; } reject:^(NSString *c, NSString *m, NSError *e) { unknown = c; }];
      check([unknown isEqualToString:@"NOT_PUBLISHED"], @"unpublish unknown rejects NOT_PUBLISHED");
      __block NSString *bad = nil; __block NSDictionary *badInfo = nil;
      [z registerService:@"zcdns" protocol:@"nope" domain:@"local." name:@"zc-bad" port:1 txt:@[]
                 resolve:^(id r) { bad = @"RESOLVED"; } reject:^(NSString *c, NSString *m, NSError *e) { bad = c; badInfo = e.userInfo; }];
      for (int i = 0; i < 60 && !bad; i++) spin(0.05);
      check(bad && ![bad isEqualToString:@"RESOLVED"] && [badInfo[@"domain"] isEqualToString:@"DNSSD"], [NSString stringWithFormat:@"bad publish rejects with DNSSD error (%@ %@)", bad, badInfo[@"message"]]);

      // Retry: resolving a service that doesn't exist, 1s timeout, one TIMEOUT error after ~2s
      [z stop:nil];
      [z scan:@"A" type:@"zcnone" protocol:@"tcp" domain:@"local." resolveTimeout:1];
      [events removeAllObjects];
      NSDate *t0 = [NSDate date];
      [z startResolve:@"zc-ghost" regtype:@"_zcdns._tcp." domain:@"local." scan:z.scans[@"A"]];
      while (eventsNamed(@"RNZeroconfError").count == 0 && -[t0 timeIntervalSinceNow] < 6) spin(0.05);
      double elapsed = -[t0 timeIntervalSinceNow];
      spin(2.5);
      NSDictionary *timeoutErr = eventsNamed(@"RNZeroconfError").firstObject[1];
      check(eventsNamed(@"RNZeroconfError").count == 1 && [timeoutErr[@"code"] isEqual:@"TIMEOUT"] && elapsed > 1.8 && elapsed < 3.0,
            [NSString stringWithFormat:@"timed out resolve retried once then 1 TIMEOUT error after %.1fs", elapsed]);

      // Stress: 300 scan cycles with random short waits and stops
      for (int i = 0; i < 300; i++) {
        [z scan:@"A" type:@"zcdns" protocol:@"tcp" domain:@"local." resolveTimeout:5];
        spin((arc4random_uniform(20)) / 1000.0);
        if (arc4random_uniform(3) == 0) { [z stop:nil]; spin((arc4random_uniform(5)) / 1000.0); }
      }
      [events removeAllObjects];
      [z scan:@"A" type:@"zcdns" protocol:@"tcp" domain:@"local." resolveTimeout:5];
      for (int i = 0; i < 100 && eventsNamed(@"RNZeroconfResolved").count == 0; i++) spin(0.05);
      check(eventsNamed(@"RNZeroconfResolved").count >= 1, @"scan still resolves after 300 rapid scan/stop cycles");

      // Two scans at once, for different types, each event tagged with its scan
      __block NSDictionary *otherPub = nil;
      [z registerService:@"zcother" protocol:@"tcp" domain:@"local." name:@"zc-other" port:45692 txt:@[]
                 resolve:^(id r) { otherPub = r; } reject:^(NSString *c, NSString *m, NSError *e) {}];
      for (int i = 0; i < 100 && !otherPub; i++) spin(0.05);
      [z stop:nil];
      [events removeAllObjects];
      [z scan:@"A" type:@"zcdns" protocol:@"tcp" domain:@"local." resolveTimeout:5];
      [z scan:@"B" type:@"zcother" protocol:@"tcp" domain:@"local." resolveTimeout:5];
      for (int i = 0; i < 100 && eventsNamed(@"RNZeroconfResolved").count < 2; i++) spin(0.05);
      spin(1.0);
      NSMutableSet *resolvedA = [NSMutableSet set], *resolvedB = [NSMutableSet set];
      for (NSArray *e in eventsNamed(@"RNZeroconfResolved")) {
        if ([e[1][@"scanId"] isEqualToString:@"A"]) [resolvedA addObject:e[1][@"name"]];
        if ([e[1][@"scanId"] isEqualToString:@"B"]) [resolvedB addObject:e[1][@"name"]];
      }
      check([resolvedA containsObject:@"zc-dnssd"] && ![resolvedA containsObject:@"zc-other"], [NSString stringWithFormat:@"scan A resolves only its type (%@)", resolvedA]);
      check([resolvedB isEqualToSet:[NSSet setWithObject:@"zc-other"]], [NSString stringWithFormat:@"scan B resolves only its type (%@)", resolvedB]);
      NSInteger starts = 0; for (NSArray *e in eventsNamed(@"RNZeroconfStart")) if (e[1][@"scanId"]) starts++;
      check(starts == 2, @"both scans emit start with their id");
      [events removeAllObjects];
      [z stop:@"A"];
      check(eventsNamed(@"RNZeroconfStop").count == 1 && [eventsNamed(@"RNZeroconfStop")[0][1][@"scanId"] isEqualToString:@"A"], @"stopping A emits stop for A only");
      check(z.scans[@"B"] != nil && z.scans[@"A"] == nil, @"B keeps running after A stops");
      __block NSDictionary *gone2 = nil;
      [z unregisterService:@"zc-other" resolve:^(id r) { gone2 = r; } reject:^(NSString *c, NSString *m, NSError *e) {}];
      for (int i = 0; i < 100 && eventsNamed(@"RNZeroconfRemove").count == 0; i++) spin(0.05);
      check(eventsNamed(@"RNZeroconfRemove").count == 1 && [eventsNamed(@"RNZeroconfRemove")[0][1][@"scanId"] isEqualToString:@"B"], @"scan B still sees removals after A stopped");

      // Teardown with a scan running and a service published
      [z invalidate];
    }
    spin(1.0);
    printf("live after teardown: resolves=%ld publications=%ld modules=%ld\n", liveResolves, livePublications, liveModules);
    check(liveResolves == 0 && livePublications == 0 && liveModules == 0, @"no leaked resolves, publications or module");
    printf("%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
  }
  return failures ? 1 : 0;
}
