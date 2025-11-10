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

import time
import sys
from zeroconf import ServiceBrowser, Zeroconf, ServiceInfo, IPVersion

class ServiceListener:
    def remove_service(self, zeroconf, type, name):
        print(f"Service {name} removed")

    def update_service(self, zeroconf, type, name):
        self._print_service_info(zeroconf, type, name, "updated")

    def add_service(self, zeroconf, type, name):
        self._print_service_info(zeroconf, type, name, "discovered")

    def _print_service_info(self, zeroconf, type, name, event):
        icon = "✅" if event == "discovered" else "🔄"
        action = "Discovered" if event == "discovered" else "Updated"
        print(f"\n{icon} Service {action}: {name}")
        # The info object contains all the resolved details
        info = zeroconf.get_service_info(type, name, timeout=5000)
        if not info:
            print("   Could not resolve details.")
            return

        print(f"   Host: {info.server}:{info.port}")

        # Print all IP addresses
        for ip_address in info.parsed_addresses():
            print(f"   IP Address: {ip_address}")
        
        # Print all TXT records (properties)
        if info.properties:
            print("   --- TXT Records ---")
            for key, value in info.properties.items():
                key_str = key.decode("utf-8")
                if value is None:
                    val_str = ""
                else:
                    try:
                        # Try to decode value as a string
                        val_str = value.decode("utf-8")
                    except UnicodeDecodeError:
                        # If it fails, it's binary data, so print as hex
                        val_str = f"0x{value.hex()}"
                print(f"   {key_str} = {val_str}")
        else:
            print("   No TXT records found.")

# Main script execution
if __name__ == "__main__":
    service_name = "_meshcop._udp.local."
    if len(sys.argv) > 1:
        service_name = sys.argv[1]

    zeroconf = Zeroconf(ip_version=IPVersion.All)
    listener = ServiceListener()
    # Browse for the specified service
    browser = ServiceBrowser(zeroconf, service_name, listener)
    
    print(f"🔍 Starting mDNS scan for {service_name} services...")
    print("Press Ctrl+C to exit.")
    
    try:
        # Let the script run and listen for services
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        zeroconf.close()
