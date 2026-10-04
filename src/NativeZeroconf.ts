// The native module's interface: codegen generates its C++ spec (cpp/ZeroconfModule implements it).
// JavaScript only reaches it through src/index.ts.
import { TurboModuleRegistry, type CodegenTypes, type TurboModule } from 'react-native'

export type TxtEntry = {
  key: string
  value: string
}

export type NativeService = {
  name: string
  fullName: string
  host: string
  port: number
  addresses: string[]
  txt: TxtEntry[]
}

// code is a string for domain 'RNZeroconf' ('TIMEOUT'), the platform's number as a string otherwise ('-65555')
export type NativeError = {
  message: string
  code: string
  domain: string
  serviceName: string
}

// A promise settles with one or the other: rejections would lose code and domain on some platforms
export type NativeResult = {
  service?: NativeService
  error?: NativeError
}

export type NativeAccessResult = {
  status?: string
  error?: NativeError
}

export type ScanEvent = {
  scanId: string
}

export type NameEvent = {
  scanId: string
  name: string
}

export type ResolvedEvent = {
  scanId: string
  service: NativeService
}

// scanId is empty for errors that are not about a scan
export type ErrorEvent = {
  scanId: string
  error: NativeError
}

export type NativeScanOptions = {
  resolveTimeout?: number
  subtype?: string
  networkInterface?: string
}

export type NativePublishOptions = {
  subtypes?: string[]
  networkInterface?: string
}

export type NativeResolveOptions = {
  timeout?: number
  networkInterface?: string
}

export interface Spec extends TurboModule {
  // implType picks the Android implementation ('NSD' or 'DNSSD'), the other platforms ignore it
  scan(
    scanId: string,
    type: string,
    protocol: string,
    domain: string,
    implType: string,
    options: NativeScanOptions,
  ): void
  // An empty scanId stops every scan
  stop(scanId: string, implType: string): void
  registerService(
    type: string,
    protocol: string,
    domain: string,
    name: string,
    port: number,
    txt: TxtEntry[],
    implType: string,
    options: NativePublishOptions,
  ): Promise<NativeResult>
  updateService(name: string, txt: TxtEntry[], implType: string): Promise<NativeResult>
  unregisterService(name: string, implType: string): Promise<NativeResult>
  resolveService(
    name: string,
    type: string,
    protocol: string,
    domain: string,
    implType: string,
    options: NativeResolveOptions,
  ): Promise<NativeResult>
  // An empty type uses the first one in NSBonjourServices (iOS)
  checkLocalNetworkAccess(type: string, timeout: number): Promise<NativeAccessResult>

  readonly onStart: CodegenTypes.EventEmitter<ScanEvent>
  readonly onStop: CodegenTypes.EventEmitter<ScanEvent>
  readonly onFound: CodegenTypes.EventEmitter<NameEvent>
  readonly onRemove: CodegenTypes.EventEmitter<NameEvent>
  readonly onResolved: CodegenTypes.EventEmitter<ResolvedEvent>
  readonly onError: CodegenTypes.EventEmitter<ErrorEvent>
  readonly onPublished: CodegenTypes.EventEmitter<NativeService>
  readonly onUnpublished: CodegenTypes.EventEmitter<NativeService>
}

// Null when the module isn't in the app: Expo Go, or an app not rebuilt after installing the library
export default TurboModuleRegistry.get<Spec>('Zeroconf')
