import {NativeModules, DeviceEventEmitter} from 'react-native'
import {EventEmitter} from 'events'

const RNZeroconf = NativeModules.RNZeroconf

export default class Zeroconf extends EventEmitter {

    constructor(props) {
        super(props)
        this._services = {}
        this._deviceListeners = {}
        this._onStart = this._onStart.bind(this)
        this._onStop = this._onStop.bind(this)
        this._onError = this._onError.bind(this)
        this._onFound = this._onFound.bind(this)
        this._onResolved = this._onResolved.bind(this)
        this._onRemove = this._onRemove.bind(this)
    }

    /**
     * Remove all listeners
     */
    removeDeviceListener(event) {
        this._deviceListeners[event].remove()
    }

    removeAllDeviceListeners() {
        Object.keys(this._deviceListeners).map(e => this._deviceListeners[e].remove())
    }

    /**
     * Scan for Zeroconf services,
     * Defaults to _http._tcp. on local domain
     */
    scan(type = 'http', protocol = 'tcp', domain = 'local.') {
        this._deviceListeners.start = DeviceEventEmitter.addListener('RNZeroconfStart', this._onStart)
        this._deviceListeners.stop = DeviceEventEmitter.addListener('RNZeroconfStop', this._onStop)
        this._deviceListeners.remove = DeviceEventEmitter.addListener('RNZeroconfStop', this._onRemove)
        this._deviceListeners.found = DeviceEventEmitter.addListener('RNZeroconfFound', this._onFound)
        this._deviceListeners.resolve = DeviceEventEmitter.addListener('RNZeroconfResolved', this._onResolved)
        this._deviceListeners.error = DeviceEventEmitter.addListener('RNZeroconfError', this._onError)

        this._services = {}
        this.emit('update')
        RNZeroconf.scan(type, protocol, domain)
    }

    /**
     * Stop current scan if any
     */
    stop() {
        RNZeroconf.stop()
        this.removeAllListeners()
    }

    /**
     * Get all the services already resolved
     */
    getServices() {
        return this._services
    }


    /**
     * On start
     * @private
     */
    _onStart() {
        this.emit('start')
    }

    /**
     * On stop
     * @private
     */
    _onStop() {
        this.emit('stop')
    }

    /**
     * On service found
     * @private
     */
    _onFound(service) {
        if (!service || !service.name) {
            return
        }
        const {name} = service

        this._services[name] = service
        this.emit('found', name)
        this.emit('update')
    }

    /**
     * On service resolved
     * @private
     */
    _onResolved(service) {
        if (!service || !service.name) {
            return
        }
        this._services[service.name] = service
        this.emit('resolved', service)
        this.emit('update')
    }

    /**
     * On service removed
     * @private
     */
    _onRemove(service) {
        if (!service || !service.name) {
            return
        }
        const {name} = service

        delete this._services[name]

        this.emit('remove', name)
        this.emit('update')
    }

    /**
     * On error
     * @private
     */
    _onError(err) {
        this.emit('error', err)
    }
}
