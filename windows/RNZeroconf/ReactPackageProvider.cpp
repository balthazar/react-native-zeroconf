#include "pch.h"

#include "ReactPackageProvider.h"
#if __has_include("ReactPackageProvider.g.cpp")
#include "ReactPackageProvider.g.cpp"
#endif

#include <TurboModuleProvider.h>

#include "ZeroconfWindowsModule.h"

using namespace winrt::Microsoft::ReactNative;

namespace winrt::RNZeroconf::implementation
{

void ReactPackageProvider::CreatePackage(IReactPackageBuilder const &packageBuilder) noexcept
{
  // The C++ module of every platform (cpp/ZeroconfModule), with the windns.h backend
  AddTurboModuleProvider<facebook::react::ZeroconfWindowsModule>(packageBuilder, L"Zeroconf");
}

} // namespace winrt::RNZeroconf::implementation
