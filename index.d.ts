export declare const ImplType: {
  readonly NSD: 'NSD'
  readonly DNSSD: 'DNSSD'
}

/** Android only: which discovery implementation to use */
export type ImplType = (typeof ImplType)[keyof typeof ImplType]

export interface Service {
  /** Human-readable service name */
  name: string
  /** Full service name, e.g. `XeroxPrinter.local._http._tcp.` */
  fullName: string
  /** Hostname, e.g. `XeroxPrinter.local.` */
  host: string
  port: number
  /** IPv4 and IPv6 addresses, IPv4 first */
  addresses: string[]
  ipv4: string[]
  ipv6: string[]
  /** TXT record attributes */
  txt: Record<string, string>
}

type TxtValue = string | number | boolean

/** TXT records as an object, or as [key, value] pairs to control the order */
export type TxtRecord = Record<string, TxtValue> | Array<[string, TxtValue]>

export interface ScanOptions {
  /** Service type without underscore, e.g. `'http'`. Defaults to `'http'` */
  type?: string
  /** `'tcp'` or `'udp'`. Defaults to `'tcp'` */
  protocol?: string
  /** Defaults to `'local.'` */
  domain?: string
  /** Android only, defaults to `NSD` */
  implType?: ImplType
  /** iOS only, seconds to try resolving a service before retrying once and giving up. Defaults to `5` */
  resolveTimeout?: number
  /** Only find services registered with this subtype, e.g. `'printer'` */
  subtype?: string
  /**
   * Network interface to scan on, e.g. `'en0'` on iOS or `'wlan0'` on Android. All of them by default.
   * Android `NSD` needs Android 13 or later for it
   */
  networkInterface?: string
}

export interface PublishOptions {
  /** Service type without underscore, e.g. `'http'` */
  type: string
  /** `'tcp'` or `'udp'` */
  protocol: string
  /** Defaults to `'local.'` */
  domain?: string
  /** Service name, should be unique on the network */
  name: string
  port: number
  txt?: TxtRecord
  /** Android only, defaults to `NSD` */
  implType?: ImplType
  /** Subtypes to register the service with, e.g. `['printer']` */
  subtypes?: string[]
  /** Network interface to publish on, all of them by default. Android `NSD` needs Android 13 or later for it */
  networkInterface?: string
}

export interface UpdateOptions {
  /** The new TXT record, replaces the current one */
  txt?: TxtRecord
  /** Android only, defaults to the implementation the service was published with */
  implType?: ImplType
}

export interface ResolveOptions {
  /** Name of the service */
  name: string
  /** Service type without underscore, e.g. `'http'`. Defaults to `'http'` */
  type?: string
  /** Defaults to `'tcp'` */
  protocol?: string
  /** Defaults to `'local.'` */
  domain?: string
  /** Android only, defaults to `NSD` */
  implType?: ImplType
  /** Seconds before rejecting with code `'TIMEOUT'`, defaults to `5` */
  timeout?: number
  /** Network interface to resolve on, all of them by default */
  networkInterface?: string
}

/**
 * Errors emitted by the `error` event
 *
 * - `DNSSD`: DNSServiceErrorType, on iOS and from Android's embedded mDNSResponder,
 *   e.g. `-65555` type missing from NSBonjourServices, `-65570` Local Network access denied
 * - `NsdManager`: Android NsdManager failure code
 * - `RNZeroconf`: the library's own errors, e.g. `'EXCEPTION'`, `'TIMEOUT'`, `'UNKNOWN_INTERFACE'`
 */
export interface ZeroconfError extends Error {
  code: number | string
  domain: 'DNSSD' | 'NsdManager' | 'RNZeroconf'
  /** The service the error is about, when there is one */
  serviceName?: string
}

export interface LocalNetworkAccessOptions {
  /** Service type without underscore, must be listed in `NSBonjourServices`. Defaults to the first one listed */
  type?: string
  /** Defaults to `'tcp'` */
  protocol?: string
  /** iOS: seconds to wait for an answer, defaults to `5` */
  timeout?: number
  /** Android: request `ACCESS_LOCAL_NETWORK` when it is missing, defaults to `true` */
  request?: boolean
}

/** Published services don't carry the address family fields */
export type PublishedService = Omit<Service, 'ipv4' | 'ipv6'>

export interface ZeroconfEvents {
  start: () => void
  stop: () => void
  /** Service found, before resolution */
  found: (name: string) => void
  /** Service fully resolved with network info */
  resolved: (service: Service) => void
  /** Service removed from the network */
  remove: (name: string) => void
  /** Services or service types list changed */
  update: () => void
  /** Service type found by `scanServiceTypes` */
  typeFound: (serviceType: ServiceType) => void
  /** Service type no longer advertised, during `scanServiceTypes` */
  typeRemove: (serviceType: ServiceType) => void
  error: (error: ZeroconfError) => void
  published: (service: PublishedService) => void
  unpublished: (service: PublishedService) => void
}

