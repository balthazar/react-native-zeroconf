import React, { Component } from 'react'
import {
  Platform,
  StyleSheet,
  TouchableOpacity,
  Text,
  SafeAreaView,
  FlatList,
  RefreshControl,
} from 'react-native'
import { List, ListItem } from 'react-native-elements'
import Zeroconf from 'react-native-zeroconf'

const zeroconf = new Zeroconf()

export default class App extends Component {
  state = {
    isScanning: false,
    selectedService: null,
    services: {},
  }

  componentDidMount() {
    this.refreshData()

    zeroconf.on('start', () => {
      this.setState({ isScanning: true })
      console.log('[Start]')
    })

    zeroconf.on('stop', () => {
      this.setState({ isScanning: false })
      console.log('[Stop]')
    })

    zeroconf.on('resolved', service => {
      console.log('[Resolve]', JSON.stringify(service, null, 2))

      this.setState({
        services: {
          ...this.state.services,
          [service.host]: service,
        },
      })
    })

    zeroconf.on('error', err => {
      this.setState({ isScanning: false })
      console.log('[Error]', err)
    })
  }

  renderRow = ({ item, index }) => {
    const { name, fullName, host } = this.state.services[item]

    return (
      <TouchableOpacity
        onPress={() =>
          this.setState({
            selectedService: host,
          })}
      >
        <ListItem title={name} subtitle={fullName} />
      </TouchableOpacity>
    )
  }

  refreshData = () => {
    const { isScanning } = this.state
    if (isScanning) {
      return
    }

    this.setState({ services: [] })

    zeroconf.scan('http', 'tcp', 'local.')

    clearTimeout(this.timeout)
    this.timeout = setTimeout(() => {
      zeroconf.stop()
    }, 5000)
  }

  render() {
    const { services, selectedService, isScanning } = this.state
    console.log(selectedService)

    const service = services[selectedService]

    if (service) {
      return (
        <SafeAreaView style={styles.container}>
          <TouchableOpacity onPress={() => this.setState({ selectedService: null })}>
            <Text style={styles.closeButton}>{'CLOSE'}</Text>
          </TouchableOpacity>

          <Text style={styles.json}>{JSON.stringify(services, null, 2)}</Text>
        </SafeAreaView>
      )
    }

    return (
      <SafeAreaView style={styles.container}>
        <Text style={styles.state}>{isScanning ? 'Scanning..' : 'Stopped'}</Text>

        <FlatList
          data={Object.keys(services)}
          renderItem={this.renderRow}
          keyExtractor={key => key}
          refreshControl={
            <RefreshControl
              refreshing={isScanning}
              onRefresh={this.refreshData}
              tintColor="skyblue"
            />
          }
        />
      </SafeAreaView>
    )
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  closeButton: {
    padding: 20,
    textAlign: 'center',
  },
  json: {
    padding: 10,
  },
  state: {
    fontSize: 20,
    textAlign: 'center',
    margin: 30,
  },
})
