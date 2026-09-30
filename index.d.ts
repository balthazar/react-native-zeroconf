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
}

/**
 * Errors emitted by the `error` event
 *
 * - `NSNetServices`: iOS NSNetServicesErrorCode, e.g. `-72007` timeout, `-72008` missing Info.plist configuration
 * - `NsdManager`: Android NsdManager failure code
 * - `DNSSD`: Android DNSServiceErrorType from the embedded mDNSResponder
 * - `RNZeroconf`: the library's own errors, e.g. `'EXCEPTION'`
 */
export interface ZeroconfError extends Error {
  code: number | string
  domain: 'NSNetServices' | 'NsdManager' | 'DNSSD' | 'RNZeroconf'
  /** The service the error is about, when there is one */
  serviceName?: string
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
  /** Services list changed */
  update: () => void
  error: (error: ZeroconfError) => void
  published: (service: PublishedService) => void
  unpublished: (service: PublishedService) => void
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
  listenerCount(event: keyof ZeroconfEvents): number

  /** Add the native event listeners (called automatically in the constructor) */
  addDeviceListeners(): void

  /** Remove the native event listeners, call it when you are done with the instance */
  removeDeviceListeners(): void

  /** All the services found so far, keyed by name */
  getServices(): Record<string, Service>

  /** Scan for services, defaults to `_http._tcp.` on the `local.` domain */
  scan(options?: ScanOptions): void
  /** @deprecated Use `scan({ type, protocol, domain, implType })` */
  scan(type?: string, protocol?: string, domain?: string, implType?: ImplType): void

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
}
