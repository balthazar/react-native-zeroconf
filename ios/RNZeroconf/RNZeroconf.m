//
//  RNZeroconf.m
//  RNZeroconf
//
//  Created by Balthazar Gronon on 25/10/2015.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import "RNZeroconf.h"
#import "RNNetServiceSerializer.h"

@interface RNZeroconf ()

@property (nonatomic, strong, readonly) NSMutableDictionary *resolvingServices;
@property (nonatomic, strong, readonly) NSMutableDictionary *publishedServices;

@end

@implementation RNZeroconf

@synthesize bridge = _bridge;

RCT_EXPORT_MODULE()

RCT_EXPORT_METHOD(scan:(NSString *)type protocol:(NSString *)protocol domain:(NSString *)domain)
{
    [self stop];
    [self.browser searchForServicesOfType:[NSString stringWithFormat:@"_%@._%@.", type, protocol] inDomain:domain];
}

RCT_EXPORT_METHOD(stop)
{
    [self.browser stop];
    [self.resolvingServices removeAllObjects];
}

- (dispatch_queue_t)methodQueue
{
    return dispatch_get_main_queue();
}

+ (BOOL)requiresMainQueueSetup
{
    return YES;
}

RCT_EXPORT_METHOD(registerService:(NSString *)type
                  protocol:(NSString *)protocol
                  domain:(NSString *)domain
                  name:(NSString *)name
                  port:(int)port
                  txt:(NSArray<NSArray<NSString *> *> *)txt)
{
    const NSNetService *svc = [[NSNetService alloc] initWithDomain:domain type:[NSString stringWithFormat:@"_%@._%@.", type, protocol] name:name port:port];
    [svc setDelegate:self];
    [svc scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];

    if (txt.count > 0) {
      [svc setTXTRecordData:[self TXTRecordDataFromPairs:txt]];
    }

    [svc publish];
    self.publishedServices[svc.name] = svc;
    NSLog(@"zeroconf publish called");
}

RCT_EXPORT_METHOD(unregisterService:(NSString *) serviceName)
{
    NSNetService *svc = self.publishedServices[serviceName];

    if (svc) {
        [svc stop];
    }
}

#pragma mark - NSNetServiceBrowserDelegate

// When a service is discovered.
- (void) netServiceBrowser:(NSNetServiceBrowser *)browser
            didFindService:(NSNetService *)service
                moreComing:(BOOL)moreComing
{
    if (service == nil) {
      return;
    }

    NSDictionary *serviceInfo = [RNNetServiceSerializer serializeServiceToDictionary:service resolved:NO];
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfFound" body:serviceInfo];

    // resolving services must be strongly referenced or they will be garbage collected
    // and will never resolve or timeout.
    // source: http://stackoverflow.com/a/16130535/2715
    self.resolvingServices[service.name] = service;

    service.delegate = self;
    [service resolveWithTimeout:5.0];
}

// When a service is removed.
- (void) netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
          didRemoveService:(NSNetService*)service
                moreComing:(BOOL)moreComing
{
    if (service == nil) {
      return;
    }

    // Stop any pending resolve for the removed service
    NSNetService *resolving = self.resolvingServices[service.name];
    if (resolving) {
        resolving.delegate = nil;
        [resolving stop];
        [self.resolvingServices removeObjectForKey:service.name];
    }

    NSDictionary *serviceInfo = [RNNetServiceSerializer serializeServiceToDictionary:service resolved:NO];
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfRemove" body:serviceInfo];
}

// When the search fails.
- (void) netServiceBrowser:(NSNetServiceBrowser *)browser
              didNotSearch:(NSDictionary *)errorDict
{
    [self reportError:errorDict];
}

// When the search stops.
- (void) netServiceBrowserDidStopSearch:(NSNetServiceBrowser *)browser
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfStop" body:nil];
}

