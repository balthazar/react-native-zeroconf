/** Android only: which discovery implementation to use */
export const ImplType = {
  NSD: 'NSD',
  DNSSD: 'DNSSD',
} as const

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
  /**
   * Seconds to try resolving each service before retrying once, then emitting an `error` with code `'TIMEOUT'`.
   * Defaults to `5`. Android `NSD` before Android 14 can't retry a resolve, the error comes after twice this time
   */
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
 * - `Windows`: Win32 / DNS_STATUS code from the Windows DNS-SD functions
 * - `RNZeroconf`: the library's own errors, e.g. `'EXCEPTION'`, `'TIMEOUT'`, `'UNKNOWN_INTERFACE'`
 */
export interface ZeroconfError extends Error {
  code: number | string
  domain: 'DNSSD' | 'NsdManager' | 'RNZeroconf' | 'Windows'
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
  /** Android only, defaults to `ImplType.NSD`. With `NSD`, services other apps publish on the same phone are left out (this app's own are included), `DNSSD` includes them */
  implType?: ImplType
  /** Network interface to list the types on, all of them by default */
  networkInterface?: string
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
