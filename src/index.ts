import { useCallback, useEffect, useRef, useState } from 'react'
import { Platform, NativeModules, DeviceEventEmitter, PermissionsAndroid } from 'react-native'
import type { EmitterSubscription } from 'react-native'
import { EventEmitter } from 'events'

import {
  ImplType,
  type LocalNetworkAccessOptions,
  type PublishedService,
  type PublishOptions,
  type ResolveOptions,
  type ScanOptions,
  type Service,
  type ServiceType,
  type ServiceTypesScanOptions,
  type TxtRecord,
  type UpdateOptions,
  type UseServiceTypesOptions,
  type UseServiceTypesResult,
  type UseZeroconfOptions,
  type UseZeroconfResult,
  type ZeroconfError,
  type ZeroconfEvents,
} from './types'

export * from './types'

type LocalNetworkAccess = 'granted' | 'denied' | 'unknown'

// The native module of iOS, Android and Windows. Android takes an implType after the service type arguments
interface NativeZeroconf {
  scan(...args: unknown[]): void
  stop(...args: unknown[]): void
  registerService(...args: unknown[]): Promise<PublishedService>
  updateService(...args: unknown[]): Promise<PublishedService>
  resolveService(...args: unknown[]): Promise<Service | null>
  unregisterService(...args: unknown[]): Promise<PublishedService | null>
  checkLocalNetworkAccess?(...args: unknown[]): Promise<LocalNetworkAccess>
}

// Payloads of the native events, scan events carry the id of the scan they belong to
interface NativePayload {
  scanId?: string
  name?: string
}

interface NativeErrorPayload extends NativePayload {
  message: string
  code: number | string
  domain: ZeroconfError['domain']
  serviceName?: string
}

const RNZeroconf: NativeZeroconf | undefined = NativeModules.RNZeroconf

const SCAN_DEFAULTS = {
  type: 'http',
  protocol: 'tcp',
  domain: 'local.',
  implType: ImplType.NSD as ImplType,
  resolveTimeout: 5,
}

const PUBLISH_DEFAULTS = {
  domain: 'local.',
  txt: {} as TxtRecord,
  implType: ImplType.NSD as ImplType,
}

/**
 * Merge options over defaults, ignoring undefined values
 */
const withDefaults = <D extends object, O extends object>(defaults: D, options: O): D & O => {
  const merged: Record<string, unknown> = { ...(defaults as Record<string, unknown>) }
  Object.entries(options).forEach(([key, value]) => {
    if (value !== undefined) {
      merged[key] = value
    }
  })
  return merged as D & O
}

const isOptionsObject = (value: unknown): value is object =>
  value !== null && typeof value === 'object'

// Options passed to native code, without the undefined ones
const nativeOptions = (options: Record<string, unknown>) =>
  Object.fromEntries(Object.entries(options).filter(([, value]) => value !== undefined))

// Android 17 (API 37) runtime permission, enforced for apps targeting API 37 or declaring it
const ACCESS_LOCAL_NETWORK = 'android.permission.ACCESS_LOCAL_NETWORK'

// Each instance runs its own scan, identified by this id in native events
let instanceCount = 0

/**
 * Error from a native { message, code, domain, serviceName } payload
 */
const toError = (payload: unknown): ZeroconfError | Error => {
  if (!payload || typeof payload !== 'object') {
    return new Error(String(payload))
  }
  const { message, code, domain, serviceName } = payload as NativeErrorPayload
  const error = new Error(message) as ZeroconfError
  error.code = code
  error.domain = domain
  if (serviceName) {
    error.serviceName = serviceName
  }
  return error
}

/**
 * Promise rejections carry the same payload as error events in userInfo
 */
const toRejection = (error: unknown) => {
  const userInfo = (error as { userInfo?: { domain?: unknown } } | null)?.userInfo
  return userInfo && userInfo.domain ? toError(userInfo) : error
}

/**
 * Normalizes rejections, and marks the promise handled so callers that don't await it
 * don't get unhandled rejection warnings (errors are also emitted as error events)
 */
const asPromise = <T>(nativePromise: T | Promise<T>): Promise<T> => {
  const promise = Promise.resolve(nativePromise).catch(error => {
    throw toRejection(error)
  })
  promise.catch(() => {})
  return promise
}

const isIPv6 = (address: string) => address.includes(':')

/**
 * Sort addresses IPv4 first and expose them by family
 */
