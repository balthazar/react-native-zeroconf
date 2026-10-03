require 'json'

package = JSON.parse(File.read(File.join(__dir__, 'package.json')))

Pod::Spec.new do |s|
  s.name         = package['name']
  s.version      = package['version']
  s.summary      = package['description']
  s.license      = package['license']

  s.authors      = package['author']
  s.homepage     = package['homepage']
  # React Native's own minimum when available (RN 0.71+)
  s.ios.deployment_target = respond_to?(:min_ios_version_supported, true) ? min_ios_version_supported : '13.4'
  s.macos.deployment_target = '10.15'
  s.tvos.deployment_target = '13.4'

  s.source       = { :git => "https://github.com/balthazar/react-native-zeroconf.git", :tag => "#{s.version}" }
  # The C++ module (cpp/), its Apple registration and the code generated from src/NativeZeroconf.ts (ios/generated)
  s.source_files  = "ios/**/*.{h,m,mm}", "cpp/*.{h,cpp}", "cpp/dnssd/*.{h,cpp}", "cpp/apple/*.{h,cpp,mm}"
  s.frameworks = 'Network'

  if respond_to?(:install_modules_dependencies, true)
    install_modules_dependencies(s)
  else
    s.dependency 'React-Core'
  end
end
