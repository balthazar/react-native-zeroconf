//
//  RNZeroconf.m
//  RNZeroconf
//
//  Created by Balthazar on 25/10/2015.
//  Copyright © 2015 Balthazar. All rights reserved.
//

#import "RNZeroconf.h"

@implementation RNZeroconf

@synthesize bridge = _bridge;

#pragma mark - React Bridge Methods

RCT_EXPORT_MODULE()

RCT_EXPORT_METHOD(scan:(NSString *)type protocol:(NSString *)protocol domain:(NSString *)domain)
{
    [self.browser stop];
    [self.browser searchForServicesOfType:[NSString stringWithFormat:@"_%@._%@.", type, protocol] inDomain:domain];
}

RCT_EXPORT_METHOD(stop)
{
    [self.browser stop];
}

#pragma mark - NSNetServiceBrowserDelegate

// When a service is discovered.
- (void) netServiceBrowser:(NSNetServiceBrowser *)browser
            didFindService:(NSNetService *)service
                moreComing:(BOOL)moreComing
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfFound" body:service.name];
}

// When a service is removed.
- (void) netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
          didRemoveService:(NSNetService*)service
                moreComing:(BOOL)moreComing
{
    [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfRemove" body:service.name];
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

#pragma mark - Class methods

- (instancetype) init
{
    self = [super init];
    
    self.browser = [[NSNetServiceBrowser alloc] init];
    [self.browser setDelegate:self];
    
    return self;
}

- (void) reportError:(NSDictionary *)errorDict
{
    for (int a = 0; a < errorDict.count; ++a) {
        NSString *key = [[errorDict allKeys] objectAtIndex:a];
        NSString *val = [errorDict objectForKey:key];
        [self.bridge.eventDispatcher sendDeviceEventWithName:@"RNZeroconfError" body:val];
    }
}

@end