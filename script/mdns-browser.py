#!/usr/bin/python3
# mDNS Service Scanner
#
# This script scans the local network for multicast DNS (mDNS) services.
# It displays detailed information about discovered and updated services,
# including their IP addresses (both IPv4 and IPv6), port numbers, and TXT records.
#
# Features:
# - Discovers services on the local network.
# - Displays distinct icons for newly discovered (✅) and updated (🔄) services.
# - Resolves and lists all associated IPv4 and IPv6 addresses.
# - Parses and displays all TXT records for each service.
# - Handles graceful shutdown with Ctrl+C.
#
# Requirements:
# - Python 3
# - zeroconf
#
# Installation:
# 1. Clone the repository or download the script.
# 2. Install the required Python packages:
#    pip install -r requirements.txt
#
# Usage:
# Run the scanner from your terminal. You can optionally provide a service name
# as an argument. If no service name is provided, it defaults to `_meshcop._udp.local.`:
#
# python3 scanner.py [service_name]
#
# Examples:
#
# Scan for the default `_meshcop._udp.local.` service:
# python3 scanner.py
#
# Scan for a custom service, e.g., `_http._tcp.local.`:
# python3 scanner.py _http._tcp.local.
#
# The script will continuously listen for services. Press Ctrl+C to stop the scan.

import sys
import queue
import time
import argparse
import threading
from typing import Optional
from zeroconf import ServiceBrowser, Zeroconf, IPVersion, ServiceInfo

# Constants
EVENT_DISCOVERED = "discovered"
EVENT_UPDATED = "updated"
SERVICE_RESOLVE_TIMEOUT_MS = 5000


def print_service_info(zeroconf: Zeroconf, type: str, name: str,
                       event: str) -> None:
    icon = "✅" if event == EVENT_DISCOVERED else "🔄"
    action = "Discovered" if event == EVENT_DISCOVERED else "Updated"
    print(f"\n{icon} Service {action}: {name}")
    info: Optional[ServiceInfo] = zeroconf.get_service_info(
        type, name, timeout=SERVICE_RESOLVE_TIMEOUT_MS)
    if not info:
        print("   Could not resolve details.")
        return

    print(
        f"   Host: {getattr(info, 'server', 'N/A')}:{getattr(info, 'port', 'N/A')}"
    )
    print(f"   Host TTL: {getattr(info, 'host_ttl', 'N/A')}")
    print(f"   Other TTL: {getattr(info, 'other_ttl', 'N/A')}")
    print(f"   Priority: {getattr(info, 'priority', 'N/A')}")
    print(f"   Weight: {getattr(info, 'weight', 'N/A')}")

    for ip_address in info.parsed_addresses():
        print(f"   IP Address: {ip_address}")

    if info.properties:
        print("   --- TXT Records ---")
        for key, value in info.properties.items():
            key_str = key.decode("utf-8")
            if value is None:
                val_str = ""
            else:
                try:
                    val_str = value.decode("utf-8")
                except UnicodeDecodeError:
                    val_str = f"0x{value.hex()}"
            print(f"   {key_str} = {val_str}")
    else:
        print("   No TXT records found.")


class ServiceListener:

    def __init__(self, q: queue.Queue) -> None:
        self.q = q

    def remove_service(self, zeroconf: Zeroconf, type: str, name: str) -> None:
        print(f"Service {name} removed")

    def update_service(self, zeroconf: Zeroconf, type: str, name: str) -> None:
        self.q.put((EVENT_UPDATED, type, name))

    def add_service(self, zeroconf: Zeroconf, type: str, name: str) -> None:
        self.q.put((EVENT_DISCOVERED, type, name))


def worker(q: queue.Queue, zeroconf_instance: Zeroconf) -> None:
    while True:
        try:
            event, type, name = q.get()
            if event is None:
                break
            print_service_info(zeroconf_instance, type, name, event)
            q.task_done()
        except Exception as e:
            print(f"Error in worker thread: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="mDNS Service Scanner")
    parser.add_argument(
        'service_name',
        nargs='?',
        default="_meshcop._udp.local.",
        help="The mDNS service to scan for (default: %(default)s)")
    service_name = parser.parse_args().service_name

    q: queue.Queue = queue.Queue()
    zeroconf = Zeroconf(ip_version=IPVersion.All)
    listener = ServiceListener(q)
    browser = ServiceBrowser(zeroconf, service_name, listener)

    resolver_thread = threading.Thread(target=worker,
                                       args=(q, zeroconf))
    resolver_thread.start()

    print(f"🔍 Starting mDNS scan for {service_name} services...")
    print("Press Ctrl+C to exit.")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
        q.put((None, None, None))
        resolver_thread.join()
    finally:
        zeroconf.close()
