//
//  RNNetService.h
//  RNZeroconf
//
//  Created by Jeremy White on 7/1/2016.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import <Foundation/Foundation.h>
#include <sys/socket.h>

FOUNDATION_EXPORT const NSString *kRNServiceKeysName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysFullName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysAddresses;
FOUNDATION_EXPORT const NSString *kRNServiceKeysHost;
FOUNDATION_EXPORT const NSString *kRNServiceKeysPort;
FOUNDATION_EXPORT const NSString *kRNServiceTxtRecords;

// Converts dns_sd results to the service dictionaries sent to JS
@interface RNNetServiceSerializer : NSObject

// "192.168.1.2" or "fe80::1", nil for other address families
+ (NSString *) stringFromAddress:(const struct sockaddr *)address;

// TXT record bytes to { key: value }, values decoded as UTF-8 (Latin-1 when not valid UTF-8)
+ (NSDictionary<NSString *, NSString *> *) dictionaryFromTXTRecord:(const void *)record length:(uint16_t)length;

// Ordered [key, value] pairs to TXT record bytes, keys of entries longer than 255 bytes are skipped and added to tooLong
+ (NSData *) TXTRecordFromPairs:(NSArray<NSArray<NSString *> *> *)pairs tooLong:(NSMutableArray<NSString *> *)tooLong;

@end