const withAddressFamilies = (service: Omit<Service, 'ipv4' | 'ipv6'>): Service => {
  const addresses = Array.isArray(service.addresses) ? service.addresses : []
  const ipv4 = addresses.filter(address => !isIPv6(address))
  const ipv6 = addresses.filter(isIPv6)
  return { ...service, addresses: [...ipv4, ...ipv6], ipv4, ipv6 }
}

// Browsing this type lists the service types on the network instead of services
const SERVICE_TYPES = { type: 'services._dns-sd', protocol: 'udp' }

// "_http._tcp" -> { type: 'http', protocol: 'tcp' }
const parseServiceType = (name: string): ServiceType | null => {
  const match = /^_([^.]+)\._(tcp|udp)$/.exec(name)
  return match ? { type: match[1]!, protocol: match[2] as ServiceType['protocol'] } : null
}

/**
 * TXT records as ordered [key, value] pairs, from an object or an array of pairs
 */
const toTxtPairs = (txt: TxtRecord | undefined): Array<[string, string]> => {
  const entries = Array.isArray(txt) ? txt : Object.entries(txt || {})
  return entries.map(([key, value]) => [String(key), String(value)])
}

const nativeModule = (): NativeZeroconf => {
  if (!RNZeroconf) {
    throw new Error(
      'react-native-zeroconf: native module not found. Make sure the library is linked and the app rebuilt. Expo Go is not supported, use a development build instead.',
    )
  }
  return RNZeroconf
}

// Typed events on top of the `events` EventEmitter
type Listener<E extends keyof ZeroconfEvents> = ZeroconfEvents[E]

// Typed events, merged into the class below
// eslint-disable-next-line @typescript-eslint/no-unsafe-declaration-merging
interface Zeroconf {
  on<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): this
  once<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): this
  addListener<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): this
  removeListener<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): this
  off<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): this
  removeAllListeners(event?: keyof ZeroconfEvents): this
  listenerCount(event: keyof ZeroconfEvents): number
}

/** Extends the `events` package EventEmitter */
// eslint-disable-next-line @typescript-eslint/no-unsafe-declaration-merging
class Zeroconf extends EventEmitter {
  private _services: Record<string, Service> = {}
  private _serviceTypes: Record<string, ServiceType> = {}
  private _scanningTypes = false
  private _publishedServices: Record<string, PublishedService> = {}
  private _publishedImplTypes: Record<string, ImplType> = {}
  private _scanImplType: ImplType | null = null
  private _scanId = `zeroconf-${++instanceCount}`
  private _hasScanned = false
  private _dListeners: Record<string, EmitterSubscription> = {}

  constructor() {
    super()
    nativeModule()
    this.addDeviceListeners()
  }

  /**
   * Add the native event listeners (called automatically in the constructor)
   */
  addDeviceListeners(): void {
    if (Object.keys(this._dListeners).length) {
      this.emit('error', new Error('RNZeroconf listeners already in place.'))
      return
    }

    this._dListeners.start = DeviceEventEmitter.addListener('RNZeroconfStart', payload => {
      if (this._isOwnEvent(payload)) {
        this.emit('start')
      }
    })

    this._dListeners.stop = DeviceEventEmitter.addListener('RNZeroconfStop', payload => {
      if (this._isOwnEvent(payload)) {
        this.emit('stop')
      }
    })

    this._dListeners.error = DeviceEventEmitter.addListener('RNZeroconfError', err => {
      if (this._isOwnEvent(err) && this.listenerCount('error') > 0) {
        this.emit('error', toError(err))
      }
    })

    this._dListeners.found = DeviceEventEmitter.addListener(
      'RNZeroconfFound',
      (service?: NativePayload) => {
        if (!service || !service.name || !this._isOwnEvent(service)) {
          return
        }
        const { name } = service

        if (this._scanningTypes) {
          const serviceType = parseServiceType(name)
          if (serviceType && !this._serviceTypes[name]) {
            this._serviceTypes[name] = serviceType
            this.emit('typeFound', serviceType)
            this.emit('update')
          }
          return
        }

        // Found services only have a name until they are resolved
        this._services[name] = { name } as Service
        this.emit('found', name)
        this.emit('update')
      },
    )

    this._dListeners.remove = DeviceEventEmitter.addListener(
      'RNZeroconfRemove',
      (service?: NativePayload) => {
        if (!service || !service.name || !this._isOwnEvent(service)) {
          return
        }
        const { name } = service

        if (this._scanningTypes) {
          const serviceType = this._serviceTypes[name]
          if (serviceType) {
            delete this._serviceTypes[name]
            this.emit('typeRemove', serviceType)
            this.emit('update')
          }
          return
        }

        delete this._services[name]

        this.emit('remove', name)
        this.emit('update')
      },
    )

    this._dListeners.resolved = DeviceEventEmitter.addListener(
      'RNZeroconfResolved',
      (data?: NativePayload & Omit<Service, 'ipv4' | 'ipv6'>) => {
        if (!data || !data.name || !this._isOwnEvent(data)) {
          return
        }

        const resolved = { ...data }
        delete resolved.scanId
        const service = withAddressFamilies(resolved)
        this._services[service.name] = service
        this.emit('resolved', service)
        this.emit('update')
      },
    )

    this._dListeners.published = DeviceEventEmitter.addListener(
      'RNZeroconfServiceRegistered',
      (service?: PublishedService) => {
        if (!service || !service.name) {
          return
        }

        this._publishedServices[service.name] = service
        this.emit('published', service)
      },
    )

    this._dListeners.unpublished = DeviceEventEmitter.addListener(
      'RNZeroconfServiceUnregistered',
      (service?: PublishedService) => {
        if (!service || !service.name) {
          return
        }

        delete this._publishedServices[service.name]
        this.emit('unpublished', service)
      },
    )
  }

