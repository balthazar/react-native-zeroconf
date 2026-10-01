"""A second mDNS stack for the Windows harness (python-zeroconf): publishes _zcpeer._tcp and answers
service type queries, so the harness can discover a service it didn't publish itself."""
import socket
import time

from zeroconf import ServiceInfo, Zeroconf

info = ServiceInfo(
    "_zcpeer._tcp.local.",
    "zc-peer._zcpeer._tcp.local.",
    addresses=[socket.inet_aton("127.0.0.1")],
    port=45710,
    properties={"from": "python"},
    server="zc-peer-host.local.",
)
zeroconf = Zeroconf()
zeroconf.register_service(info)
print("peer published zc-peer._zcpeer._tcp.local.", flush=True)
time.sleep(900)
