//
//  RNZeroconf.h
//  RNZeroconf
//
//  Created by Balthazar on 25/10/2015.
//  Copyright © 2015 Balthazar. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RCTBridge.h"
#import "RCTBridgeModule.h"
#import "RCTEventDispatcher.h"

@interface RNZeroconf : NSObject <RCTBridgeModule>

@property (nonatomic, strong) NSMutableDictionary *hosts;
@property (nonatomic, strong) NSNetServiceBrowser *browser;

@end