// When the search starts.
- (void) netServiceBrowserWillSearch:(NSNetServiceBrowser *)browser
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfStart" body:nil];
}

#pragma mark - NSNetServiceDelegate

// When the service has resolved it's network data (IP addresses, etc)
- (void) netServiceDidResolveAddress:(NSNetService *)sender
{
    NSDictionary *serviceInfo = [RNNetServiceSerializer serializeServiceToDictionary:sender resolved:YES];
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfResolved" body:serviceInfo];

    sender.delegate = nil;
    [self.resolvingServices removeObjectForKey:sender.name];
}

// When the service has failed to resolve it's network data (IP addresses, etc)
- (void) netService:(NSNetService *)sender
      didNotResolve:(NSDictionary *)errorDict
{
    [self reportError:errorDict];

    sender.delegate = nil;
    [self.resolvingServices removeObjectForKey:sender.name];
}

- (void)netServiceWillPublish:(NSNetService *)sender
{
    NSLog(@"zeroconf netServiceWillPublish");
}

// When a service is successfully published
- (void)netServiceDidPublish:(NSNetService *)sender
{
    NSLog(@"zeroconf netServiceDidPublish");
    NSDictionary *serviceInfo = [RNNetServiceSerializer serializeServiceToDictionary:sender resolved:YES];
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfServiceRegistered" body:serviceInfo];

    self.publishedServices[sender.name] = sender;

}

- (void)netService:(NSNetService *)sender
     didNotPublish:(NSDictionary<NSString *,NSNumber *> *)errorDict
{
    NSLog(@"zeroconf netServiceDidNotPublish");

    [self reportError:errorDict];
    NSLog(@"zeroconf %@", errorDict);
    sender.delegate = nil;
    [self.publishedServices removeObjectForKey:sender.name];

}

- (void)netServiceDidStop:(NSNetService *)sender
{
    sender.delegate = nil;
    [self.publishedServices removeObjectForKey:sender.name];

    NSDictionary *serviceInfo = [RNNetServiceSerializer serializeServiceToDictionary:sender resolved:YES];
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfServiceUnregistered" body:serviceInfo];

}

#pragma mark - Class methods

- (instancetype) init
{
    self = [super init];

    if (self) {
        _resolvingServices = [[NSMutableDictionary alloc] init];
        _publishedServices = [[NSMutableDictionary alloc] init];
        _browser = [[NSNetServiceBrowser alloc] init];
        [_browser setDelegate:self];
    }

    return self;
}

// Builds the TXT record in the given order, unlike dataFromTXTRecordDictionary
- (NSData *) TXTRecordDataFromPairs:(NSArray<NSArray<NSString *> *> *)pairs
{
    NSMutableData *data = [NSMutableData data];
    for (NSArray<NSString *> *pair in pairs) {
        if (pair.count != 2) {
            continue;
        }
        NSData *entry = [[NSString stringWithFormat:@"%@=%@", pair[0], pair[1]] dataUsingEncoding:NSUTF8StringEncoding];
        if (entry.length > 255) {
            [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfError" body:[NSString stringWithFormat:@"TXT record entry %@ is longer than 255 bytes", pair[0]]];
            continue;
        }
        uint8_t length = (uint8_t)entry.length;
        [data appendBytes:&length length:1];
        [data appendData:entry];
    }
    return data;
}

// Called when the bridge is torn down (e.g. reload): stop scanning and unpublish services
- (void) invalidate
{
    [self.browser stop];
    for (NSNetService *service in self.resolvingServices.allValues) {
        service.delegate = nil;
        [service stop];
    }
    [self.resolvingServices removeAllObjects];
    for (NSNetService *service in self.publishedServices.allValues) {
        service.delegate = nil;
        [service stop];
    }
    [self.publishedServices removeAllObjects];
}

- (void) reportError:(NSDictionary *)errorDict
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfError" body:[NSString stringWithFormat:@"%@",errorDict]];
}

@end
