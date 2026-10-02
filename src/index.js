import { useCallback, useEffect, useRef, useState } from 'react'
import { Platform, NativeModules, DeviceEventEmitter, PermissionsAndroid } from 'react-native'
import { EventEmitter } from 'events'

const RNZeroconf = NativeModules.RNZeroconf

export const ImplType = {
  NSD: 'NSD',
  DNSSD: 'DNSSD',
}

const SCAN_DEFAULTS = {
  type: 'http',
  protocol: 'tcp',
  domain: 'local.',
  implType: ImplType.NSD,
  resolveTimeout: 5,
}

const PUBLISH_DEFAULTS = {
  domain: 'local.',
  txt: {},
  implType: ImplType.NSD,
}

/**
 * Merge options over defaults, ignoring undefined values
 */
const withDefaults = (defaults, options) => {
  const merged = { ...defaults }
  Object.entries(options).forEach(([key, value]) => {
    if (value !== undefined) {
      merged[key] = value
    }
  })
  return merged
}

const isOptionsObject = value => value !== null && typeof value === 'object'

// Options passed to native code, without the undefined ones
const nativeOptions = options =>
  Object.fromEntries(Object.entries(options).filter(([, value]) => value !== undefined))

// Android 17 (API 37) runtime permission, enforced for apps targeting API 37 or declaring it
const ACCESS_LOCAL_NETWORK = 'android.permission.ACCESS_LOCAL_NETWORK'

// Each instance runs its own scan, identified by this id in native events
let instanceCount = 0

/**
 * Error from a native { message, code, domain, serviceName } payload
 */
const toError = payload => {
  if (!payload || typeof payload !== 'object') {
    return new Error(payload)
  }
  const error = new Error(payload.message)
  error.code = payload.code
  error.domain = payload.domain
  if (payload.serviceName) {
    error.serviceName = payload.serviceName
  }
  return error
}

/**
 * Promise rejections carry the same payload as error events in userInfo
 */
const toRejection = error =>
  error && error.userInfo && error.userInfo.domain ? toError(error.userInfo) : error

/**
 * Normalizes rejections, and marks the promise handled so callers that don't await it
 * don't get unhandled rejection warnings (errors are also emitted as error events)
 */
const asPromise = nativePromise => {
  const promise = Promise.resolve(nativePromise).catch(error => {
    throw toRejection(error)
  })
  promise.catch(() => {})
  return promise
}

const isIPv6 = address => address.includes(':')

/**
 * Sort addresses IPv4 first and expose them by family
 */
const withAddressFamilies = service => {
  const addresses = Array.isArray(service.addresses) ? service.addresses : []
  const ipv4 = addresses.filter(address => !isIPv6(address))
  const ipv6 = addresses.filter(isIPv6)
  return { ...service, addresses: [...ipv4, ...ipv6], ipv4, ipv6 }
}

/**
 * TXT records as ordered [key, value] pairs, from an object or an array of pairs
 */
// Browsing this type lists the service types on the network instead of services
const SERVICE_TYPES = { type: 'services._dns-sd', protocol: 'udp' }

// "_http._tcp" -> { type: 'http', protocol: 'tcp' }
const parseServiceType = name => {
  const match = /^_([^.]+)\._(tcp|udp)$/.exec(name)
  return match ? { type: match[1], protocol: match[2] } : null
}

const toTxtPairs = txt => {
  const entries = Array.isArray(txt) ? txt : Object.entries(txt || {})
  return entries.map(([key, value]) => [String(key), String(value)])
}

export default class Zeroconf extends EventEmitter {
  constructor(props) {
    super(props)

    if (!RNZeroconf) {
      throw new Error(
        'react-native-zeroconf: native module not found. Make sure the library is linked and the app rebuilt. Expo Go is not supported, use a development build instead.',
      )
    }

    this._services = {}
    this._serviceTypes = {}
    this._scanningTypes = false
    this._publishedServices = {}
    this._publishedImplTypes = {}
    this._scanImplType = null
    this._scanId = `zeroconf-${++instanceCount}`
    this._hasScanned = false
    this._dListeners = {}

    this.addDeviceListeners()
  }

