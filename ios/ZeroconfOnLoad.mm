// Registers the C++ module on Apple platforms: one dns_sd backend on a private queue, and the
// Local Network check of Network.framework
#import <Foundation/Foundation.h>
#import <ReactCommon/CxxTurboModuleUtils.h>

#include "../cpp/ZeroconfModule.h"
#include "../cpp/apple/DispatchExecutor.h"
#include "../cpp/apple/LocalNetworkAccess.h"
#include "../cpp/dnssd/DnssdBackend.h"

using namespace facebook::react;

@interface ZeroconfOnLoad : NSObject
@end

@implementation ZeroconfOnLoad

+ (void)load
{
  registerCxxModuleToGlobalModuleMap(
      std::string(ZeroconfModule::kModuleName),
      [](std::shared_ptr<CallInvoker> jsInvoker) {
        ZeroconfPlatform platform;
        // One mDNS stack on Apple platforms, implType is Android's
        platform.backendKey = [](const std::string &) { return std::string("dnssd"); };
        platform.createBackend = [](const std::string &, rnzeroconf::Events events) -> std::shared_ptr<rnzeroconf::Backend> {
          return rnzeroconf::DnssdBackend::Create(std::make_shared<rnzeroconf::DispatchExecutor>("com.balthazargronon.rnzeroconf"), std::move(events));
        };
        platform.checkLocalNetworkAccess = rnzeroconf::CheckLocalNetworkAccess;
        return std::make_shared<ZeroconfModule>(jsInvoker, std::move(platform));
      });
}

@end
