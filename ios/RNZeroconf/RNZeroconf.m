//
//  RNZeroconf.m
//  RNZeroconf
//
//  Created by Balthazar Gronon on 25/10/2015.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import "RNZeroconf.h"
#import "RNNetServiceSerializer.h"
#import <Network/Network.h>
#import <dns_sd.h>

// dns_sd callbacks are delivered on the main queue, and refs must be deallocated on it
#define RNZ_QUEUE dispatch_get_main_queue()

#pragma mark - State

@class RNZScan;

// A found service being resolved: DNSServiceResolve for host, port and TXT, then DNSServiceGetAddrInfo for addresses
@interface RNZResolve : NSObject
@property (nonatomic, weak) RNZeroconf *module;
@property (nonatomic, weak) RNZScan *scan;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *regtype;
@property (nonatomic, copy) NSString *domain;
@property (nonatomic, assign) DNSServiceRef resolveRef;
@property (nonatomic, assign) DNSServiceRef addressRef;
@property (nonatomic, copy) NSString *host;
@property (nonatomic, assign) uint16_t port;
@property (nonatomic, strong) NSDictionary *txt;
@property (nonatomic, strong) NSMutableOrderedSet<NSString *> *addresses;
@property (nonatomic, assign) BOOL retried;
@property (nonatomic, assign) BOOL finished;
@property (nonatomic, strong) dispatch_block_t timeoutBlock;
@property (nonatomic, strong) dispatch_block_t emitBlock;
@end

@implementation RNZResolve
@end

// One browse, keyed by the id of the JS instance that started it, several can run at once
@interface RNZScan : NSObject
@property (nonatomic, weak) RNZeroconf *module;
@property (nonatomic, copy) NSString *scanId;
@property (nonatomic, assign) DNSServiceRef browseRef;
@property (nonatomic, assign) NSTimeInterval resolveTimeoutSeconds;
// Services are reported once per network interface, count them so found/remove are emitted once
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSNumber *> *foundInterfaces;
@property (nonatomic, strong) NSMutableDictionary<NSString *, RNZResolve *> *resolvingServices;
@end

@implementation RNZScan
@end

// A service registered with DNSServiceRegister
@interface RNZPublication : NSObject
@property (nonatomic, weak) RNZeroconf *module;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *regtype;
@property (nonatomic, copy) NSString *domain;
@property (nonatomic, assign) uint16_t port;
@property (nonatomic, strong) NSDictionary *txt;
@property (nonatomic, assign) DNSServiceRef ref;
@property (nonatomic, copy) RCTPromiseResolveBlock resolve;
@property (nonatomic, copy) RCTPromiseRejectBlock reject;
@end

@implementation RNZPublication
@end

@interface RNZeroconf ()

@property (nonatomic, strong, readonly) NSMutableDictionary<NSString *, RNZScan *> *scans;
// Registered, keyed by the name actually used (it can differ from the requested one)
@property (nonatomic, strong, readonly) NSMutableDictionary<NSString *, RNZPublication *> *publishedServices;
// Waiting for the register callback
@property (nonatomic, strong, readonly) NSMutableSet<RNZPublication *> *pendingPublications;

- (void) handleBrowse:(RNZScan *)scan flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error name:(const char *)name regtype:(const char *)regtype domain:(const char *)domain;
- (void) handleResolve:(RNZResolve *)resolve error:(DNSServiceErrorType)error host:(const char *)host port:(uint16_t)port txtLength:(uint16_t)txtLength txt:(const unsigned char *)txt;
- (void) handleAddress:(RNZResolve *)resolve flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error address:(const struct sockaddr *)address;
- (void) handleRegister:(RNZPublication *)publication flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error name:(const char *)name;

@end

#pragma mark - dns_sd callbacks

static void RNZBrowseReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                           const char *name, const char *regtype, const char *domain, void *context)
{
    RNZScan *scan = (__bridge RNZScan *)context;
    [scan.module handleBrowse:scan flags:flags error:error name:name regtype:regtype domain:domain];
}

static void RNZResolveReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                            const char *fullName, const char *host, uint16_t port, uint16_t txtLength,
                            const unsigned char *txt, void *context)
{
    RNZResolve *resolve = (__bridge RNZResolve *)context;
    [resolve.module handleResolve:resolve error:error host:host port:ntohs(port) txtLength:txtLength txt:txt];
}

