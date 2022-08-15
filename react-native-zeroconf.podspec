require 'json'

package = JSON.parse(File.read(File.join(__dir__, 'package.json')))

Pod::Spec.new do |s|
  s.name         = package['name']
  s.version      = package['version']
  s.summary      = package['description']
  s.license      = package['license']

  s.authors      = package['author']
  s.homepage     = package['homepage']
  s.ios.deployment_target = '9.0'
  s.macos.deployment_target = '10.13'
  s.tvos.deployment_target = '9.0'
  
  s.source       = { :git => "https://github.com/balthazar/react-native-zeroconf.git", :tag => "v#{s.version}" }
  s.source_files  = "ios/**/*.{h,m}"

  s.dependency 'React-Core'
end
