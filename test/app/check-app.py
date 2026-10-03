"""Waits for the result service published by test/windows/App.tsx and exits 0 if the self-test passed."""
import sys
import time

from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

TIMEOUT = 300
result = {}


class Listener(ServiceListener):
    def add_service(self, zc, type_, name):
        info = zc.get_service_info(type_, name, timeout=3000)
        if info:
            result.update({k.decode(): (v or b"").decode() for k, v in info.properties.items()})

    def update_service(self, zc, type_, name):
        self.add_service(zc, type_, name)

    def remove_service(self, zc, type_, name):
        pass


zeroconf = Zeroconf()
ServiceBrowser(zeroconf, "_zcresult._tcp.local.", Listener())
deadline = time.time() + TIMEOUT
while not result and time.time() < deadline:
    time.sleep(1)
zeroconf.close()

if not result:
    print(f"No result from the app after {TIMEOUT}s")
    sys.exit(1)
print("App log:", result.get("log"))
print("Result:", result.get("result"), "failed:", result.get("failed"))
sys.exit(0 if result.get("result") == "pass" else 1)
