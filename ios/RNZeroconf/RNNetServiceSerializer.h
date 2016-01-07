//
//  RNNetService.h
//  RNZeroconf
//
//  Created by Jeremy White on 1/7/16.
//  Copyright © 2016 Balthazar. All rights reserved.
//

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT const NSString *kRNServiceKeysName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysFullName;
FOUNDATION_EXPORT const NSString *kRNServiceKeysAddresses;
FOUNDATION_EXPORT const NSString *kRNServiceKeysHost;
FOUNDATION_EXPORT const NSString *kRNServiceKeysPort;

@interface RNNetServiceSerializer : NSObject

+ (NSDictionary *) serializeServiceToDictionary:(NSNetService *)service
                                        resolved:(BOOL)resolved;

@end
