#!/usr/bin/python3

import time
import socket
from zeroconf import ServiceInfo, Zeroconf

import uuid

# Service details
SERVICE_TYPE = "_meshcop._udp.local."
SERVICE_NAME = f"OpenThread Border Router.{uuid.uuid4().hex}._meshcop._udp.local."
SERVER_NAME = "otbr.local."
IP_ADDRESS = "192.168.1.2"
IP_ADDRESS_V6 = "fe80::1"
PORT = 49152

# TXT records
PROPERTIES = {
    b"rv": b"1",
    b"vn": b"OpenThread",
    b"mn": b"BorderRouter",
    b"nn": b"OTBR",
    b"xp": b"12345678",
    b"tv": b"1.1.1",
}

if __name__ == "__main__":
    zeroconf = Zeroconf()
    info = ServiceInfo(
        SERVICE_TYPE,
        SERVICE_NAME,
        addresses=[socket.inet_aton(IP_ADDRESS), socket.inet_pton(socket.AF_INET6, IP_ADDRESS_V6)],
        port=PORT,
        properties=PROPERTIES,
        server=SERVER_NAME,
    )
    zeroconf.register_service(info)
    try:
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        zeroconf.unregister_service(info)
        zeroconf.close()
