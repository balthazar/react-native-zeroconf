const mdns = require('mdns')

const ad = mdns.createAdvertisement(mdns.tcp('http'), 4321)
ad.start()

var browser = mdns.createBrowser(mdns.tcp('http'))

browser.on('serviceUp', service => {
  console.log('service up: ', service)
})
browser.on('serviceDown', service => {
  console.log('service down: ', service)
})

browser.start()