/** A service type advertised on the network, e.g. `{ type: 'http', protocol: 'tcp' }` for `_http._tcp` */
export interface ServiceType {
  type: string
  protocol: 'tcp' | 'udp'
}

export interface ServiceTypesScanOptions {
  /** Defaults to `local.` */
  domain?: string
  /** Android only, defaults to `ImplType.NSD`. With `NSD`, the list leaves out services published by the phone running the app, `DNSSD` includes them */
  implType?: ImplType
  /** Network interface to list the types on, all of them by default */
  networkInterface?: string
}

/** Extends the `events` package EventEmitter */
export default class Zeroconf {
  constructor()

  on<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): this
  once<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): this
  addListener<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): this
  removeListener<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): this
  off<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): this
  removeAllListeners(event?: keyof ZeroconfEvents): this
  /** Adds a listener and returns a function that removes it */
  subscribe<E extends keyof ZeroconfEvents>(event: E, listener: ZeroconfEvents[E]): () => void
  listenerCount(event: keyof ZeroconfEvents): number

  /** Add the native event listeners (called automatically in the constructor) */
  addDeviceListeners(): void

  /** Remove the native event listeners, call it when you are done with the instance */
  removeDeviceListeners(): void

  /** All the services found so far, keyed by name */
  getServices(): Record<string, Service>

  /** Service types found by `scanServiceTypes` */
  getServiceTypes(): ServiceType[]

  /**
   * Scan for the service types advertised on the network, emits `typeFound` and `typeRemove`.
   * On iOS it requires the `com.apple.developer.networking.multicast` entitlement.
   */
  scanServiceTypes(options?: ServiceTypesScanOptions): void

  /** Scan for services, defaults to `_http._tcp.` on the `local.` domain */
  scan(options?: ScanOptions): void
  /** @deprecated Use `scan({ type, protocol, domain, implType })` */
  scan(type?: string, protocol?: string, domain?: string, implType?: ImplType): void

  /**
   * Checks the Local Network permission. iOS can show the permission prompt when it hasn't been answered yet.
   * Android 17 (API 37): apps targeting API 37 need `ACCESS_LOCAL_NETWORK`, requested when missing unless
   * `request` is `false`. Resolves `'granted'` when there is nothing to grant.
   */
  checkLocalNetworkAccess(
    options?: LocalNetworkAccessOptions,
  ): Promise<'granted' | 'denied' | 'unknown'>

  /**
   * Stop the current scan
   * @param implType Android only, defaults to the implementation used by the last scan
   */
  stop(implType?: ImplType): void

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

  /**
   * Unpublish a service. Resolves once it is no longer advertised, rejects with a `ZeroconfError`
   * (e.g. code `'NOT_PUBLISHED'`).
   * @param implType Android only, defaults to the implementation the service was published with
   */
  unpublishService(name: string, implType?: ImplType): Promise<PublishedService | null>

  /**
   * Replace the TXT record of a published service, resolves with the updated service.
   * Android `NSD` has no update: the service is published again under the same name.
   */
  updateService(name: string, options: UpdateOptions): Promise<PublishedService>

  /** Resolve one service by name without scanning. Rejects with code `'TIMEOUT'` when it doesn't answer in time */
  resolveService(options: ResolveOptions): Promise<Service>
}

export interface UseZeroconfOptions extends ScanOptions {
  /** Scan only while true, defaults to `true` */
  enabled?: boolean
}

export interface UseZeroconfResult {
  /** Resolved services, updated as they are found, resolved and removed */
  services: Service[]
  isScanning: boolean
  /** Last error, reset when a new scan starts */
  error: ZeroconfError | null
  stop(): void
  /** Clears the services and scans again */
  restart(): void
}

/**
 * Scans while mounted and returns the resolved services. Scans again when the options change,
 * stops and cleans up on unmount. Each hook runs its own scan, several can run at once.
 */
export function useZeroconf(options?: UseZeroconfOptions): UseZeroconfResult

export interface UseServiceTypesOptions extends ServiceTypesScanOptions {
  /** Scan only while true, defaults to `true` */
  enabled?: boolean
}

export interface UseServiceTypesResult {
  /** Service types advertised on the network, updated as they appear and disappear */
  serviceTypes: ServiceType[]
  isScanning: boolean
  /** Last error, reset when a new scan starts */
  error: ZeroconfError | null
  stop(): void
  /** Clears the service types and scans again */
  restart(): void
}

/** Lists the service types advertised on the network while mounted */
export function useServiceTypes(options?: UseServiceTypesOptions): UseServiceTypesResult
