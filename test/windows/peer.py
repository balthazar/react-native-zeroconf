"""A second mDNS stack for the Windows tests (python-zeroconf).

- zc-peer._zcpeer._tcp, answered for service type queries too
- zc-peer-sub, registered only under the _zcsub subtype of _zcpeer._tcp
- When the trigger file exists, zc-peer's TXT record changes to v=2 (live updates)
- zc-ghost._zcghost._tcp is announced but never answers its SRV and TXT queries (resolve timeouts)
"""
import os
import socket
import tempfile
import time

from zeroconf import DNSOutgoing, DNSPointer, ServiceInfo, Zeroconf
from zeroconf.const import _CLASS_IN, _FLAGS_AA, _FLAGS_QR_RESPONSE, _TYPE_PTR

TRIGGER = os.path.join(tempfile.gettempdir(), "rnzeroconf-peer-update")


def service(type_, name, port, properties):
    return ServiceInfo(
        type_,
        name,
        addresses=[socket.inet_aton("127.0.0.1")],
        port=port,
        properties=properties,
        server="zc-peer-host.local.",
    )


zeroconf = Zeroconf()
peer = service("_zcpeer._tcp.local.", "zc-peer._zcpeer._tcp.local.", 45710, {"from": "python"})
zeroconf.register_service(peer)
zeroconf.register_service(service("_zcsub._sub._zcpeer._tcp.local.", "zc-peer-sub._zcpeer._tcp.local.", 45711, {"from": "python-sub"}))
print("peer published zc-peer and zc-peer-sub", flush=True)


def announce_ghost():
    out = DNSOutgoing(_FLAGS_QR_RESPONSE | _FLAGS_AA)
    out.add_answer_at_time(DNSPointer("_zcghost._tcp.local.", _TYPE_PTR, _CLASS_IN, 120, "zc-ghost._zcghost._tcp.local."), 0)
    zeroconf.send(out)


deadline = time.time() + 900
while time.time() < deadline:
    if os.path.exists(TRIGGER):
        os.remove(TRIGGER)
        zeroconf.update_service(service("_zcpeer._tcp.local.", "zc-peer._zcpeer._tcp.local.", 45710, {"from": "python", "v": "2"}))
        print("peer updated zc-peer TXT", flush=True)
    announce_ghost()
    time.sleep(0.5)