  /**
   * Add a listener and get a function that removes it, handy in useEffect cleanups
   */
  subscribe<E extends keyof ZeroconfEvents>(event: E, listener: Listener<E>): () => void {
    this.on(event, listener)
    return () => this.removeListener(event, listener)
  }

  /**
   * Scan events belong to the instance that started the scan. Instances that never scanned still receive
   * every scan's events, as before multiple scans, and events without a scan id (publishing) go to everyone.
   */
  private _isOwnEvent(payload: unknown): boolean {
    if (!payload || typeof payload !== 'object' || !this._hasScanned) {
      return true
    }
    const { scanId } = payload as NativePayload
    return scanId == null || scanId === this._scanId
  }

  /**
   * Remove the native event listeners, call it when you are done with the instance
   */
  removeDeviceListeners(): void {
    Object.values(this._dListeners).forEach(listener => listener.remove())
    this._dListeners = {}
  }

  /**
   * All the services found so far, keyed by name
   */
  getServices(): Record<string, Service> {
    return this._services
  }

  /**
   * Service types found by `scanServiceTypes`
   */
  getServiceTypes(): ServiceType[] {
    return Object.values(this._serviceTypes)
  }

  /**
   * Scan for the service types advertised on the network (_http._tcp, _ipp._tcp...), emits typeFound and typeRemove.
   * On iOS, only types declared in NSBonjourServices can then be scanned.
   * On iOS it requires the multicast entitlement. On Android with NSD and on Windows, services other apps publish on the same device are left out.
   */
  scanServiceTypes({ domain, implType, networkInterface }: ServiceTypesScanOptions = {}): void {
    this._startScan({ ...SERVICE_TYPES, domain, implType, networkInterface }, true)
  }

  /**
   * Scan for services, defaults to `_http._tcp.` on the `local.` domain
   */
  scan(options?: ScanOptions): void
  /** @deprecated Use `scan({ type, protocol, domain, implType })` */
  scan(type?: string, protocol?: string, domain?: string, implType?: ImplType): void
  scan(
    options?: ScanOptions | string,
    protocolArg?: string,
    domainArg?: string,
    implTypeArg?: ImplType,
  ): void {
    this._startScan(
      isOptionsObject(options)
        ? options
        : { type: options, protocol: protocolArg, domain: domainArg, implType: implTypeArg },
      false,
    )
  }

  private _startScan(options: ScanOptions, scanningTypes: boolean): void {
    const { type, protocol, domain, implType, resolveTimeout, subtype, networkInterface } =
      withDefaults(SCAN_DEFAULTS, options)
    const native = nativeModule()

    this._services = {}
    this._serviceTypes = {}
    this._scanningTypes = scanningTypes
    this._hasScanned = true
    this.emit('update')
    if (Platform.OS === 'android') {
      if (this._scanImplType && implType !== this._scanImplType) {
        // This instance's scan moves to the other implementation
        native.stop(this._scanId, this._scanImplType)
      }
      this._scanImplType = implType
      native.scan(
        this._scanId,
        type,
        protocol,
        domain,
        implType,
        nativeOptions({ resolveTimeout, subtype, networkInterface }),
      )
    } else {
      native.scan(
        this._scanId,
        type,
        protocol,
        domain,
        nativeOptions({ resolveTimeout, subtype, networkInterface }),
      )
    }
  }

