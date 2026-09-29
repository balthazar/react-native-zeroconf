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
  error: (error: Error) => void
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

  /**
   * Scan for services, defaults to `_http._tcp.` on the `local.` domain
   * @param implType Android only, defaults to `NSD`
   */
  scan(type?: string, protocol?: string, domain?: string, implType?: ImplType): void

  /**
   * Stop the current scan
   * @param implType Android only, defaults to the implementation used by the last scan
   */
  stop(implType?: ImplType): void

  /**
   * Publish a service. Type and protocol are passed without underscores, e.g. `('http', 'tcp', ...)`
   * @param implType Android only, defaults to `NSD`
   */
  publishService(
    type: string,
    protocol: string,
    domain: string | undefined,
    name: string,
    port: number,
    txt?: Record<string, string | number | boolean>,
    implType?: ImplType,
  ): void

  /**
   * Unpublish a service
   * @param implType Android only, defaults to the implementation the service was published with
   */
  unpublishService(name: string, implType?: ImplType): void
}
