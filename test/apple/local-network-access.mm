// Harness for the Local Network check (cpp/apple/LocalNetworkAccess.mm), run on macOS
#import <Foundation/Foundation.h>

#include "../../cpp/apple/LocalNetworkAccess.h"

#include <cstdio>
#include <string>

using namespace rnzeroconf;

int main() {
  @autoreleasepool {
    std::string result;
    std::string rejected;
    NSDate *start = [NSDate date];
    CheckLocalNetworkAccess(
        "_zctest._tcp.", 5, [&](const std::string &status) { result = status; }, [&](const Error &error) { rejected = error.code; });
    while (result.empty() && rejected.empty() && -[start timeIntervalSinceNow] < 8) {
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    }
    printf("with a type: %s after %.2fs\n", (result.empty() ? rejected : result).c_str(), -[start timeIntervalSinceNow]);
    // The answer depends on the machine's Local Network settings, any of the three is valid
    bool typedOK = result == "granted" || result == "denied" || result == "unknown";

    result.clear();
    rejected.clear();
    CheckLocalNetworkAccess(
        "", 5, [&](const std::string &status) { result = status; }, [&](const Error &error) { rejected = error.code; });
    printf("no type, no Info.plist: %s\n", (result.empty() ? rejected : result).c_str());
    bool missingOK = rejected == "MISSING_BONJOUR_SERVICES";

    printf("%s\n", typedOK && missingOK ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return typedOK && missingOK ? 0 : 1;
  }
}
