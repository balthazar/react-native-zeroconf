import { useCallback, useEffect, useMemo, useState } from 'react'
import { ActivityIndicator, FlatList, Platform, Pressable, RefreshControl, StyleSheet, Text, View } from 'react-native'
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context'
import { StatusBar } from 'expo-status-bar'
import { MaterialCommunityIcons } from '@expo/vector-icons'
import { ImplType, Service, ServiceType, ZeroconfError, useServiceTypes, useZeroconf } from 'react-native-zeroconf'
import { IOS_TYPES, deviceIcon, typeInfo, typeKey } from './serviceTypes'

// DNSSD gives host names on every Android version, used to group services by device
const implType = ImplType.DNSSD

interface FoundService extends Service {
  serviceType: ServiceType
}

interface Device {
  host: string
  name: string
  address?: string
  services: FoundService[]
}

export default function App() {
  return (
    <SafeAreaProvider>
      <StatusBar style="dark" />
      <Browser />
    </SafeAreaProvider>
  )
}

function Browser() {
  // Android lists the service types on the network, iOS scans a known list
  const listed = useServiceTypes({ enabled: Platform.OS === 'android' })
  const types = Platform.OS === 'android' ? listed.serviceTypes : IOS_TYPES

  const [servicesByType, setServicesByType] = useState<Record<string, FoundService[]>>({})
  const [errors, setErrors] = useState<Record<string, ZeroconfError>>({})
  const [round, setRound] = useState(0)

  const onServices = useCallback((key: string, services: FoundService[]) => {
    setServicesByType(previous => ({ ...previous, [key]: services }))
  }, [])
  const onError = useCallback((key: string, error: ZeroconfError | null) => {
    setErrors(({ [key]: _, ...others }) => (error ? { ...others, [key]: error } : others))
  }, [])

  const refresh = () => {
    setServicesByType({})
    setErrors({})
    setRound(r => r + 1)
    listed.restart()
  }

  const devices = useMemo(() => groupByDevice(Object.values(servicesByType).flat()), [servicesByType])
  const error = listed.error ?? Object.values(errors)[0]

  return (
    <SafeAreaView style={styles.screen} edges={['top']}>
      {/* One scan per service type, they render nothing */}
      {types.map(serviceType => (
        <TypeScanner key={`${round}${typeKey(serviceType)}`} serviceType={serviceType} onServices={onServices} onError={onError} />
      ))}

      <View style={styles.header}>
        <Text style={styles.title}>Network</Text>
        <Text style={styles.subtitle}>
          {devices.length} {devices.length === 1 ? 'device' : 'devices'} · {types.length}{' '}
          {types.length === 1 ? 'service type' : 'service types'}
        </Text>
      </View>
      {error ? (
        <View style={styles.error}>
          <MaterialCommunityIcons name="alert-circle-outline" size={18} color="#b42318" />
          <Text style={styles.errorText}>{error.message}</Text>
        </View>
      ) : null}

      <FlatList
        data={devices}
        keyExtractor={device => device.host}
        renderItem={({ item }) => <DeviceRow device={item} />}
        contentContainerStyle={styles.list}
        refreshControl={<RefreshControl refreshing={false} onRefresh={refresh} />}
        ListEmptyComponent={
          <View style={styles.empty}>
            <ActivityIndicator />
            <Text style={styles.emptyText}>Looking for devices…</Text>
          </View>
        }
      />
    </SafeAreaView>
  )
}

function TypeScanner({
  serviceType,
  onServices,
  onError,
}: {
  serviceType: ServiceType
  onServices: (key: string, services: FoundService[]) => void
  onError: (key: string, error: ZeroconfError | null) => void
}) {
  const key = typeKey(serviceType)
  const { services, error } = useZeroconf({ ...serviceType, implType })

  useEffect(() => onServices(key, services.map(service => ({ ...service, serviceType }))), [key, services, serviceType, onServices])
  useEffect(() => onError(key, error), [key, error, onError])
  // Forget this type's services when its scan goes away
  useEffect(() => () => onServices(key, []), [key, onServices])

  return null
}

