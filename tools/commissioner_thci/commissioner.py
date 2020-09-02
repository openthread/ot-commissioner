#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   Copyright (c) 2019, The OpenThread Commissioner Authors.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   3. Neither the name of the copyright holder nor the
#      names of its contributors may be used to endorse or promote products
#      derived from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#   POSSIBILITY OF SUCH DAMAGE.
"""This file defines the Thread 1.2 commissioner interface.
"""

from abc import abstractmethod

# Joiner Types
JOINER_TYPE_MESHCOP = 0
JOINER_TYPE_CCM_AE = 1
JOINER_TYPE_CCM_NMKP = 2

# Commissioner Dataset TLVs
TLV_TYPE_BORDER_AGENT_LOCATOR = 9
TLV_TYPE_COMM_SESSION_ID = 11
TLV_TYPE_STEERING_DATA = 8
TLV_TYPE_AE_STEERING_DATA = 61
TLV_TYPE_NMKP_STEERING_DATA = 62
TLV_TYPE_JOINER_UDP_PORT = 18
TLV_TYPE_AE_UDP_PORT = 65
TLV_TYPE_NMKP_UDP_PORT = 66

# Active Operational Dataset TLVs
TLV_TYPE_ACTIVE_TIMESTAMP = 14
TLV_TYPE_CHANNEL = 0
TLV_TYPE_CHANNEL_MASK = 53
TLV_TYPE_EXTENDED_PAN_ID = 2
TLV_TYPE_MESH_LOCAL_PREFIX = 7
TLV_TYPE_NETWORK_MASTER_KEY = 5
TLV_TYPE_NETWORK_NAME = 3
TLV_TYPE_PAN_ID = 1
TLV_TYPE_PSKC = 4
TLV_TYPE_SECURITY_POLICY = 12

# Opending Operational Dataset TLVs
TLV_TYPE_DELAY_TIMER = 52
TLV_TYPE_PENDING_TIMESTAMP = 51

# Backbone Router Dataset TLVs
TLV_TYPE_TRI_HOSTNAME = 67
TLV_TYPE_REGISTRAR_HOSTNAME = 69
TLV_TYPE_REGISTRAR_IPv6_ADDR = 68


class Error(Exception):
    """Base class for exceptions of commissioner."""

    pass


# TODO: specific exceptions definition ?


class Configuration(object):
    """The configuration for initializing a commissioner.

    Attributes:
        isCcmMode: A boolean indicating if we run in CCM mode.
            Decides credentials to be used and message signing.
        id: A string represents the commissioner ID.
        pskc: A byte array represents the PSKc for non-CCM network.
        privateKey: The private key file name.
        cert: The certificate file name.
        trustAnchor: The trust anchor file name.
        domainName: The domain name of connecting Thread network.
    """

    def __init__(self, id="thread-comm"):
        self.isCcmMode = True
        self.id = id
        self.pskc = bytearray.fromhex('00112233445566778899aabbccddeeff')
        self.privateKey = "/etc/commissioner/private_key.pem"
        self.cert = "/etc/commissioner/certificate.pem"
        self.trustAnchor = "/etc/commissioner/trust_anchor.pem"
        self.domainName = 'Thread'


