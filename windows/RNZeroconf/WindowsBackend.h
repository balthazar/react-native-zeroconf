// The Windows backend of the C++ module: ZeroconfCore (windns.h) behind cpp/Backend.h.
// Declared without windows.h, which stays in WindowsBackend.cpp and ZeroconfCore.
#pragma once

#include "../../cpp/Backend.h"

#include <memory>

namespace rnzeroconf {

std::shared_ptr<Backend> CreateWindowsBackend(Events events);

} // namespace rnzeroconf