static void RNZAddressReply(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error,
                            const char *host, const struct sockaddr *address, uint32_t ttl, void *context)
{
    RNZResolve *resolve = (__bridge RNZResolve *)context;
    [resolve.module handleAddress:resolve flags:flags error:error address:address];
}

static void RNZRegisterReply(DNSServiceRef ref, DNSServiceFlags flags, DNSServiceErrorType error,
                             const char *name, const char *regtype, const char *domain, void *context)
{
    RNZPublication *publication = (__bridge RNZPublication *)context;
    [publication.module handleRegister:publication flags:flags error:error name:name];
}

// Deallocates on the next turn of the queue, for refs whose callback may currently be running.
// keepAlive keeps the callback context alive until then.
static void RNZDeallocateLater(DNSServiceRef ref, id keepAlive)
{
    if (ref == NULL) {
        return;
    }
    dispatch_async(RNZ_QUEUE, ^{
        DNSServiceRefDeallocate(ref);
        (void)keepAlive;
    });
}

@implementation RNZeroconf

@synthesize bridge = _bridge;

RCT_EXPORT_MODULE()

- (dispatch_queue_t)methodQueue
{
    return RNZ_QUEUE;
}

+ (BOOL)requiresMainQueueSetup
{
    return YES;
}

- (instancetype) init
{
    self = [super init];

    if (self) {
        _scans = [[NSMutableDictionary alloc] init];
        _publishedServices = [[NSMutableDictionary alloc] init];
        _pendingPublications = [[NSMutableSet alloc] init];
    }

    return self;
}

#pragma mark - Scan

RCT_EXPORT_METHOD(scan:(NSString *)scanId
                  type:(NSString *)type
                  protocol:(NSString *)protocol
                  domain:(NSString *)domain
                  resolveTimeout:(double)resolveTimeout)
{
    [self stopScan:scanId];

    RNZScan *scan = [[RNZScan alloc] init];
    scan.module = self;
    scan.scanId = scanId;
    scan.resolveTimeoutSeconds = resolveTimeout > 0 ? resolveTimeout : 5.0;
    scan.foundInterfaces = [[NSMutableDictionary alloc] init];
    scan.resolvingServices = [[NSMutableDictionary alloc] init];

    NSString *regtype = [NSString stringWithFormat:@"_%@._%@", type, protocol];
    DNSServiceRef ref = NULL;
    DNSServiceErrorType error = DNSServiceBrowse(&ref, 0, kDNSServiceInterfaceIndexAny, regtype.UTF8String,
                                                 domain.length > 0 ? domain.UTF8String : NULL,
                                                 RNZBrowseReply, (__bridge void *)scan);
    if (error != kDNSServiceErr_NoError) {
        [self sendError:[self errorWithCode:error action:@"Browsing services" serviceName:nil] scan:scan];
        return;
    }
    DNSServiceSetDispatchQueue(ref, RNZ_QUEUE);
    scan.browseRef = ref;
    self.scans[scanId] = scan;
    [self sendEvent:@"RNZeroconfStart" scan:scan body:@{}];
}

// Stops the scan with this id, or every scan without one
RCT_EXPORT_METHOD(stop:(NSString *)scanId)
{
    if (scanId.length > 0) {
        [self stopScan:scanId];
        return;
    }
    for (NSString *key in self.scans.allKeys) {
        [self stopScan:key];
    }
}

- (void) stopScan:(NSString *)scanId
{
    RNZScan *scan = self.scans[scanId];
    if (!scan) {
        return;
    }
    [self.scans removeObjectForKey:scanId];

    for (RNZResolve *resolve in scan.resolvingServices.allValues) {
        [self cancelResolve:resolve];
    }
    [scan.resolvingServices removeAllObjects];
    [scan.foundInterfaces removeAllObjects];

    if (scan.browseRef) {
        DNSServiceRefDeallocate(scan.browseRef);
        scan.browseRef = NULL;
    }
    [self sendEvent:@"RNZeroconfStop" scan:scan body:@{}];
}

