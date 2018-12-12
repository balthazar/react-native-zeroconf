import { NativeModules, DeviceEventEmitter } from 'react-native'
import { EventEmitter } from 'events'

const RNZeroconf = NativeModules.RNZeroconf

export default class Zeroconf extends EventEmitter {

  constructor (props) {
    super(props)

    this._services = {}
    this._resolvedServices = {}
    this._dListeners = {}

    this.addDeviceListeners()
  }

  /**
   * Add all event listeners
   */
  addDeviceListeners () {

    if (Object.keys(this._dListeners).length) {
      return this.emit('error', 'RNZeroconf listeners already in place.')
    }

    this._dListeners.start = DeviceEventEmitter.addListener('RNZeroconfStart', () => this.emit('start'))
    this._dListeners.stop = DeviceEventEmitter.addListener('RNZeroconfStop', () => this.emit('stop'))
    this._dListeners.error = DeviceEventEmitter.addListener('RNZeroconfError', (err) => this.emit('errorEvent', err)
      )

    this._dListeners.found = DeviceEventEmitter.addListener('RNZeroconfFound', service => {
      if (!service || !service.name) { return }
      const { name } = service

      this._services[name] = service
      this.emit('found', service)
      this.emit('update', service)
    })

    this._dListeners.remove = DeviceEventEmitter.addListener('RNZeroconfRemove', service => {
      if (!service || !service.name) { return }
      const { name } = service

      delete this._services[name]
      delete this._resolvedServices[name]

      this.emit('remove', service)
      this.emit('update', service)
    })

    this._dListeners.resolved = DeviceEventEmitter.addListener('RNZeroconfResolved', service => {
      if (!service || !service.name) { return }

      this._resolvedServices[service.name] = service
      this._services[service.name] = service
      this.emit('resolved', service)
      this.emit('update', service)
    })

  }

  /**
   * Remove all event listeners and clean map
   */
  removeDeviceListeners () {
    Object.keys(this._dListeners).forEach(name => this._dListeners[name].remove())
    this._dListeners = {}
  }

  /**
   * Get all the services fully resolved or not
   */
  getServices () {
    return this._services
  }

  /**
   * Get all the services fully resolved
   */
  getResolvedServices () {
    return this._resolvedServices
  }

  /**
   * Scan for Zeroconf services,
   * Defaults to _http._tcp. on local domain
   */
  scan (type = 'http', protocol = 'tcp', domain = 'local.') {
    this._services = {}
    this._resolvedServices = {}
    this.emit('start')
    RNZeroconf.scan(type, protocol, domain)
  }

  /**
   * Stop current scan if any
   */
  async stop () {
      await RNZeroconf.stop()
  }

}
