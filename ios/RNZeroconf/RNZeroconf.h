//
//  RNZeroconf.h
//  RNZeroconf
//
//  Created by Balthazar Gronon on 25/10/2015.
//  Copyright © 2016 Balthazar Gronon MIT
//

#import <Foundation/Foundation.h>
#import <React/RCTBridge.h>
#import <React/RCTBridgeModule.h>
#import <React/RCTEventDispatcher.h>

@interface RNZeroconf : NSObject <RCTBridgeModule, NSNetServiceBrowserDelegate, NSNetServiceDelegate>

@property (nonatomic, strong) NSNetServiceBrowser *browser;

@end