function DeviceRow({ device }: { device: Device }) {
  const [open, setOpen] = useState(false)
  const icon = deviceIcon(device.services.map(service => service.serviceType.type))

  return (
    <Pressable style={styles.card} onPress={() => setOpen(o => !o)}>
      <View style={styles.row}>
        <View style={styles.icon}>
          <MaterialCommunityIcons name={icon} size={24} color="#1d4ed8" />
        </View>
        <View style={styles.grow}>
          <Text style={styles.name} numberOfLines={1}>
            {device.name}
          </Text>
          <Text style={styles.detail} numberOfLines={1}>
            {[device.address, `${device.services.length} ${device.services.length === 1 ? 'service' : 'services'}`]
              .filter(Boolean)
              .join(' · ')}
          </Text>
        </View>
        <MaterialCommunityIcons name={open ? 'chevron-down' : 'chevron-right'} size={22} color="#9ca3af" />
      </View>

      {open ? (
        <View style={styles.services}>
          <Text style={styles.host}>{device.host}</Text>
          {device.services.map(service => {
            const info = typeInfo(service.serviceType)
            return (
              <View key={`${typeKey(service.serviceType)}${service.name}`} style={styles.service}>
                <MaterialCommunityIcons name={info.icon} size={18} color="#4b5563" />
                <View style={styles.grow}>
                  <Text style={styles.serviceName}>
                    {info.label} <Text style={styles.detail}>:{service.port}</Text>
                  </Text>
                  <Text style={styles.detail} numberOfLines={1}>
                    {service.name}
                  </Text>
                  {Object.keys(service.txt).length ? (
                    <Text style={styles.txt} numberOfLines={3}>
                      {Object.entries(service.txt)
                        .map(([k, v]) => `${k}=${v}`)
                        .join('  ')}
                    </Text>
                  ) : null}
                </View>
              </View>
            )
          })}
        </View>
      ) : null}
    </Pressable>
  )
}

// Services advertised by the same host are one device
function groupByDevice(services: FoundService[]): Device[] {
  const byHost = new Map<string, FoundService[]>()
  for (const service of services) {
    const host = service.host || service.addresses[0] || service.name
    byHost.set(host, [...(byHost.get(host) ?? []), service])
  }
  return [...byHost]
    .map(([host, hostServices]) => ({
      host,
      name: deviceName(host, hostServices),
      address: displayAddress(hostServices),
      services: hostServices.sort((a, b) => typeInfo(a.serviceType).label.localeCompare(typeInfo(b.serviceType).label)),
    }))
    .sort((a, b) => a.name.localeCompare(b.name))
}

// The first address reachable from other devices: IPv4 first, no loopback or link-local addresses
function displayAddress(services: FoundService[]): string | undefined {
  const addresses = services.flatMap(service => service.addresses)
  return addresses.find(address => !/^(127\.|169\.254\.|::1$|fe80:)/i.test(address)) ?? addresses[0]
}

// Friendly names come from the services: Chromecasts have one in their TXT record, AirPlay audio names are "MAC@Name"
function deviceName(host: string, services: FoundService[]): string {
  for (const service of services) {
    const { type } = service.serviceType
    if (type === 'googlecast' && service.txt.fn) return service.txt.fn
    if (type === 'airplay' || type === 'companion-link') return service.name
    if (type === 'raop' && service.name.includes('@')) return service.name.split('@').slice(1).join('@')
  }
  return host.replace(/\.local\.?$/, '')
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: '#f6f7f9' },
  header: { paddingHorizontal: 20, paddingTop: 12, paddingBottom: 8 },
  title: { fontSize: 30, fontWeight: '700', color: '#111827' },
  subtitle: { fontSize: 14, color: '#6b7280', marginTop: 2 },
  error: {
    flexDirection: 'row',
    gap: 8,
    marginHorizontal: 16,
    marginBottom: 4,
    padding: 10,
    borderRadius: 10,
    backgroundColor: '#fef3f2',
  },
  errorText: { flex: 1, fontSize: 13, color: '#b42318' },
  list: { padding: 16, gap: 10, flexGrow: 1 },
  empty: { flex: 1, alignItems: 'center', justifyContent: 'center', gap: 10, paddingTop: 80 },
  emptyText: { fontSize: 15, color: '#6b7280' },
  card: { backgroundColor: '#ffffff', borderRadius: 14, padding: 14, borderWidth: 1, borderColor: '#e5e7eb' },
  row: { flexDirection: 'row', alignItems: 'center', gap: 12 },
  icon: { width: 42, height: 42, borderRadius: 21, backgroundColor: '#eff4ff', alignItems: 'center', justifyContent: 'center' },
  grow: { flex: 1 },
  name: { fontSize: 16, fontWeight: '600', color: '#111827' },
  detail: { fontSize: 13, color: '#6b7280' },
  services: { marginTop: 12, paddingTop: 12, borderTopWidth: 1, borderTopColor: '#f0f1f3', gap: 12 },
  host: { fontSize: 12, color: '#9ca3af' },
  service: { flexDirection: 'row', gap: 10 },
  serviceName: { fontSize: 14, fontWeight: '500', color: '#1f2937' },
  txt: { fontSize: 11, color: '#9ca3af', marginTop: 2, fontFamily: Platform.select({ ios: 'Menlo', default: 'monospace' }) },
})