- (void) handleBrowse:(RNZScan *)scan flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error name:(const char *)name regtype:(const char *)regtype domain:(const char *)domain
{
    if (scan.browseRef == NULL) {
        return;
    }
    if (error != kDNSServiceErr_NoError) {
        [self sendError:[self errorWithCode:error action:@"Browsing services" serviceName:nil] scan:scan];
        // The browse can't continue after an error
        RNZDeallocateLater(scan.browseRef, scan);
        scan.browseRef = NULL;
        if (self.scans[scan.scanId] == scan) {
            [self stopScan:scan.scanId];
        }
        return;
    }

    NSString *serviceName = name ? [NSString stringWithUTF8String:name] : nil;
    if (!serviceName) {
        return;
    }
    NSInteger interfaces = scan.foundInterfaces[serviceName].integerValue;

    if (flags & kDNSServiceFlagsAdd) {
        scan.foundInterfaces[serviceName] = @(interfaces + 1);
        if (interfaces == 0) {
            [self sendEvent:@"RNZeroconfFound" scan:scan body:@{ kRNServiceKeysName: serviceName }];
            [self startResolve:serviceName regtype:[NSString stringWithUTF8String:regtype] domain:[NSString stringWithUTF8String:domain] scan:scan];
        }
        return;
    }

    if (interfaces > 1) {
        scan.foundInterfaces[serviceName] = @(interfaces - 1);
        return;
    }
    [scan.foundInterfaces removeObjectForKey:serviceName];

    // Stop any pending resolve for the removed service
    RNZResolve *resolve = scan.resolvingServices[serviceName];
    if (resolve) {
        [self cancelResolve:resolve];
        [scan.resolvingServices removeObjectForKey:serviceName];
    }
    [self sendEvent:@"RNZeroconfRemove" scan:scan body:@{ kRNServiceKeysName: serviceName }];
}

#pragma mark - Resolve

- (void) startResolve:(NSString *)name regtype:(NSString *)regtype domain:(NSString *)domain scan:(RNZScan *)scan
{
    RNZResolve *resolve = [[RNZResolve alloc] init];
    resolve.module = self;
    resolve.scan = scan;
    resolve.name = name;
    resolve.regtype = regtype;
    resolve.domain = domain;
    scan.resolvingServices[name] = resolve;
    [self resolve:resolve];
}

- (void) resolve:(RNZResolve *)resolve
{
    resolve.finished = NO;
    resolve.addresses = [[NSMutableOrderedSet alloc] init];

    DNSServiceRef ref = NULL;
    DNSServiceErrorType error = DNSServiceResolve(&ref, 0, kDNSServiceInterfaceIndexAny, resolve.name.UTF8String,
                                                  resolve.regtype.UTF8String, resolve.domain.UTF8String,
                                                  RNZResolveReply, (__bridge void *)resolve);
    if (error != kDNSServiceErr_NoError) {
        [self failResolve:resolve error:[self errorWithCode:error action:@"Resolving service" serviceName:resolve.name]];
        return;
    }
    DNSServiceSetDispatchQueue(ref, RNZ_QUEUE);
    resolve.resolveRef = ref;

    __weak RNZeroconf *weakSelf = self;
    __weak RNZResolve *weakResolve = resolve;
    resolve.timeoutBlock = dispatch_block_create(0, ^{
        [weakSelf resolveTimedOut:weakResolve];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(resolve.scan.resolveTimeoutSeconds * NSEC_PER_SEC)), RNZ_QUEUE, resolve.timeoutBlock);
}

- (void) resolveTimedOut:(RNZResolve *)resolve
{
    if (!resolve || resolve.finished || resolve.scan.resolvingServices[resolve.name] != resolve) {
        return;
    }
    [self cancelResolve:resolve];

    // Slow devices can time out, retry once before reporting the error (#186)
    if (!resolve.retried) {
        resolve.retried = YES;
        [self resolve:resolve];
        return;
    }

    [self failResolve:resolve error:@{
        @"message": [NSString stringWithFormat:@"Resolving service %@ failed: timed out", resolve.name],
        @"code": @"TIMEOUT",
        @"domain": @"RNZeroconf",
        @"serviceName": resolve.name,
    }];
}

