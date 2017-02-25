//
//  RNNetService.h
//  RNZeroconf
//
//  Created by Jeremy White on 7/1/2016.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT const NSString *kRNServiceKeysName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysFullName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysAddresses;
FOUNDATION_EXPORT const NSString *kRNServiceKeysHost;
FOUNDATION_EXPORT const NSString *kRNServiceKeysPort;
FOUNDATION_EXPORT const NSString *kRNServiceTxtRecords;

@interface RNNetServiceSerializer : NSObject

+ (NSDictionary *) serializeServiceToDictionary:(NSNetService *)service
                                       resolved:(BOOL)resolved;

@end
