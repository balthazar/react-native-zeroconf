import { Platform, NativeModules, DeviceEventEmitter } from 'react-native'
import { EventEmitter } from 'events'

const RNZeroconf = NativeModules.RNZeroconf

export const ImplType = {
  NSD: 'NSD',
  DNSSD: 'DNSSD',
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

    this._dListeners.start = DeviceEventEmitter.addListener('RNZeroconfStart', () =>
      this.emit('start'),
    )

    this._dListeners.stop = DeviceEventEmitter.addListener('RNZeroconfStop', () =>
      this.emit('stop'),
    )

    this._dListeners.error = DeviceEventEmitter.addListener('RNZeroconfError', err => {
      if (this.listenerCount('error') > 0) {
        this.emit('error', new Error(err))
      }
    })

    this._dListeners.found = DeviceEventEmitter.addListener('RNZeroconfFound', service => {
      if (!service || !service.name) {
        return
      }
      const { name } = service

      this._services[name] = service
      this.emit('found', name)
      this.emit('update')
    })

    this._dListeners.remove = DeviceEventEmitter.addListener('RNZeroconfRemove', service => {
      if (!service || !service.name) {
        return
      }
      const { name } = service

      delete this._services[name]

      this.emit('remove', name)
      this.emit('update')
    })

    this._dListeners.resolved = DeviceEventEmitter.addListener('RNZeroconfResolved', data => {
      if (!data || !data.name) {
        return
      }

      const service = withAddressFamilies(data)
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
   * Scan for Zeroconf services,
   * Defaults to _http._tcp. on local domain
   */
  scan(type = 'http', protocol = 'tcp', domain = 'local.', implType = ImplType.NSD) {
    this._services = {}
    this.emit('update')
    if (Platform.OS === 'android') {
      if (this._scanImplType && implType !== this._scanImplType) {
        // Only one scan runs at a time, stop the one running on the other implementation
        RNZeroconf.stop(this._scanImplType)
      }
      this._scanImplType = implType
      RNZeroconf.scan(type, protocol, domain, implType)
    } else {
      RNZeroconf.scan(type, protocol, domain)
    }
  }

  /**
   * Stop current scan if any,
   * Defaults to the implementation used by the last scan
   */
  stop(implType = this._scanImplType || ImplType.NSD) {
    if (Platform.OS === 'android') {
      RNZeroconf.stop(implType)
    } else {
      RNZeroconf.stop()
    }
  }

  /**
   * Publish a service
   */
  publishService(type, protocol, domain = 'local.', name, port, txt = {}, implType = ImplType.NSD) {
    const txtRecord = toTxtPairs(txt)
    if (Platform.OS === 'android') {
      this._publishedImplTypes[name] = implType
      RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord, implType)
    } else {
      RNZeroconf.registerService(type, protocol, domain, name, port, txtRecord)
    }
  }

  /**
   * Unpublish a service,
   * Defaults to the implementation the service was published with
   */
  unpublishService(name, implType = this._publishedImplTypes[name] || ImplType.NSD) {
    if (Platform.OS === 'android') {
      delete this._publishedImplTypes[name]
      RNZeroconf.unregisterService(name, implType)
    } else {
      RNZeroconf.unregisterService(name)
    }
  }
}
