//
//  RNNetService.m
//  RNZeroconf
//
//  Created by Jeremy White on 7/1/2016.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import "RNNetServiceSerializer.h"
#include <arpa/inet.h>

const NSString *kRNServiceKeysName = @"name";
const NSString *kRNServiceKeysFullName = @"fullName";
const NSString *kRNServiceKeysAddresses = @"addresses";
const NSString *kRNServiceKeysHost = @"host";
const NSString *kRNServiceKeysPort = @"port";
const NSString *kRNServiceTxtRecords = @"txt";

@implementation RNNetServiceSerializer

+ (NSDictionary *) serializeServiceToDictionary:(NSNetService *)service
                                       resolved:(BOOL)resolved
{
    NSMutableDictionary *serviceInfo = [[NSMutableDictionary alloc] init];
    serviceInfo[kRNServiceKeysName] = service.name;

    if (resolved) {
        serviceInfo[kRNServiceKeysFullName] = [NSString stringWithFormat:@"%@%@", service.hostName, service.type];
        serviceInfo[kRNServiceKeysAddresses] = [self addressesFromService:service];
        serviceInfo[kRNServiceKeysHost] = service.hostName;
        serviceInfo[kRNServiceKeysPort] = @(service.port);
        
        NSDictionary<NSString *, NSData *> *txtRecordDict = [NSNetService dictionaryFromTXTRecordData:service.TXTRecordData];
        
        NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
        for (NSString *key in txtRecordDict) {
            @try{
                dict[key] = [[NSString alloc]
                            initWithData:txtRecordDict[key]
                            encoding:NSASCIIStringEncoding];
            }
            @catch(NSException *exception){
                NSLog(@"%@", exception);
            }
        }
        serviceInfo[kRNServiceTxtRecords] = dict;
    }

    return [NSDictionary dictionaryWithDictionary:serviceInfo];
}

+ (NSArray<NSString *> *) addressesFromService:(NSNetService *)service
{
    NSMutableArray<NSString *> *addresses = [[NSMutableArray alloc] init];

    // source: http://stackoverflow.com/a/4976808/2715
    char addressBuffer[INET6_ADDRSTRLEN];

    for (NSData *data in service.addresses) {
        memset(addressBuffer, 0, INET6_ADDRSTRLEN);

        typedef union {
            struct sockaddr sa;
            struct sockaddr_in ipv4;
            struct sockaddr_in6 ipv6;
        } ip_socket_address;

        ip_socket_address *socketAddress = (ip_socket_address *)[data bytes];

        if (socketAddress && (socketAddress->sa.sa_family == AF_INET || socketAddress->sa.sa_family == AF_INET6)) {
            const char *addressStr = inet_ntop(
                socketAddress->sa.sa_family,
                (socketAddress->sa.sa_family == AF_INET ? (void *)&(socketAddress->ipv4.sin_addr) : (void *)&(socketAddress->ipv6.sin6_addr)),
                addressBuffer,
                sizeof(addressBuffer)
            );

            if (addressStr) {
                NSString *address = [NSString stringWithUTF8String:addressStr];
                [addresses addObject:address];
            }
        }
    }

    return [NSArray arrayWithArray:addresses];
}

@end
