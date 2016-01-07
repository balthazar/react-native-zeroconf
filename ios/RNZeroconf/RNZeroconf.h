//
//  RNZeroconf.h
//  RNZeroconf
//
//  Created by Balthazar on 25/10/2015.
//  Copyright © 2016 Balthazar MIT
//

#import <Foundation/Foundation.h>
#import "RCTBridge.h"
#import "RCTBridgeModule.h"
#import "RCTEventDispatcher.h"

@interface RNZeroconf : NSObject <RCTBridgeModule, NSNetServiceDelegate>

@property (nonatomic, strong) NSNetServiceBrowser *browser;

@end
