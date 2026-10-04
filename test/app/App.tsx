// Self-test app for CI (Windows, the iOS simulator): runs the library through JavaScript against the
// python-zeroconf peer (test/app/peer.py), then publishes _zcresult._tcp with the result in its TXT record,
// which test/app/check-app.py waits for.
import React, { useEffect, useState } from 'react'
import { Platform, ScrollView, Text, TurboModuleRegistry } from 'react-native'
import Zeroconf, { Service, ZeroconfError } from 'react-native-zeroconf'

const withTimeout = <T,>(promise: Promise<T>, seconds: number, what: string) =>
  Promise.race([
    promise,
    new Promise<T>((_, reject) => setTimeout(() => reject(new Error(`${what} timed out`)), seconds * 1000)),
  ])

export default function App() {
  const [lines, setLines] = useState<string[]>([])

  useEffect(() => {
    const zeroconf = new Zeroconf()
    const out: string[] = []
    const failed: string[] = []
    const log = (ok: boolean, step: string, detail: string) => {
      out.push(`${ok ? 'ok  ' : 'FAIL'} ${step}: ${detail}`)
      if (!ok) failed.push(step)
      setLines([...out])
    }
    zeroconf.on('error', (e: ZeroconfError) => out.push(`error event [${e.domain} ${e.code}] ${e.message}`))

    // Collects a scan's resolved names and errors for a while
    const scanFor = (options: object, seconds: number) =>
      new Promise<{ names: string[]; errors: ZeroconfError[] }>(resolve => {
        const scanner = new Zeroconf()
        const names: string[] = []
        const errors: ZeroconfError[] = []
        scanner.on('resolved', service => names.push(service.name))
        scanner.on('error', error => errors.push(error))
        scanner.scan(options)
        setTimeout(() => {
          scanner.stop()
          scanner.removeDeviceListeners()
          resolve({ names, errors })
        }, seconds * 1000)
      })

    const run = async () => {
      // Every platform runs the C++ module
      log(TurboModuleRegistry.get('Zeroconf') != null, 'C++ module', `registered on ${Platform.isTV ? 'tvos' : Platform.OS}`)

      // Scan: found and resolved, through events
      try {
        const peer = await withTimeout(
          new Promise<Service>(resolve => {
            zeroconf.on('resolved', service => service.name === 'zc-peer' && resolve(service))
            zeroconf.scan({ type: 'zcpeer' })
          }),
          30,
          'scan',
        )
        log(peer.port === 45710 && peer.txt.from === 'python', 'scan', `${peer.host} ${peer.port} ${JSON.stringify(peer.txt)}`)
      } catch (e) {
        log(false, 'scan', String(e))
      }
      zeroconf.stop()

      // resolveService
      try {
        const peer = await zeroconf.resolveService({ name: 'zc-peer', type: 'zcpeer', timeout: 10 })
        log(peer.port === 45710 && peer.addresses.length > 0, 'resolveService', `${peer.port} ${peer.addresses.join(',')}`)
      } catch (e) {
        log(false, 'resolveService', `${(e as ZeroconfError).code} ${(e as Error).message}`)
      }

      // Subtypes: zc-peer-sub is only registered under the _zcsub subtype
      const subtype = await scanFor({ type: 'zcpeer', subtype: 'zcsub' }, 8)
      log(subtype.names.length > 0 && subtype.names.every(name => name === 'zc-peer-sub'), 'subtype scan', subtype.names.join(','))

      // resolveTimeout: zc-ghost is announced but never answers, it is reported with TIMEOUT after one retry
      const ghost = await scanFor({ type: 'zcghost', resolveTimeout: 1 }, 8)
      const timeout = ghost.errors.find(error => error.code === 'TIMEOUT' && error.serviceName === 'zc-ghost')
      log(timeout !== undefined && ghost.names.length === 0, 'resolveTimeout', ghost.errors.map(error => `${error.code} ${error.serviceName}`).join(','))

      // Publish, update, unpublish, and a structured rejection
      try {
        const published = await withTimeout(zeroconf.publishService({ type: 'zcapp', protocol: 'tcp', name: 'zc-app', port: 45720, txt: { a: '1' } }), 15, 'publish')
        log(published.name === 'zc-app', 'publishService', published.name)
        const updated = await withTimeout(zeroconf.updateService('zc-app', { txt: { a: '2' } }), 15, 'update')
        log(updated.txt.a === '2', 'updateService', JSON.stringify(updated.txt))
        const gone = await withTimeout(zeroconf.unpublishService('zc-app'), 15, 'unpublish')
        log(gone?.name === 'zc-app', 'unpublishService', String(gone?.name))
      } catch (e) {
        log(false, 'publishing', `${(e as ZeroconfError).code} ${(e as Error).message}`)
      }
      try {
        await zeroconf.unpublishService('never-published')
        log(false, 'NOT_PUBLISHED', 'resolved')
      } catch (e) {
        const error = e as ZeroconfError
        log(error.code === 'NOT_PUBLISHED' && error.domain === 'RNZeroconf', 'NOT_PUBLISHED', `${error.domain} ${error.code}`)
      }

      // Report through mDNS: check-app.py browses for this service
      const result = failed.length ? 'fail' : 'pass'
      out.push(`result: ${result}`)
      setLines([...out])
      await zeroconf.publishService({
        type: 'zcresult',
        protocol: 'tcp',
        name: 'zc-result',
        port: 45730,
        txt: { result, failed: failed.join(',') || '-', log: out.join(' | ').slice(0, 240) },
      })
    }
    run()
  }, [])

  return (
    <ScrollView contentContainerStyle={{ padding: 20 }}>
      {lines.map((line, i) => (
        <Text key={i}>{line}</Text>
      ))}
    </ScrollView>
  )
}
