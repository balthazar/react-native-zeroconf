
Pod::Spec.new do |s|
  s.name         = "RNZeroconf"
  s.version      = "1.0.0"
  s.summary      = "RNZeroconf"
  s.description  = "A Zeroconf discovery utility for react-native"
  s.homepage     = "https://github.com/balthazar/react-native-zeroconf"
  s.license      = "MIT"
  # s.license      = { :type => "MIT", :file => "FILE_LICENSE" }
  s.author             = { "author" => "author@domain.cn" }
  s.platform     = :ios, "7.0"
  s.source       = { :git => "https://github.com/balthazar/react-native-zeroconf.git", :tag => "master" }
  s.source_files  = "RNZeroconf/**/*.{h,m}"
  s.requires_arc = true


  s.dependency "React"
  #s.dependency "others"

end