- (void) handleResolve:(RNZResolve *)resolve error:(DNSServiceErrorType)error host:(const char *)host port:(uint16_t)port txtLength:(uint16_t)txtLength txt:(const unsigned char *)txt
{
    if (resolve.finished || resolve.resolveRef == NULL) {
        return;
    }
    if (error != kDNSServiceErr_NoError) {
        [self failResolve:resolve error:[self errorWithCode:error action:@"Resolving service" serviceName:resolve.name]];
        return;
    }

    // Only the first answer is needed
    RNZDeallocateLater(resolve.resolveRef, resolve);
    resolve.resolveRef = NULL;

    resolve.host = host ? [NSString stringWithUTF8String:host] : nil;
    resolve.port = port;
    resolve.txt = [RNNetServiceSerializer dictionaryFromTXTRecord:txt length:txtLength];

    DNSServiceRef ref = NULL;
    DNSServiceErrorType addressError = DNSServiceGetAddrInfo(&ref, 0, kDNSServiceInterfaceIndexAny,
                                                             kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6,
                                                             host, RNZAddressReply, (__bridge void *)resolve);
    if (addressError != kDNSServiceErr_NoError) {
        [self failResolve:resolve error:[self errorWithCode:addressError action:@"Resolving service" serviceName:resolve.name]];
        return;
    }
    DNSServiceSetDispatchQueue(ref, RNZ_QUEUE);
    resolve.addressRef = ref;
}

- (void) handleAddress:(RNZResolve *)resolve flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error address:(const struct sockaddr *)address
{
    if (resolve.finished || resolve.addressRef == NULL) {
        return;
    }
    if (error != kDNSServiceErr_NoError) {
        // No record for one of the address families is not a failure
        if (error == kDNSServiceErr_NoSuchRecord) {
            return;
        }
        [self failResolve:resolve error:[self errorWithCode:error action:@"Resolving service" serviceName:resolve.name]];
        return;
    }

    NSString *string = [RNNetServiceSerializer stringFromAddress:address];
    if (string && (flags & kDNSServiceFlagsAdd)) {
        [resolve.addresses addObject:string];
    }
    if (flags & kDNSServiceFlagsMoreComing) {
        return;
    }

    // IPv4 and IPv6 answers can arrive in separate batches, wait briefly before emitting
    if (resolve.emitBlock) {
        dispatch_block_cancel(resolve.emitBlock);
    }
    __weak RNZeroconf *weakSelf = self;
    __weak RNZResolve *weakResolve = resolve;
    resolve.emitBlock = dispatch_block_create(0, ^{
        [weakSelf finishResolve:weakResolve];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), RNZ_QUEUE, resolve.emitBlock);
}

- (void) finishResolve:(RNZResolve *)resolve
{
    if (!resolve || resolve.finished || resolve.addresses.count == 0) {
        return;
    }
    [self cancelResolve:resolve];
    if (resolve.scan.resolvingServices[resolve.name] == resolve) {
        [resolve.scan.resolvingServices removeObjectForKey:resolve.name];
    }

    [self sendEvent:@"RNZeroconfResolved" scan:resolve.scan body:@{
        kRNServiceKeysName: resolve.name,
        kRNServiceKeysFullName: [NSString stringWithFormat:@"%@%@.", resolve.host ?: @"", resolve.regtype],
        kRNServiceKeysHost: resolve.host ?: @"",
        kRNServiceKeysPort: @(resolve.port),
        kRNServiceKeysAddresses: resolve.addresses.array,
        kRNServiceTxtRecords: resolve.txt ?: @{},
    }];
}

- (void) failResolve:(RNZResolve *)resolve error:(NSDictionary *)error
{
    [self cancelResolve:resolve];
    if (resolve.scan.resolvingServices[resolve.name] == resolve) {
        [resolve.scan.resolvingServices removeObjectForKey:resolve.name];
    }
    [self sendError:error scan:resolve.scan];
}

// Stops everything in flight for a resolve, safe to call from its own callbacks
- (void) cancelResolve:(RNZResolve *)resolve
{
    resolve.finished = YES;
    if (resolve.timeoutBlock) {
        dispatch_block_cancel(resolve.timeoutBlock);
        resolve.timeoutBlock = nil;
    }
    if (resolve.emitBlock) {
        dispatch_block_cancel(resolve.emitBlock);
        resolve.emitBlock = nil;
    }
    RNZDeallocateLater(resolve.resolveRef, resolve);
    resolve.resolveRef = NULL;
    RNZDeallocateLater(resolve.addressRef, resolve);
    resolve.addressRef = NULL;
}

#pragma mark - Publish