  /**
   * Add all event listeners
   */
  addDeviceListeners() {
    if (Object.keys(this._dListeners).length) {
      return this.emit('error', new Error('RNZeroconf listeners already in place.'))
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

    this._dListeners.found = DeviceEventEmitter.addListener('RNZeroconfFound', service => {
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

      this._services[name] = { name }
      this.emit('found', name)
      this.emit('update')
    })

    this._dListeners.remove = DeviceEventEmitter.addListener('RNZeroconfRemove', service => {
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
    })

    this._dListeners.resolved = DeviceEventEmitter.addListener('RNZeroconfResolved', data => {
      if (!data || !data.name || !this._isOwnEvent(data)) {
        return
      }

      const resolved = { ...data }
      delete resolved.scanId
      const service = withAddressFamilies(resolved)
      this._services[service.name] = service
      this.emit('resolved', service)
      this.emit('update')
    })

    this._dListeners.published = DeviceEventEmitter.addListener(
      'RNZeroconfServiceRegistered',
      service => {
        if (!service || !service.name) {
          return
        }

        this._publishedServices[service.name] = service
        this.emit('published', service)
      },
    )

    this._dListeners.unpublished = DeviceEventEmitter.addListener(
      'RNZeroconfServiceUnregistered',
      service => {
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
  subscribe(event, listener) {
    this.on(event, listener)
    return () => this.removeListener(event, listener)
  }

  /**
   * Scan events belong to the instance that started the scan. Instances that never scanned still receive
   * every scan's events, as before multiple scans, and events without a scan id (publishing) go to everyone.
   */
  _isOwnEvent(payload) {
    if (!payload || typeof payload !== 'object' || payload.scanId == null || !this._hasScanned) {
      return true
    }
    return payload.scanId === this._scanId
  }

  /**
   * Remove all event listeners and clean map
   */
  removeDeviceListeners() {
    Object.keys(this._dListeners).forEach(name => this._dListeners[name].remove())
    this._dListeners = {}
  }

  /**
   * Get all the services already resolved
   */
  getServices() {
    return this._services
  }

  /**
   * Get the service types found by scanServiceTypes, as [{ type, protocol }]
   */
  getServiceTypes() {
    return Object.values(this._serviceTypes)
  }

  /**
   * Scan for the service types advertised on the network (_http._tcp, _ipp._tcp...), emits typeFound and typeRemove.
   * On iOS, only types declared in NSBonjourServices can then be scanned.
   * On iOS it requires the multicast entitlement. On Android with NSD and on Windows, services other apps publish on the same device are left out.
   *
   * scanServiceTypes({ domain, implType })
   */
  scanServiceTypes({ domain, implType, networkInterface } = {}) {
    this._startScan({ ...SERVICE_TYPES, domain, implType, networkInterface }, true)
  }

  /**
   * Scan for Zeroconf services, defaults to _http._tcp. on the local. domain
   *
   * scan({ type, protocol, domain, implType, resolveTimeout })
   * scan(type, protocol, domain, implType) is deprecated
   */
  scan(options, protocolArg, domainArg, implTypeArg) {
    this._startScan(
      isOptionsObject(options)
        ? options
        : { type: options, protocol: protocolArg, domain: domainArg, implType: implTypeArg },
      false,
    )
  }

  _startScan(options, scanningTypes) {
    const { type, protocol, domain, implType, resolveTimeout, subtype, networkInterface } =
      withDefaults(SCAN_DEFAULTS, options)

    this._services = {}
    this._serviceTypes = {}
    this._scanningTypes = scanningTypes
    this._hasScanned = true
    this.emit('update')
    if (Platform.OS === 'android') {
      if (this._scanImplType && implType !== this._scanImplType) {
        // This instance's scan moves to the other implementation
        RNZeroconf.stop(this._scanId, this._scanImplType)
      }
      this._scanImplType = implType
      RNZeroconf.scan(
        this._scanId,
        type,
        protocol,
        domain,
        implType,
        nativeOptions({ resolveTimeout, subtype, networkInterface }),
      )
    } else {
      RNZeroconf.scan(
        this._scanId,
        type,
        protocol,
        domain,
        nativeOptions({ resolveTimeout, subtype, networkInterface }),
      )
    }
  }

  /**
   * Stop current scan if any,
   * Defaults to the implementation used by the last scan
   */
  stop(implType = this._scanImplType || ImplType.NSD) {
    if (Platform.OS === 'android') {
      RNZeroconf.stop(this._scanId, implType)
    } else {
      RNZeroconf.stop(this._scanId)
    }
  }

  /**
   * Checks the Local Network permission, resolves 'granted', 'denied' or 'unknown'.
   * iOS: uses a service type from NSBonjourServices (the first one by default) and can show the permission prompt.
   * Android: apps targeting Android 17 (API 37) need ACCESS_LOCAL_NETWORK on Android 17 devices, it is requested
   * when missing unless request is false. 'granted' when there is nothing to grant.
   */
  async checkLocalNetworkAccess({ type, protocol = 'tcp', timeout = 5, request = true } = {}) {
    if (Platform.OS === 'android') {
      if (!RNZeroconf.checkLocalNetworkAccess) {
        return 'unknown'
      }
      const status = await asPromise(RNZeroconf.checkLocalNetworkAccess())
      if (status !== 'denied' || !request) {
        return status
      }
      const result = await PermissionsAndroid.request(ACCESS_LOCAL_NETWORK)
      return result === PermissionsAndroid.RESULTS.GRANTED ? 'granted' : 'denied'
    }
    const serviceType = type ? `_${type}._${protocol}` : null
    return asPromise(RNZeroconf.checkLocalNetworkAccess(serviceType, timeout))
  }

  /**
   * Publish a service, resolves with the published service once it is advertised.
   * Its name can differ from the requested one when that name is already taken.
   *
   * publishService({ type, protocol, domain, name, port, txt, implType })
   * publishService(type, protocol, domain, name, port, txt, implType) is deprecated
   */
  publishService(options, protocolArg, domainArg, nameArg, portArg, txtArg, implTypeArg) {
    const { type, protocol, domain, name, port, txt, implType, subtypes, networkInterface } =
      withDefaults(
        PUBLISH_DEFAULTS,
        isOptionsObject(options)
          ? options
          : {
              type: options,
              protocol: protocolArg,
              domain: domainArg,
              name: nameArg,
              port: portArg,
              txt: txtArg,
              implType: implTypeArg,
            },
      )
    const txtRecord = toTxtPairs(txt)
    const native = nativeOptions({ subtypes, networkInterface })
    if (Platform.OS === 'android') {
      this._publishedImplTypes[name] = implType
      return asPromise(
        RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord, implType, native),
      )
    }
    return asPromise(
      RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord, native),
    )
  }

  /**
   * Replace the TXT record of a published service, resolves with the updated service.
   * Android NSD has no update, the service is published again under the same name.
   *
   * updateService(name, { txt })
   */
  updateService(
    name,
    { txt = {}, implType = this._publishedImplTypes[name] || ImplType.NSD } = {},
  ) {
    const txtRecord = toTxtPairs(txt)
    if (Platform.OS === 'android') {
      return asPromise(RNZeroconf.updateService(name, txtRecord, implType))
    }
    return asPromise(RNZeroconf.updateService(name, txtRecord))
  }

  /**
   * Resolve one service by name without scanning, for a device found before.
   * Resolves with the service, rejects with code 'TIMEOUT' when it doesn't answer in time.
   *
   * resolveService({ name, type, protocol, domain, implType, timeout, networkInterface })
   */
  resolveService(options) {
    const { name, type, protocol, domain, implType, timeout, networkInterface } = withDefaults(
      { ...SCAN_DEFAULTS, timeout: 5 },
      options,
    )
    const native = nativeOptions({ timeout, networkInterface })
    const promise =
      Platform.OS === 'android'
        ? RNZeroconf.resolveService(name, type, protocol, domain, implType, native)
        : RNZeroconf.resolveService(name, type, protocol, domain, native)
    return asPromise(asPromise(promise).then(service => service && withAddressFamilies(service)))
  }

  /**
   * Unpublish a service, resolves once it is no longer advertised.
   * Defaults to the implementation the service was published with
   */
  unpublishService(name, implType = this._publishedImplTypes[name] || ImplType.NSD) {
    if (Platform.OS === 'android') {
      delete this._publishedImplTypes[name]
      return asPromise(RNZeroconf.unregisterService(name, implType))
    }
    return asPromise(RNZeroconf.unregisterService(name))
  }
}

// Runs a scan while mounted and enabled, and again when deps change or on restart
function useScan(enabled, startScan, read, deps) {
  const zeroconfRef = useRef(null)
  const [items, setItems] = useState([])
  const [isScanning, setIsScanning] = useState(false)
  const [error, setError] = useState(null)
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
 * Scans while mounted and returns the resolved services.
 * Scans again when the options change, stops and cleans up on unmount.
 * Each hook runs its own scan, several can run at once.
 *
 * const { services, isScanning, error, stop, restart } = useZeroconf({ type: 'http' })
 */
export function useZeroconf(options = {}) {
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
export function useServiceTypes(options = {}) {
  const { domain, implType, networkInterface, enabled = true } = options
  const { items, ...scan } = useScan(
    enabled,
    zeroconf => zeroconf.scanServiceTypes({ domain, implType, networkInterface }),
    zeroconf => zeroconf.getServiceTypes(),
    [domain, implType, networkInterface],
  )
  return { serviceTypes: items, ...scan }
}
