import Zeroconf from 'react-native-zeroconf'
const z = new Zeroconf()
async function f() {
  const a: 'granted' | 'denied' | 'unknown' = await z.checkLocalNetworkAccess()
  await z.checkLocalNetworkAccess({ type: 'http', timeout: 3 })
  // @ts-expect-error unknown option
  await z.checkLocalNetworkAccess({ tipe: 'http' })
  return a
}
f()