  /**
   * Stop the current scan
   * @param implType Android only, defaults to the implementation used by the last scan
   */
  stop(implType: ImplType = this._scanImplType || ImplType.NSD): void {
    if (Platform.OS === 'android') {
      nativeModule().stop(this._scanId, implType)
    } else {
      nativeModule().stop(this._scanId)
    }
  }

  /**
   * Checks the Local Network permission, resolves 'granted', 'denied' or 'unknown'.
   * iOS: uses a service type from NSBonjourServices (the first one by default) and can show the permission prompt.
   * Android: apps targeting Android 17 (API 37) need ACCESS_LOCAL_NETWORK on Android 17 devices, it is requested
   * when missing unless request is false. 'granted' when there is nothing to grant.
   */
  async checkLocalNetworkAccess({
    type,
    protocol = 'tcp',
    timeout = 5,
    request = true,
  }: LocalNetworkAccessOptions = {}): Promise<LocalNetworkAccess> {
    const native = nativeModule()
    if (Platform.OS === 'android') {
      if (!native.checkLocalNetworkAccess) {
        return 'unknown'
      }
      const status = await asPromise(native.checkLocalNetworkAccess())
      if (status !== 'denied' || !request) {
        return status
      }
      const result = await PermissionsAndroid.request(
        ACCESS_LOCAL_NETWORK as Parameters<typeof PermissionsAndroid.request>[0],
      )
      return result === PermissionsAndroid.RESULTS.GRANTED ? 'granted' : 'denied'
    }
    const serviceType = type ? `_${type}._${protocol}` : null
    return asPromise(native.checkLocalNetworkAccess!(serviceType, timeout))
  }

  /**
   * Publish a service. Resolves with the published service once it is advertised, its name can differ
   * from the requested one when that name is already taken. Rejects with a `ZeroconfError`.
   */
  publishService(options: PublishOptions): Promise<PublishedService>
  /** @deprecated Use `publishService({ type, protocol, domain, name, port, txt, implType })` */
  publishService(
    type: string,
    protocol: string,
    domain: string | undefined,
    name: string,
    port: number,
    txt?: TxtRecord,
    implType?: ImplType,
  ): Promise<PublishedService>
  publishService(
    options: PublishOptions | string,
    protocolArg?: string,
    domainArg?: string,
    nameArg?: string,
    portArg?: number,
    txtArg?: TxtRecord,
    implTypeArg?: ImplType,
  ): Promise<PublishedService> {
    const { type, protocol, domain, name, port, txt, implType, subtypes, networkInterface } =
      withDefaults(
        PUBLISH_DEFAULTS,
        isOptionsObject(options)
          ? options
          : ({
              type: options,
              protocol: protocolArg,
              domain: domainArg,
              name: nameArg,
              port: portArg,
              txt: txtArg,
              implType: implTypeArg,
            } as PublishOptions),
      )
    const native = nativeModule()
    const txtRecord = toTxtPairs(txt)
    const options_ = nativeOptions({ subtypes, networkInterface })
    if (Platform.OS === 'android') {
      this._publishedImplTypes[name] = implType
      return asPromise(
        native.registerService(type, protocol, domain, name, port, txtRecord, implType, options_),
      )
    }
    return asPromise(
      native.registerService(type, protocol, domain, name, port, txtRecord, options_),
    )
  }

  /**
   * Replace the TXT record of a published service, resolves with the updated service.
   * Android `NSD` has no update: the service is published again under the same name.
   */
  updateService(
    name: string,
    { txt = {}, implType = this._publishedImplTypes[name] || ImplType.NSD }: UpdateOptions = {},
  ): Promise<PublishedService> {
    const txtRecord = toTxtPairs(txt)
    if (Platform.OS === 'android') {
      return asPromise(nativeModule().updateService(name, txtRecord, implType))
    }
    return asPromise(nativeModule().updateService(name, txtRecord))
  }

  /**
   * Resolve one service by name without scanning, for a device found before.
   * Resolves with the service, rejects with code 'TIMEOUT' when it doesn't answer in time.
   */
  resolveService(options: ResolveOptions): Promise<Service> {
    const { name, type, protocol, domain, implType, timeout, networkInterface } = withDefaults(
      { ...SCAN_DEFAULTS, timeout: 5 },
      options,
    )
    const native = nativeModule()
    const nativeOptions_ = nativeOptions({ timeout, networkInterface })
    const promise =
      Platform.OS === 'android'
        ? native.resolveService(name, type, protocol, domain, implType, nativeOptions_)
        : native.resolveService(name, type, protocol, domain, nativeOptions_)
    return asPromise(
      asPromise(promise).then(service => (service && withAddressFamilies(service)) as Service),
    )
  }

