// The Local Network check of Apple platforms, with Network.framework. No React Native dependency:
// ios/ZeroconfOnLoad.mm hands it to the module, test/apple/local-network-access.mm tests it.
#import "LocalNetworkAccess.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#include <dns_sd.h>

namespace rnzeroconf {

/**
 * Resolves 'granted', 'denied' or 'unknown'. There is no API to read the Local Network permission,
 * so this advertises a temporary Bonjour service and browses for it: seeing it means access is granted,
 * a browser waiting with kDNSServiceErr_PolicyDenied until the timeout means it is denied.
 * The type must be listed in NSBonjourServices, defaults to the first one. It can show the permission prompt.
 */
void CheckLocalNetworkAccess(
    const std::string &type,
    double timeout,
    std::function<void(const std::string &status)> resolve,
    std::function<void(const Error &error)> reject) {
  NSString *serviceType = type.empty() ? nil : [NSString stringWithUTF8String:type.c_str()];
  if (serviceType.length == 0) {
    NSArray *declared = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSBonjourServices"];
    serviceType = [declared isKindOfClass:[NSArray class]] ? declared.firstObject : nil;
  }
  if (serviceType.length == 0) {
    reject(LibraryError("MISSING_BONJOUR_SERVICES", "No service type to check with, add one to NSBonjourServices in Info.plist"));
    return;
  }
  // "_http._tcp." -> "_http._tcp"
  if ([serviceType hasSuffix:@"."]) {
    serviceType = [serviceType substringToIndex:serviceType.length - 1];
  }

  NSString *name = [NSString stringWithFormat:@"rnzeroconf-%@", [[NSUUID UUID].UUIDString substringToIndex:8]];
  dispatch_queue_t queue = dispatch_get_main_queue();

  nw_parameters_t parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION);
  nw_listener_t listener = nw_listener_create(parameters);
  nw_listener_set_advertise_descriptor(listener, nw_advertise_descriptor_create_bonjour_service(name.UTF8String, serviceType.UTF8String, "local."));
  nw_listener_set_queue(listener, queue);
  nw_listener_set_new_connection_handler(listener, ^(nw_connection_t connection) {
    nw_connection_cancel(connection);
  });

  nw_browse_descriptor_t descriptor = nw_browse_descriptor_create_bonjour_service(serviceType.UTF8String, "local.");
  nw_browser_t browser = nw_browser_create(descriptor, nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
  nw_browser_set_queue(browser, queue);

  __block BOOL finished = NO;
  __block BOOL policyDenied = NO;
  void (^finish)(NSString *) = ^(NSString *result) {
    if (finished) {
      return;
    }
    finished = YES;
    nw_browser_cancel(browser);
    nw_listener_cancel(listener);
    resolve(result.UTF8String);
  };

  nw_browser_set_browse_results_changed_handler(browser, ^(nw_browse_result_t, nw_browse_result_t newResult, bool) {
    nw_endpoint_t endpoint = newResult ? nw_browse_result_copy_endpoint(newResult) : nil;
    const char *found = endpoint ? nw_endpoint_get_bonjour_service_name(endpoint) : NULL;
    if (found && strcmp(found, name.UTF8String) == 0) {
      finish(@"granted");
    }
  });
  nw_browser_set_state_changed_handler(browser, ^(nw_browser_state_t state, nw_error_t error) {
    if (error && nw_error_get_error_domain(error) == nw_error_domain_dns) {
      // Stays waiting while the prompt is shown, only conclude on timeout
      policyDenied = nw_error_get_error_code(error) == kDNSServiceErr_PolicyDenied;
    } else if (state == nw_browser_state_ready) {
      policyDenied = NO;
    }
  });

  nw_listener_start(listener);
  nw_browser_start(browser);

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)((timeout > 0 ? timeout : 5.0) * NSEC_PER_SEC)), queue, ^{
    finish(policyDenied ? @"denied" : @"unknown");
  });
}

} // namespace rnzeroconf
