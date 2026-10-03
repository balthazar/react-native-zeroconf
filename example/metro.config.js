// Runs the example against the library source in ../src, no build needed
const path = require('path')
const { getDefaultConfig } = require('expo/metro-config')

const root = path.resolve(__dirname, '..')
const config = getDefaultConfig(__dirname)

config.watchFolders = [root]
config.resolver.resolveRequest = (context, moduleName, platform) => {
  if (moduleName === 'react-native-zeroconf') {
    return { type: 'sourceFile', filePath: path.join(root, 'src/index.ts') }
  }
  // The library's own node_modules has another React version, use the example's
  if (/^react(-native)?($|\/)/.test(moduleName)) {
    return context.resolveRequest({ ...context, originModulePath: path.join(__dirname, 'index.ts') }, moduleName, platform)
  }
  return context.resolveRequest(context, moduleName, platform)
}

module.exports = config