RCT_EXPORT_METHOD(registerService:(NSString *)type
                  protocol:(NSString *)protocol
                  domain:(NSString *)domain
                  name:(NSString *)name
                  port:(int)port
                  txt:(NSArray<NSArray<NSString *> *> *)txt
                  resolve:(RCTPromiseResolveBlock)resolve
                  reject:(RCTPromiseRejectBlock)reject)
{
    NSMutableArray<NSString *> *tooLong = [NSMutableArray array];
    NSData *txtRecord = [RNNetServiceSerializer TXTRecordFromPairs:txt ?: @[] tooLong:tooLong];
    for (NSString *key in tooLong) {
        [self sendError:@{
            @"message": [NSString stringWithFormat:@"TXT record entry %@ is longer than 255 bytes", key],
            @"code": @"TXT_ENTRY_TOO_LONG",
            @"domain": @"RNZeroconf",
            @"serviceName": name,
        }];
    }

    // dns_sd lets the same app register a name twice, rename like Bonjour does on conflicts
    NSString *availableName = name;
    for (NSInteger suffix = 2; [self isNameInUse:availableName]; suffix++) {
        availableName = [NSString stringWithFormat:@"%@ (%ld)", name, (long)suffix];
    }
    name = availableName;

    RNZPublication *publication = [[RNZPublication alloc] init];
    publication.module = self;
    publication.name = name;
    publication.regtype = [NSString stringWithFormat:@"_%@._%@", type, protocol];
    publication.domain = domain.length > 0 ? domain : @"local.";
    publication.port = (uint16_t)port;
    publication.txt = [RNNetServiceSerializer dictionaryFromTXTRecord:txtRecord.bytes length:(uint16_t)txtRecord.length];
    publication.resolve = resolve;
    publication.reject = reject;

    DNSServiceRef ref = NULL;
    DNSServiceErrorType error = DNSServiceRegister(&ref, 0, kDNSServiceInterfaceIndexAny, name.UTF8String,
                                                   publication.regtype.UTF8String,
                                                   domain.length > 0 ? domain.UTF8String : NULL,
                                                   NULL, htons((uint16_t)port),
                                                   (uint16_t)txtRecord.length, txtRecord.length > 0 ? txtRecord.bytes : NULL,
                                                   RNZRegisterReply, (__bridge void *)publication);
    if (error != kDNSServiceErr_NoError) {
        NSDictionary *errorInfo = [self errorWithCode:error action:@"Publishing service" serviceName:name];
        [self sendError:errorInfo];
        [self reject:reject error:errorInfo];
        return;
    }
    DNSServiceSetDispatchQueue(ref, RNZ_QUEUE);
    publication.ref = ref;
    [self.pendingPublications addObject:publication];
}

- (void) handleRegister:(RNZPublication *)publication flags:(DNSServiceFlags)flags error:(DNSServiceErrorType)error name:(const char *)name
{
    if (publication.ref == NULL) {
        return;
    }
    if (error != kDNSServiceErr_NoError) {
        NSDictionary *errorInfo = [self errorWithCode:error action:@"Publishing service" serviceName:publication.name];
        [self sendError:errorInfo];
        [self.pendingPublications removeObject:publication];
        if (self.publishedServices[publication.name] == publication) {
            [self.publishedServices removeObjectForKey:publication.name];
        }
        RNZDeallocateLater(publication.ref, publication);
        publication.ref = NULL;
        if (publication.reject) {
            [self reject:publication.reject error:errorInfo];
        }
        publication.resolve = nil;
        publication.reject = nil;
        return;
    }
    if (!(flags & kDNSServiceFlagsAdd)) {
        return;
    }

    // The name can differ from the requested one when it was already taken
    NSString *registeredName = (name ? [NSString stringWithUTF8String:name] : nil) ?: publication.name;
    if (self.publishedServices[publication.name] == publication) {
        [self.publishedServices removeObjectForKey:publication.name];
    }
    publication.name = registeredName;
    self.publishedServices[registeredName] = publication;
    [self.pendingPublications removeObject:publication];

    NSDictionary *serviceInfo = [self publicationInfo:publication];
    [self sendEvent:@"RNZeroconfServiceRegistered" body:serviceInfo];
    if (publication.resolve) {
        publication.resolve(serviceInfo);
    }
    publication.resolve = nil;
    publication.reject = nil;
}

