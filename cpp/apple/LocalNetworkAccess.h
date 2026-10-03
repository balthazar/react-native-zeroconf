// The Local Network permission on Apple platforms
#pragma once

#include "../Backend.h"

#include <functional>
#include <string>

namespace rnzeroconf {

// Settles with 'granted', 'denied' or 'unknown', on the main queue. type is a service type from
// NSBonjourServices ("_http._tcp"), the first one listed when empty. Can show the permission prompt
void CheckLocalNetworkAccess(
    const std::string &type,
    double timeout,
    std::function<void(const std::string &status)> resolve,
    std::function<void(const Error &error)> reject);

} // namespace rnzeroconf
