'use strict';

import React, { NativeModules, DeviceEventEmitter } from 'react-native';
import { EventEmitter } from 'events';

const RNZeroconf = NativeModules.RNZeroconf;

export default class Zeroconf extends EventEmitter {

  constructor (props) {
    super(props);

    this._services = [];

    DeviceEventEmitter.addListener('RNZeroconfStart', () => {
      this.emit('start');
    });

    DeviceEventEmitter.addListener('RNZeroconfStop', () => {
      this.emit('stop');
    });

    DeviceEventEmitter.addListener('RNZeroconfError', err => {
      this.emit('error', err);
    });

    DeviceEventEmitter.addListener('RNZeroconfFound', name => {
      if (!name) { return; }
      this._services.push(name)
      this.emit('found', name);
      this.emit('update');
    });

    DeviceEventEmitter.addListener('RNZeroconfRemove', name => {
      const index = this._services.indexOf(name);
      if (index !== -1) { this._services.splice(index, 1); }
      this.emit('remove', name);
      this.emit('update');
    });

  }

  /**
   * Get all the services already resolved
   */
  getServices () {
    return this._services;
  }

  /**
   * Scan for Zeroconf services,
   * Defaults to _http._tcp. on local domain
   */
  scan (type = 'http', protocol = 'tcp', domain = 'local.') {
    this._services = [];
    this.emit('update');
    RNZeroconf.scan(type, protocol, domain);
  }

  /**
   * Stop current scan if any
   */
  stop () {
    RNZeroconf.stop();
  }

}