RCT_EXPORT_METHOD(unregisterService:(NSString *)serviceName
                  resolve:(RCTPromiseResolveBlock)resolve
                  reject:(RCTPromiseRejectBlock)reject)
{
    RNZPublication *publication = self.publishedServices[serviceName];
    if (!publication) {
        [self reject:reject error:@{
            @"message": [NSString stringWithFormat:@"Service %@ is not published", serviceName],
            @"code": @"NOT_PUBLISHED",
            @"domain": @"RNZeroconf",
            @"serviceName": serviceName,
        }];
        return;
    }

    // Deallocating the ref unregisters the service
    [self.publishedServices removeObjectForKey:serviceName];
    DNSServiceRefDeallocate(publication.ref);
    publication.ref = NULL;

    NSDictionary *serviceInfo = [self publicationInfo:publication];
    [self sendEvent:@"RNZeroconfServiceUnregistered" body:serviceInfo];
    resolve(serviceInfo);
}

- (BOOL) isNameInUse:(NSString *)name
{
    if (self.publishedServices[name]) {
        return YES;
    }
    for (RNZPublication *publication in self.pendingPublications) {
        if ([publication.name isEqualToString:name]) {
            return YES;
        }
    }
    return NO;
}

- (NSDictionary *) publicationInfo:(RNZPublication *)publication
{
    return @{
        kRNServiceKeysName: publication.name,
        kRNServiceKeysFullName: [NSString stringWithFormat:@"%@.%@.%@", publication.name, publication.regtype, publication.domain],
        kRNServiceKeysPort: @(publication.port),
        kRNServiceKeysAddresses: @[],
        kRNServiceTxtRecords: publication.txt ?: @{},
    };
}

#pragma mark - Local Network access

/**
 * Resolves 'granted', 'denied' or 'unknown'. There is no API to read the Local Network permission,
 * so this advertises a temporary Bonjour service and browses for it: seeing it means access is granted,
 * a browser waiting with kDNSServiceErr_PolicyDenied until the timeout means it is denied.
 * The type must be listed in NSBonjourServices, defaults to the first one. It can show the permission prompt.
 */
RCT_EXPORT_METHOD(checkLocalNetworkAccess:(NSString *)type
                  timeout:(double)timeout
                  resolve:(RCTPromiseResolveBlock)resolve
                  reject:(RCTPromiseRejectBlock)reject)
{
    NSString *serviceType = type;
    if (serviceType.length == 0) {
        NSArray *declared = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSBonjourServices"];
        serviceType = [declared isKindOfClass:[NSArray class]] ? declared.firstObject : nil;
    }
    if (serviceType.length == 0) {
        [self reject:reject error:@{
            @"message": @"No service type to check with, add one to NSBonjourServices in Info.plist",
            @"code": @"MISSING_BONJOUR_SERVICES",
            @"domain": @"RNZeroconf",
        }];
        return;
    }
    // "_http._tcp." -> "_http._tcp"
    if ([serviceType hasSuffix:@"."]) {
        serviceType = [serviceType substringToIndex:serviceType.length - 1];
    }

    NSString *name = [NSString stringWithFormat:@"rnzeroconf-%@", [[NSUUID UUID].UUIDString substringToIndex:8]];
    dispatch_queue_t queue = RNZ_QUEUE;

    nw_parameters_t parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION);
    nw_listener_t listener = nw_listener_create(parameters);
    nw_listener_set_advertise_descriptor(listener, nw_advertise_descriptor_create_bonjour_service(name.UTF8String, serviceType.UTF8String, "local."));
    nw_listener_set_queue(listener, queue);
    nw_listener_set_new_connection_handler(listener, ^(nw_connection_t connection) {
        nw_connection_cancel(connection);
    });

    nw_browse_descriptor_t descriptor = nw_browse_descriptor_create_bonjour_service(serviceType.UTF8String, "local.");
    nw_browser_t browser = nw_browser_create(descriptor, nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    nw_browser_set_queue(browser, queue);

    __block BOOL finished = NO;
    __block BOOL policyDenied = NO;
    void (^finish)(NSString *) = ^(NSString *result) {
        if (finished) {
            return;
        }
        finished = YES;
        nw_browser_cancel(browser);
        nw_listener_cancel(listener);
        resolve(result);
    };

    nw_browser_set_browse_results_changed_handler(browser, ^(nw_browse_result_t oldResult, nw_browse_result_t newResult, bool batchComplete) {
        nw_endpoint_t endpoint = newResult ? nw_browse_result_copy_endpoint(newResult) : nil;
        const char *found = endpoint ? nw_endpoint_get_bonjour_service_name(endpoint) : NULL;
        if (found && strcmp(found, name.UTF8String) == 0) {
            finish(@"granted");
        }
    });
    nw_browser_set_state_changed_handler(browser, ^(nw_browser_state_t state, nw_error_t error) {
        if (error && nw_error_get_error_domain(error) == nw_error_domain_dns) {
            // Stays waiting while the prompt is shown, only conclude on timeout
            policyDenied = nw_error_get_error_code(error) == kDNSServiceErr_PolicyDenied;
        } else if (state == nw_browser_state_ready) {
            policyDenied = NO;
        }
    });

    nw_listener_start(listener);
    nw_browser_start(browser);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)((timeout > 0 ? timeout : 5.0) * NSEC_PER_SEC)), queue, ^{
        finish(policyDenied ? @"denied" : @"unknown");
    });
}

