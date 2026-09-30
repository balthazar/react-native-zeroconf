import { useCallback, useEffect, useRef, useState } from 'react'
import { Platform, NativeModules, DeviceEventEmitter } from 'react-native'
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

      this._services[name] = { name }
      this.emit('found', name)
      this.emit('update')
    })

    this._dListeners.remove = DeviceEventEmitter.addListener('RNZeroconfRemove', service => {
      if (!service || !service.name || !this._isOwnEvent(service)) {
        return
      }
      const { name } = service

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
   * Scan for Zeroconf services, defaults to _http._tcp. on the local. domain
   *
   * scan({ type, protocol, domain, implType, resolveTimeout })
   * scan(type, protocol, domain, implType) is deprecated
   */
  scan(options, protocolArg, domainArg, implTypeArg) {
    const { type, protocol, domain, implType, resolveTimeout } = withDefaults(
      SCAN_DEFAULTS,
      isOptionsObject(options)
        ? options
        : { type: options, protocol: protocolArg, domain: domainArg, implType: implTypeArg },
    )

    this._services = {}
    this._hasScanned = true
    this.emit('update')
    if (Platform.OS === 'android') {
      if (this._scanImplType && implType !== this._scanImplType) {
        // This instance's scan moves to the other implementation
        RNZeroconf.stop(this._scanId, this._scanImplType)
      }
      this._scanImplType = implType
      RNZeroconf.scan(this._scanId, type, protocol, domain, implType)
    } else {
      RNZeroconf.scan(this._scanId, type, protocol, domain, resolveTimeout)
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
   * Checks the iOS Local Network permission, resolves 'granted', 'denied' or 'unknown'.
   * Uses a service type from NSBonjourServices (the first one by default) and can show the permission prompt.
   * Android has no equivalent permission to check and resolves 'unknown'.
   */
  checkLocalNetworkAccess({ type, protocol = 'tcp', timeout = 5 } = {}) {
    if (Platform.OS === 'android') {
      return Promise.resolve('unknown')
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
    const { type, protocol, domain, name, port, txt, implType } = withDefaults(
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
    if (Platform.OS === 'android') {
      this._publishedImplTypes[name] = implType
      return asPromise(
        RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord, implType),
      )
    }
    return asPromise(RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord))
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

/**
 * Scans while mounted and returns the resolved services.
 * Scans again when the options change, stops and cleans up on unmount.
 * Each hook runs its own scan, several can run at once.
 *
 * const { services, isScanning, error, stop, restart } = useZeroconf({ type: 'http' })
 */
export function useZeroconf(options = {}) {
  const { type, protocol, domain, implType, resolveTimeout, enabled = true } = options
  const zeroconfRef = useRef(null)
  const [services, setServices] = useState([])
  const [isScanning, setIsScanning] = useState(false)
  const [error, setError] = useState(null)
  const [scanCount, setScanCount] = useState(0)

  useEffect(() => {
    const zeroconf = new Zeroconf()
    zeroconfRef.current = zeroconf
    // Found services only have a name until they are resolved
    const updateServices = () =>
      setServices(Object.values(zeroconf.getServices()).filter(service => service.addresses))
    const unsubscribes = [
      zeroconf.subscribe('start', () => setIsScanning(true)),
      zeroconf.subscribe('stop', () => setIsScanning(false)),
      zeroconf.subscribe('update', updateServices),
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
    setServices([])
    setError(null)
    zeroconf.scan({ type, protocol, domain, implType, resolveTimeout })
    return () => zeroconf.stop()
  }, [enabled, type, protocol, domain, implType, resolveTimeout, scanCount])

  const stop = useCallback(() => {
    if (zeroconfRef.current) {
      zeroconfRef.current.stop()
    }
  }, [])
  const restart = useCallback(() => setScanCount(count => count + 1), [])

  return { services, isScanning, error, stop, restart }
}
