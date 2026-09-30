//
//  RNNetService.m
//  RNZeroconf
//
//  Created by Jeremy White on 7/1/2016.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import "RNNetServiceSerializer.h"
#include <arpa/inet.h>
#include <dns_sd.h>

const NSString *kRNServiceKeysName = @"name";
const NSString *kRNServiceKeysFullName = @"fullName";
const NSString *kRNServiceKeysAddresses = @"addresses";
const NSString *kRNServiceKeysHost = @"host";
const NSString *kRNServiceKeysPort = @"port";
const NSString *kRNServiceTxtRecords = @"txt";

@implementation RNNetServiceSerializer

+ (NSString *) stringFromAddress:(const struct sockaddr *)address
{
    if (address == NULL) {
        return nil;
    }

    char buffer[INET6_ADDRSTRLEN];
    const char *result = NULL;
    if (address->sa_family == AF_INET) {
        result = inet_ntop(AF_INET, &((const struct sockaddr_in *)address)->sin_addr, buffer, sizeof(buffer));
    } else if (address->sa_family == AF_INET6) {
        result = inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)address)->sin6_addr, buffer, sizeof(buffer));
    }
    return result ? [NSString stringWithUTF8String:result] : nil;
}

+ (NSDictionary<NSString *, NSString *> *) dictionaryFromTXTRecord:(const void *)record length:(uint16_t)length
{
    NSMutableDictionary *txt = [[NSMutableDictionary alloc] init];
    uint16_t count = TXTRecordGetCount(length, record);
    for (uint16_t i = 0; i < count; i++) {
        char key[256];
        uint8_t valueLength = 0;
        const void *value = NULL;
        if (TXTRecordGetItemAtIndex(length, record, i, sizeof(key), key, &valueLength, &value) != kDNSServiceErr_NoError || key[0] == '\0') {
            continue;
        }

        NSString *string = @"";
        if (value != NULL && valueLength > 0) {
            NSData *data = [NSData dataWithBytes:value length:valueLength];
            string = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]
                ?: [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding]
                ?: @"";
        }
        NSString *keyString = [NSString stringWithUTF8String:key];
        if (keyString) {
            txt[keyString] = string;
        }
    }
    return txt;
}

+ (NSData *) TXTRecordFromPairs:(NSArray<NSArray<NSString *> *> *)pairs tooLong:(NSMutableArray<NSString *> *)tooLong
{
    NSMutableData *data = [NSMutableData data];
    for (NSArray<NSString *> *pair in pairs) {
        if (pair.count != 2) {
            continue;
        }
        NSData *entry = [[NSString stringWithFormat:@"%@=%@", pair[0], pair[1]] dataUsingEncoding:NSUTF8StringEncoding];
        if (entry.length > 255) {
            [tooLong addObject:pair[0]];
            continue;
        }
        uint8_t entryLength = (uint8_t)entry.length;
        [data appendBytes:&entryLength length:1];
        [data appendData:entry];
    }
    return data;
}

@end