#pragma mark - Teardown

// Called when the bridge is torn down (e.g. reload): stop scanning and unpublish services
- (void) invalidate
{
    [self stop:nil];
    for (RNZPublication *publication in self.publishedServices.allValues) {
        DNSServiceRefDeallocate(publication.ref);
        publication.ref = NULL;
    }
    [self.publishedServices removeAllObjects];
    for (RNZPublication *publication in self.pendingPublications) {
        DNSServiceRefDeallocate(publication.ref);
        publication.ref = NULL;
    }
    [self.pendingPublications removeAllObjects];
}

#pragma mark - Events and errors

- (void) sendEvent:(NSString *)name body:(id)body
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:name body:body];
}

// Scan events carry the id of the scan they belong to
- (void) sendEvent:(NSString *)name scan:(RNZScan *)scan body:(NSDictionary *)body
{
    NSMutableDictionary *withScan = [body mutableCopy];
    withScan[@"scanId"] = scan.scanId ?: @"";
    [self sendEvent:name body:withScan];
}

- (void) sendError:(NSDictionary *)error scan:(RNZScan *)scan
{
    NSMutableDictionary *withScan = [error mutableCopy];
    withScan[@"scanId"] = scan.scanId ?: @"";
    [self sendError:withScan];
}

// Emits an error event as { message, code, domain, serviceName }
- (void) sendError:(NSDictionary *)error
{
    [self sendEvent:@"RNZeroconfError" body:error];
}

// Rejects a promise with the same { message, code, domain, serviceName } shape as error events, in userInfo
- (void) reject:(RCTPromiseRejectBlock)reject error:(NSDictionary *)error
{
    id code = error[@"code"];
    NSError *nsError = [NSError errorWithDomain:error[@"domain"]
                                           code:[code isKindOfClass:[NSNumber class]] ? [code integerValue] : 0
                                       userInfo:error];
    reject([NSString stringWithFormat:@"%@", code], error[@"message"], nsError);
}

- (NSDictionary *) errorWithCode:(DNSServiceErrorType)code action:(NSString *)action serviceName:(NSString *)serviceName
{
    NSString *subject = serviceName ? [NSString stringWithFormat:@"%@ %@", action, serviceName] : action;
    NSMutableDictionary *error = [@{
        @"message": [NSString stringWithFormat:@"%@ failed: %@", subject, [RNZeroconf describeError:code]],
        @"code": @(code),
        @"domain": @"DNSSD",
    } mutableCopy];
    if (serviceName) {
        error[@"serviceName"] = serviceName;
    }
    return error;
}

// Readable description of a DNSServiceErrorType
+ (NSString *) describeError:(DNSServiceErrorType)code
{
    switch (code) {
        case kDNSServiceErr_PolicyDenied: return @"Local Network access denied, or the service type is missing from NSBonjourServices in Info.plist";
        case kDNSServiceErr_NameConflict: return @"name already in use";
        case kDNSServiceErr_BadParam: return @"bad parameter";
        case kDNSServiceErr_NoSuchName: return @"no such name";
        case kDNSServiceErr_NoSuchRecord: return @"no such record";
        case kDNSServiceErr_ServiceNotRunning: return @"mDNSResponder is not running";
        case kDNSServiceErr_Timeout: return @"timed out";
        case kDNSServiceErr_NoMemory: return @"out of memory";
        case kDNSServiceErr_Unsupported: return @"unsupported";
        default: return [NSString stringWithFormat:@"error %d", code];
    }
}

@end