class ICommissioner(object):
    """The Thread 1.2 Commissioner interface.
    """

    def __init__(self, config):
        """Initialize a commissioner instance with configuration.

        Args:
            config: A Configuration instance.
        """
        pass

    @abstractmethod
    def start(self, borderAgentAddr, borderAgentPort):
        """Start petitioning to given border agent.
        Example: Start("::", 49191)

        Args:
            borderAgentAddr: The IPv6 address of given border agent.
            borderAgentPort: The listening UDP port of given border agent.

        Raises:
            Error: An error occurred when petitioning.
        """
        pass

    @abstractmethod
    def stop(self):
        """Stop the commissioner.

        Raises:
            Error: An error occurred when stopping.
        """
        pass

    @abstractmethod
    def isActive(self):
        """Decide if the commissioner is currently active.

        Returns:
            A boolean indicates wether this commissioner is active or not.

        Raises:
            Error: An error occurred when requesting commissioner state.
        """
        pass

    @abstractmethod
    def getSessionId(self):
        """Get the commissioner session ID.

        Returns:
            An integer represents the commissioner session ID.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def abortRequests(self):
        """Abort all outstanding requests.
        Used in case the user wants to terminate all requests immediately.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_COMMISSIONER_GET(self, tlvTypes):
        """Get the Commissioner Dataset by sending MGMT_COMMISSIONER_GET.req.

        Args:
            tlvTypes: A TLV type list of wanted TLVs.

        Returns:
            A dict mapping TLV types to corresponding data. A complete
            Commissioner Dataset example:

            {
                TLV_TYPE_BORDER_AGENT_LOCATOR : 1022,
                TLV_TYPE_COMM_SESSION_ID : 57,
                TLV_TYPE_STEERING_DATA : b'\xFF',
                TLV_TYPE_AE_STEERING_DATA : b'\x00',
                TLV_TYPE_NMKP_STEERING_DATA : b'\x00',
                TLV_TYPE_JOINER_UDP_PORT : 1000,
                TLV_TYPE_AE_UDP_PORT : 1001,
                TLV_TYPE_NMKP_UDP_PORT : 1002
            }

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_COMMISSIONER_SET(self, commDataset):
        """Set the Commissioner Dataset by sending MGMT_COMMISSIONER_SET.req.

        Args:
            commDataset: A commissioner dataset to be set.
                See 'MGMT_COMMISSIONER_GET()' for example.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def enableJoiner(self, joinerType, eui64=None, password=None):
        """Enables a joiner for given type of joining.

        This method enables a joiner by adding its eui64 to corresponding
        steering data and send MGMT_COMMISSIONER_SET.req to the leader.

        Args:
            joinerType: A value of JOINER_TYPE_XXX indicates the joiner type.
            eui64: An optional integer represents the 64-bit Extended Unique
                Identifier. If not specified, all joiners are enabled.
            password: String, joiner's password.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def disableJoiner(self, joinerType, eui64=None):
        """Disables a joiner for given type of joining.

        This method disables a joiner by removing its eui64 from corresponding
        steering data and send MGMT_COMMISSIONER_SET.req to the leader.

        Due to the theory of bloom filter, a disabled joiner could possiblely
        be allowed for joining.

        Args:
            joinerType: A value of JOINER_TYPE_XXX indicates the joiner type.
            eui64: An optional integer represents the 64-bit Extended Unique
                Identifier. If not specified, all joiners are disabled.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_ACTIVE_GET(self, tlvTypes):
        """Get the Active Operational Dataset by sending MGMT_ACTIVE_GET.req.

        Args:
            tlvTypes: A TLV type list of wanted TLVs.

        Returns:
            A dict mapping TLV types to corresponding data. A complete
            Active Operational Dataset example:

            {
                TLV_TYPE_ACTIVE_TIMESTAMP : Timestamp(128, 12, False),
                TLV_TYPE_CHANNEL : Channel(19, 0),
                TLV_TYPE_CHANNEL_MASK : [ChannelMaskEntry(0, b'\x00\x00\x00\x00')],
                TLV_TYPE_EXTENDED_PAN_ID : b'\xDE\xAD\x00\xBE\xEF\x00\xCA\xFE',
                TLV_TYPE_MESH_LOCAL_PREFIX : "fdde:ad00:beef::/64",
                TLV_TYPE_NETWORK_MASTER_KEY : b'\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff'
                TLV_TYPE_NETWORK_NAME : "openthread-test",
                TLV_TYPE_PAN_ID : 0xFACE,
                TLV_TYPE_PSKC : b'\x3a\xa5\x5f\x91\xca\x47\xd1\xe4\xe7\x1a\x08\xcb\x35\xe9\x15\x91',
                TLV_TYPE_SECURITY_POLICY : SecurityPolicy(100, 0x00F8)
            }

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_ACTIVE_SET(self, activeOpDataset):
        """Set the Active Operational Dataset by sending MGMT_ACTIVE_SET.req.

        Args:
            activeOpDataset: A Active Operational Dataset to be set.
                See 'MGMT_ACTIVE_GET()' for example.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_PENDING_GET(self, tlvTypes):
        """Get the Pending Operational Dataset by sending MGMT_PENDING_GET.req.

        Args:
            tlvTypes: A TLV type list of wanted TLVs.

        Returns:
            A dict mapping TLV types to corresponding data. A complete
            Pending Operational Dataset example:

            {
                TLV_TYPE_DELAY_TIMER : 1000,
                TLV_TYPE_PENDING_TIMESTAMP : Timestamp(129, 12, False),
                TLV_TYPE_ACTIVE_TIMESTAMP : Timestamp(128, 12, False),
                TLV_TYPE_CHANNEL : Channel(19, 0),
                TLV_TYPE_CHANNEL_MASK : [ChannelMaskEntry(0, b'\x00\x00\x00\x00')],
                TLV_TYPE_EXTENDED_PAN_ID : b'\xDE\xAD\x00\xBE\xEF\x00\xCA\xFE',
                TLV_TYPE_MESH_LOCAL_PREFIX : "fdde:ad00:beef::/64",
                TLV_TYPE_NETWORK_MASTER_KEY : b'\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff'
                TLV_TYPE_NETWORK_NAME : "openthread-test",
                TLV_TYPE_PAN_ID : 0xFACE,
                TLV_TYPE_PSKC : b'\x3a\xa5\x5f\x91\xca\x47\xd1\xe4\xe7\x1a\x08\xcb\x35\xe9\x15\x91',
                TLV_TYPE_SECURITY_POLICY : SecurityPolicy(100, 0x00F8)
            }

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_PENDING_SET(self, pendingOpDataset):
        """Set the Pending Operational Dataset by sending MGMT_PENDING_SET.req.

        Args:
            pendingOpDataset: A Pending Operational Dataset to be set.
                See 'MGMT_PENDING_GET()' for example.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_BBR_GET(self, tlvTypes):
        """Get the BBR Dataset by sending MGMT_BBR_GET.req.

        Available only in CCM mode.

        Args:
            tlvTypes: A TLV type list of wanted TLVs.

        Returns:
            A dict mapping TLV types to corresponding data. A complete
            BBR Dataset example:

            {
                TLV_TYPE_TRI_HOSTNAME : "[fdaa:bb::de6]",
                TLV_TYPE_REGISTRAR_HOSTNAME : "[fdaa:bb::de6]",
                TLV_TYPE_REGISTRAR_IPv6_ADDR : "fdaa:bb::de6"
            }

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_BBR_SET(self, bbrDataset):
        """Set the BBR dataset by sending MGMT_BBR_SET.req.

        Available only in CCM mode.

        Args:
            bbrDataset: A BBR Dataset to be set.
                See 'MGMT_BBR_GET()' for example.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MLR(self, multicastAddrs, timeout):
        """Request the PBBR to register multicast listener.

        Args:
            multicastAddrs: A list of string which represents A IPv6
                multicast address.
            timeout: A time period after which the registration as a listener
                to the included multicast group(s) expires; In seconds.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_ANNOUNCE_BEGIN(self, channelMask, count, period, dstAddr):
        """Request devices to initiate Operational Dataset Announcements.

        Args:
            channelMask: A byte array identifies the set of
                channels used to transmit MLE Announce messages.
            count: An integer identifies the number of MLE
                Announce transmissions per channel.
            period: An integer identifies the period in
                milliseconds between successive MLE Announce transmissions.
            dstAddr: A string represents the IPv6 address of the dest devices.
                It can be both IPv6 unicast & multicast address.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_PANID_QUERY(self, channelMask, panId, dstAddr, timeout):
        """Request devices to detect PAN ID conflicts.

        Args:
            channelMask: A byte array specifies the ScanChannels and
                ChannelPage when performing an IEEE 802.15.4 Active Scan.
            panId: An integer specifies the PAN ID that is used to
                check for conflicts.
            dstAddr: A string represents the IPv6 address of the dest devices.
                It can be both IPv6 unicast & multicast address.
            timeout: An integer specifies how long (in seconds) to wait
                for PANID_CONFLICT responses.

        Returns:
            A boolean indicates wether there are PAN ID conflicts.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_ED_SCAN(self, channelMask, count, period, scanDuration, dstAddr,
                     timeout):
        """Request devices to measure energy on one or more channels.

        Args:
            channelMask: A byte array specifies the ScanChannels and
                ChannelPage when performing an IEEE 802.15.4 ED Scan.
            count: An integer identifies the number of IEEE 802.15.4
                ED Scans per channel.
            period: An integer identifies the period between successive
                IEEE 802.15.4 ED Scans.
            scanDuration: Identifies the IEEE 802.15.4 ScanDuration
                to use when performing an IEEE 802.15.4 ED Scan.
            dstAddr: A string represents the IPv6 address of the dest devices.
                It can be both IPv6 unicast & multicast address.
            timeout:  An integer specifies how long (in seconds) to wait
                for ENERGY_REPORT responses.

        Returns:
            The corresponding energy report. Example:

            EnergyReport(
                [ChannelMaskEntry(b'\x00\x1f\xf8\x00', 0)],
                b'\x9e\xe2\xe2\xe2\xe2\xe2\xe2\xe2\xe2\xe2',
            ),

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_REENROLL(self, dstAddr):
        """Request a device to perform re-enrollment.

        Available only in CCM mode.

        Args:
            dstAddr: A string represents the IPv6 address of the dest device.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_DOMAIN_RESET(self, dstAddr):
        """Request a device to perform domain reset.

        Available only in CCM mode.

        Args:
            dstAddr: A string represents the IPv6 address of the dest device.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def MGMT_NET_MIGRATE(self, dstAddr, designatedNetwork):
        """Request a device to migrate to the designated network.

        Available only in CCM mode.

        Args:
            dstAddr: A string represents the IPv6 address of the dest device.
            designatedNetwork: A string represents the designated network name
                this device will migrate to.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def requestCOM_TOK(self, registrarAddr, registrarPort):
        """Request Commissioner Token from registrar.

        Available only in CCM mode.

        Args:
            registrarAddr: A string represents the
                registrar address (IPv6 or IPv4).
            registrarPort: An integer represents the registrar listening port.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def setCOM_TOK(self, signedCOM_TOK, verifyCert):
        """Force set the Commissioner Token.

        This method set the Commissioner Token to the given signed token
        if the signature verification succeed.

        Available only in CCM mode.

        Args:
            signedCOM_TOK: A byte array represents the COSE_SIGN1 signed
                Commissioner Token.
            verifyCert: A byte array represents the PEM/DER encoded
                certificate used to verify the signature of the token.

        Raises:
            Error:
        """
        pass

    @abstractmethod
    def getCOM_TOK(self):
        """Get the signed Commissioner Token.

        Available only in CCM mode.

        Returns:
            A byte array represents the COSE_SIGN1 signed Commissioner Token.
            An empty byte array is returned if the Commissioner Token hasn't
            been set yet.
        """
        pass

    @abstractmethod
    def getCommissioningLogs(self):
        """Get Commissioning logs

        Returns:
           Commissioning logs
        """
        pass

    class Timestamp:
        """Timestamp class in operational dataset"""

        def __init__(self, seconds, ticks, u):
            self._seconds = seconds
            self._ticks = ticks
            self._u = u

        @property
        def seconds(self):
            return self._seconds

        @property
        def ticks(self):
            return self._ticks

        @property
        def u(self):
            return self._u

    class Channel:
        """Channel class in operational dataset"""

        def __init__(self, number, page):
            self._number = number
            self._page = page

        @property
        def number(self):
            return self._number

        @property
        def page(self):
            return self._page

    class ChannelMaskEntry:
        """ChannelMaskEntry class in operational dataset"""

        def __init__(self, masks, page):
            self._masks = masks
            self._page = page

        @property
        def masks(self):
            return self._masks

        @property
        def page(self):
            return self._page

    class SecurityPolicy:
        """SecurityPolicy class in operational dataset"""

        def __init__(self, rotationTime, flags):
            self._rotationTime = rotationTime
            self._flags = flags

        @property
        def rotationTime(self):
            return self._rotationTime

        @property
        def flags(self):
            return self._flags

    class EnergyReport:
        """EnergyReport class returned by MGMT_ED_SCAN"""

        def __init__(self, channelMask, energyList):
            self._channelMask = channelMask
            self._energyList = energyList

        @property
        def channelMask(self):
            return self._channelMask

        @property
        def energyList(self):
            return self._energyList
