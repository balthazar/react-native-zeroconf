import type { ComponentProps } from 'react'
import type { MaterialCommunityIcons } from '@expo/vector-icons'
import type { ServiceType } from 'react-native-zeroconf'

type IconName = ComponentProps<typeof MaterialCommunityIcons>['name']

interface TypeInfo {
  label: string
  icon: IconName
}

// Known service types, in the order used to pick a device's icon (a printer that also serves HTTP is a printer)
const KNOWN: Record<string, TypeInfo> = {
  ipp: { label: 'Printer (IPP)', icon: 'printer' },
  ipps: { label: 'Printer (IPPS)', icon: 'printer' },
  printer: { label: 'Printer (LPD)', icon: 'printer' },
  'pdl-datastream': { label: 'Printer (raw)', icon: 'printer' },
  uscan: { label: 'Scanner', icon: 'scanner' },
  googlecast: { label: 'Chromecast', icon: 'cast' },
  airplay: { label: 'AirPlay', icon: 'cast' },
  raop: { label: 'AirPlay audio', icon: 'speaker-wireless' },
  'spotify-connect': { label: 'Spotify Connect', icon: 'spotify' },
  hap: { label: 'HomeKit accessory', icon: 'home-automation' },
  matter: { label: 'Matter', icon: 'home-automation' },
  matterc: { label: 'Matter (commissioning)', icon: 'home-automation' },
  'companion-link': { label: 'Apple device', icon: 'apple' },
  workstation: { label: 'Workstation', icon: 'desktop-classic' },
  smb: { label: 'File sharing (SMB)', icon: 'folder-network' },
  afpovertcp: { label: 'File sharing (AFP)', icon: 'folder-network' },
  ssh: { label: 'SSH', icon: 'console' },
  'sftp-ssh': { label: 'SFTP', icon: 'console' },
  alexa: { label: 'Alexa', icon: 'speaker' },
  mqtt: { label: 'MQTT broker', icon: 'access-point-network' },
  https: { label: 'Web server (HTTPS)', icon: 'lock' },
  http: { label: 'Web server', icon: 'web' },
  'sleep-proxy': { label: 'Sleep proxy', icon: 'sleep' },
  'device-info': { label: 'Device info', icon: 'information-outline' },
}
const PRIORITY = Object.keys(KNOWN)

export const typeKey = ({ type, protocol }: ServiceType) => `_${type}._${protocol}`

export const typeInfo = (serviceType: ServiceType): TypeInfo =>
  KNOWN[serviceType.type] ?? { label: typeKey(serviceType), icon: 'lan' }

// The icon of the most telling type a device advertises
export const deviceIcon = (types: string[]): IconName => {
  const known = types.filter(type => type in KNOWN).sort((a, b) => PRIORITY.indexOf(a) - PRIORITY.indexOf(b))
  return known.length ? KNOWN[known[0]].icon : 'devices'
}

// iOS can't list the types on the network, it scans these instead. They are declared in app.json (NSBonjourServices)
export const IOS_TYPES: ServiceType[] = [
  'http',
  'https',
  'ipp',
  'ipps',
  'printer',
  'pdl-datastream',
  'airplay',
  'raop',
  'googlecast',
  'spotify-connect',
  'companion-link',
  'hap',
  'ssh',
  'smb',
].map(type => ({ type, protocol: 'tcp' }))