  /**
   * Unpublish a service. Resolves once it is no longer advertised, rejects with a `ZeroconfError`
   * (e.g. code `'NOT_PUBLISHED'`).
   * @param implType Android only, defaults to the implementation the service was published with
   */
  unpublishService(
    name: string,
    implType: ImplType = this._publishedImplTypes[name] || ImplType.NSD,
  ): Promise<PublishedService | null> {
    if (Platform.OS === 'android') {
      delete this._publishedImplTypes[name]
      return asPromise(nativeModule().unregisterService(name, implType))
    }
    return asPromise(nativeModule().unregisterService(name))
  }
}

export default Zeroconf

interface ScanState<T> {
  items: T[]
  isScanning: boolean
  error: ZeroconfError | null
  stop(): void
  restart(): void
}

// Runs a scan while mounted and enabled, and again when deps change or on restart
function useScan<T>(
  enabled: boolean,
  startScan: (zeroconf: Zeroconf) => void,
  read: (zeroconf: Zeroconf) => T[],
  deps: unknown[],
): ScanState<T> {
  const zeroconfRef = useRef<Zeroconf | null>(null)
  const [items, setItems] = useState<T[]>([])
  const [isScanning, setIsScanning] = useState(false)
  const [error, setError] = useState<ZeroconfError | null>(null)
  const [scanCount, setScanCount] = useState(0)
  const readRef = useRef(read)
  readRef.current = read

  useEffect(() => {
    const zeroconf = new Zeroconf()
    zeroconfRef.current = zeroconf
    const unsubscribes = [
      zeroconf.subscribe('start', () => setIsScanning(true)),
      zeroconf.subscribe('stop', () => setIsScanning(false)),
      zeroconf.subscribe('update', () => setItems(readRef.current(zeroconf))),
      zeroconf.subscribe('error', setError),
    ]
    return () => {
      unsubscribes.forEach(unsubscribe => unsubscribe())
      zeroconf.stop()
      zeroconf.removeDeviceListeners()
      zeroconfRef.current = null
    }
  }, [])

  useEffect(() => {
    const zeroconf = zeroconfRef.current
    if (!zeroconf || !enabled) {
      return undefined
    }
    setItems([])
    setError(null)
    startScan(zeroconf)
    return () => zeroconf.stop()
    // The options are the dependencies, not the callbacks built from them
  }, [enabled, scanCount, ...deps])

  const stop = useCallback(() => {
    if (zeroconfRef.current) {
      zeroconfRef.current.stop()
    }
  }, [])
  const restart = useCallback(() => setScanCount(count => count + 1), [])

  return { items, isScanning, error, stop, restart }
}

/**
 * Scans while mounted and returns the resolved services. Scans again when the options change,
 * stops and cleans up on unmount. Each hook runs its own scan, several can run at once.
 *
 * const { services, isScanning, error, stop, restart } = useZeroconf({ type: 'http' })
 */
export function useZeroconf(options: UseZeroconfOptions = {}): UseZeroconfResult {
  const {
    type,
    protocol,
    domain,
    implType,
    resolveTimeout,
    subtype,
    networkInterface,
    enabled = true,
  } = options
  const { items, ...scan } = useScan(
    enabled,
    zeroconf =>
      zeroconf.scan({
        type,
        protocol,
        domain,
        implType,
        resolveTimeout,
        subtype,
        networkInterface,
      }),
    // Found services only have a name until they are resolved
    zeroconf => Object.values(zeroconf.getServices()).filter(service => service.addresses),
    [type, protocol, domain, implType, resolveTimeout, subtype, networkInterface],
  )
  return { services: items, ...scan }
}

/**
 * Lists the service types advertised on the network while mounted.
 *
 * const { serviceTypes, isScanning, error, stop, restart } = useServiceTypes()
 */
export function useServiceTypes(options: UseServiceTypesOptions = {}): UseServiceTypesResult {
  const { domain, implType, networkInterface, enabled = true } = options
  const { items, ...scan } = useScan(
    enabled,
    zeroconf => zeroconf.scanServiceTypes({ domain, implType, networkInterface }),
    zeroconf => zeroconf.getServiceTypes(),
    [domain, implType, networkInterface],
  )
  return { serviceTypes: items, ...scan }
}
