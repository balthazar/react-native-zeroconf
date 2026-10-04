// The C++ module (src/NativeZeroconf.ts) in the shapes the class works with: TXT records as pairs,
// error codes as numbers for the platform domains, and promises rejecting with { userInfo }
import NativeZeroconf, {
  type NativeError,
  type NativeResult,
  type NativeService,
  type TxtEntry,
} from './NativeZeroconf'
import type { ImplType, PublishedService, Service, ZeroconfError } from './types'

export type LocalNetworkAccess = 'granted' | 'denied' | 'unknown'

export type BridgeEvent =
  'start' | 'stop' | 'found' | 'remove' | 'resolved' | 'error' | 'published' | 'unpublished'

// Event payloads as the class handles them: scan events carry their scan id, errors are
// { message, code, domain, serviceName }
export interface BridgePayload {
  scanId?: string
  name?: string
  [key: string]: unknown
}

export interface Bridge {
  scan(
    scanId: string,
    type: string,
    protocol: string,
    domain: string,
    implType: ImplType,
    options: Record<string, unknown>,
  ): void
  stop(scanId: string, implType: ImplType): void
  registerService(
    type: string,
    protocol: string,
    domain: string,
    name: string,
    port: number,
    txt: Array<[string, string]>,
    implType: ImplType,
    options: Record<string, unknown>,
  ): Promise<PublishedService>
  updateService(
    name: string,
    txt: Array<[string, string]>,
    implType: ImplType,
  ): Promise<PublishedService>
  unregisterService(name: string, implType: ImplType): Promise<PublishedService | null>
  resolveService(
    name: string,
    type: string,
    protocol: string,
    domain: string,
    implType: ImplType,
    options: Record<string, unknown>,
  ): Promise<Omit<Service, 'ipv4' | 'ipv6'> | null>
  // iOS: a service type from NSBonjourServices to check with. Android: the permission check
  checkLocalNetworkAccess(type: string | null, timeout: number): Promise<LocalNetworkAccess>
  listen(event: BridgeEvent, handler: (payload: BridgePayload) => void): { remove(): void }
}

// The C++ module

// Codes are strings natively: numbers for the platform domains ('-65555'), names for the library's ('TIMEOUT')
const fromNativeError = (error: NativeError) => ({
  message: error.message,
  code: error.domain === 'RNZeroconf' ? error.code : Number(error.code),
  domain: error.domain as ZeroconfError['domain'],
  ...(error.serviceName ? { serviceName: error.serviceName } : {}),
})

const fromNativeService = (service: NativeService): Omit<Service, 'ipv4' | 'ipv6'> => ({
  name: service.name,
  fullName: service.fullName,
  host: service.host,
  port: service.port,
  addresses: service.addresses,
  txt: Object.fromEntries(service.txt.map(({ key, value }) => [key, value])),
})

const toTxtEntries = (txt: Array<[string, string]>): TxtEntry[] =>
  txt.map(([key, value]) => ({ key, value }))

// A rejection carries the same payload as error events, in userInfo
const settled = async (result: Promise<NativeResult>) => {
  const { service, error } = await result
  if (error) {
    throw { userInfo: fromNativeError(error) }
  }
  return service ? fromNativeService(service) : null
}

const turboBridge = (module: NonNullable<typeof NativeZeroconf>): Bridge => ({
  scan: (scanId, type, protocol, domain, implType, options) =>
    module.scan(scanId, type, protocol, domain, implType, options),
  stop: (scanId, implType) => module.stop(scanId, implType),
  registerService: async (type, protocol, domain, name, port, txt, implType, options) =>
    (await settled(
      module.registerService(
        type,
        protocol,
        domain,
        name,
        port,
        toTxtEntries(txt),
        implType,
        options,
      ),
    )) as PublishedService,
  updateService: async (name, txt, implType) =>
    (await settled(module.updateService(name, toTxtEntries(txt), implType))) as PublishedService,
  unregisterService: async (name, implType) =>
    (await settled(module.unregisterService(name, implType))) as PublishedService | null,
  resolveService: (name, type, protocol, domain, implType, options) =>
    settled(module.resolveService(name, type, protocol, domain, implType, options)),
  checkLocalNetworkAccess: async (type, timeout) => {
    const { status, error } = await module.checkLocalNetworkAccess(type ?? '', timeout)
    if (error) {
      throw { userInfo: fromNativeError(error) }
    }
    return (status ?? 'unknown') as LocalNetworkAccess
  },
  listen: (event, handler) => {
    // An empty scan id means the event is not about a scan: it reaches every instance
    const withScan = (scanId: string, payload: BridgePayload) =>
      handler(scanId ? { ...payload, scanId } : payload)
    switch (event) {
      case 'start':
        return module.onStart(({ scanId }) => withScan(scanId, {}))
      case 'stop':
        return module.onStop(({ scanId }) => withScan(scanId, {}))
      case 'found':
        return module.onFound(({ scanId, name }) => withScan(scanId, { name }))
      case 'remove':
        return module.onRemove(({ scanId, name }) => withScan(scanId, { name }))
      case 'resolved':
        return module.onResolved(({ scanId, service }) =>
          withScan(scanId, { ...fromNativeService(service) }),
        )
      case 'error':
        return module.onError(({ scanId, error }) =>
          withScan(scanId, { ...fromNativeError(error) }),
        )
      case 'published':
        return module.onPublished(service => handler({ ...fromNativeService(service) }))
      case 'unpublished':
        return module.onUnpublished(service => handler({ ...fromNativeService(service) }))
    }
  },
})

export const getBridge = (): Bridge | null => (NativeZeroconf ? turboBridge(NativeZeroconf) : null)
