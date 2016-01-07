//
//  RNZeroconf.m
//  RNZeroconf
//
//  Created by Balthazar on 25/10/2015.
//  Copyright (c) 2016 Balthazar
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
//  documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
//  persons to whom the Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
//  Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
//  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
//  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#import "RNZeroconf.h"
#import "RNNetServiceSerializer.h"

@interface RNZeroconf ()

@property (nonatomic, strong) NSMutableDictionary *resolvingServices;

@end

@implementation RNZeroconf

@synthesize bridge = _bridge;

#pragma mark - React Bridge Methods

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

#pragma mark - NSNetServiceBrowserDelegate

// When a service is discovered.
- (void) netServiceBrowser:(NSNetServiceBrowser *)browser
            didFindService:(NSNetService *)service
                moreComing:(BOOL)moreComing
{
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

#pragma mark - Class methods

- (instancetype) init
{
    self = [super init];

    if (self)
    {
        _resolvingServices = [[NSMutableDictionary alloc] init];

        _browser = [[NSNetServiceBrowser alloc] init];
        [_browser setDelegate:self];
    }
    
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