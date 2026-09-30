# Android Emulator

The Android emulator does not support IGMP or multicast by default ([documented limitation](https://developer.android.com/studio/run/emulator-networking)). mDNS relies on multicast UDP to `224.0.0.251:5353`, so **discovery does not work on a stock emulator**.

> **Recommended:** test on a physical device connected to the same network as the services.

## Advanced: TAP bridged networking (Linux, Ethernet)

Only if you really need the emulator. Read the caveats first:

- Use a **Google APIs** system image, not **Google Play**: Play images don't allow `adb root`, which is needed to configure `eth1`.
- Always start with **`-no-snapshot-load`**, otherwise the saved state ignores the QEMU network device and `eth1` does not exist.
- The configuration **does not persist**: redo step 3 after every emulator restart.
- **One emulator per TAP interface** (create `tap1`, `tap2`... for more).
- Use addresses from **your** subnet in place of the examples.
- Wi-Fi bridging does not work on most systems. Use Ethernet.

### 1. Create the TAP interface and bridge (once)

```bash
sudo ip tuntap add dev tap0 mode tap user $USER
sudo ip link add name br0 type bridge
sudo ip link show                                  # find your Ethernet interface
sudo ip link set <your-ethernet-interface> master br0   # e.g. enp3s0
sudo ip link set tap0 master br0
sudo ip link set dev tap0 up
sudo ip link set dev br0 up
sudo dhcpcd br0                                    # or: sudo dhclient br0
```

### 2. Start the emulator on the TAP interface

```bash
emulator -avd <avd_name> \
  -no-snapshot-load \
  -qemu \
  -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no \
  -device virtio-net-pci,netdev=mynet0
```

### 3. Configure `eth1` in the emulator

```bash
adb root
adb shell ip link set eth1 up
adb shell ip addr add <unused_ip> dev eth1              # e.g. 192.168.1.213/24
adb shell ip route add default via <gateway> dev eth1   # e.g. 192.168.1.1
adb shell ping -c 2 <device_ip>                          # check connectivity
```

### 4. Point Metro at the bridge address

```bash
ip addr show br0 | grep "inet "      # e.g. inet 192.168.1.28/24
REACT_NATIVE_PACKAGER_HOSTNAME=192.168.1.28 npx expo start --android
```
