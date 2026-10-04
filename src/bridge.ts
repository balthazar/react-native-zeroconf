// One interface over the native side: the C++ module (src/NativeZeroconf.ts) where it is registered,
// the previous native module (NativeModules.RNZeroconf with device events) elsewhere
import { DeviceEventEmitter, NativeModules, Platform } from 'react-native'

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
  // Android: no arguments, the permission check. Undefined on old Android modules without it
  checkLocalNetworkAccess?(type: string | null, timeout: number): Promise<LocalNetworkAccess>
  listen(event: BridgeEvent, handler: (payload: BridgePayload) => void): { remove(): void }
}

// The previous native module
interface LegacyModule {
  scan(...args: unknown[]): void
  stop(...args: unknown[]): void
  registerService(...args: unknown[]): Promise<PublishedService>
  updateService(...args: unknown[]): Promise<PublishedService>
  resolveService(...args: unknown[]): Promise<Omit<Service, 'ipv4' | 'ipv6'> | null>
  unregisterService(...args: unknown[]): Promise<PublishedService | null>
  checkLocalNetworkAccess?(...args: unknown[]): Promise<LocalNetworkAccess>
}

const LEGACY_EVENTS: Record<BridgeEvent, string> = {
  start: 'RNZeroconfStart',
  stop: 'RNZeroconfStop',
  found: 'RNZeroconfFound',
  remove: 'RNZeroconfRemove',
  resolved: 'RNZeroconfResolved',
  error: 'RNZeroconfError',
  published: 'RNZeroconfServiceRegistered',
  unpublished: 'RNZeroconfServiceUnregistered',
}

// The previous module takes implType on Android only
const legacyBridge = (module: LegacyModule): Bridge => {
  const isAndroid = () => Platform.OS === 'android'
  return {
    scan: (scanId, type, protocol, domain, implType, options) =>
      isAndroid()
        ? module.scan(scanId, type, protocol, domain, implType, options)
        : module.scan(scanId, type, protocol, domain, options),
    stop: (scanId, implType) => (isAndroid() ? module.stop(scanId, implType) : module.stop(scanId)),
    registerService: (type, protocol, domain, name, port, txt, implType, options) =>
      isAndroid()
        ? module.registerService(type, protocol, domain, name, port, txt, implType, options)
        : module.registerService(type, protocol, domain, name, port, txt, options),
    updateService: (name, txt, implType) =>
      isAndroid() ? module.updateService(name, txt, implType) : module.updateService(name, txt),
    unregisterService: (name, implType) =>
      isAndroid() ? module.unregisterService(name, implType) : module.unregisterService(name),
    resolveService: (name, type, protocol, domain, implType, options) =>
      isAndroid()
        ? module.resolveService(name, type, protocol, domain, implType, options)
        : module.resolveService(name, type, protocol, domain, options),
    checkLocalNetworkAccess: module.checkLocalNetworkAccess
      ? (type, timeout) =>
          isAndroid()
            ? module.checkLocalNetworkAccess!()
            : module.checkLocalNetworkAccess!(type, timeout)
      : undefined,
    listen: (event, handler) => DeviceEventEmitter.addListener(LEGACY_EVENTS[event], handler),
  }
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

// A rejection carries the same payload as error events, in userInfo, as from the previous module
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
    switch (event) {
      case 'start':
        return module.onStart(handler)
      case 'stop':
        return module.onStop(handler)
      case 'found':
        return module.onFound(handler)
      case 'remove':
        return module.onRemove(handler)
      case 'resolved':
        return module.onResolved(({ scanId, service }) =>
          handler({ ...fromNativeService(service), scanId }),
        )
      case 'error':
        return module.onError(({ scanId, error }) =>
          handler({ ...fromNativeError(error), ...(scanId ? { scanId } : {}) }),
        )
      case 'published':
        return module.onPublished(service => handler({ ...fromNativeService(service) }))
      case 'unpublished':
        return module.onUnpublished(service => handler({ ...fromNativeService(service) }))
    }
  },
})

export const getBridge = (): Bridge | null => {
  if (NativeZeroconf) {
    return turboBridge(NativeZeroconf)
  }
  const legacy: LegacyModule | undefined = NativeModules.RNZeroconf
  return legacy ? legacyBridge(legacy) : null
}
