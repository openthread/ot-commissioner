#!/usr/bin/env python3
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
"""
Thread 1.2 commissioner interface implementation
"""

from future.utils import raise_

import serial
import time
import re
import uuid
import json
import base64
import binascii
import sys

import commissioner

from commissioner import TLV_TYPE_BORDER_AGENT_LOCATOR
from commissioner import TLV_TYPE_COMM_SESSION_ID
from commissioner import TLV_TYPE_STEERING_DATA
from commissioner import TLV_TYPE_AE_STEERING_DATA
from commissioner import TLV_TYPE_NMKP_STEERING_DATA
from commissioner import TLV_TYPE_JOINER_UDP_PORT
from commissioner import TLV_TYPE_AE_UDP_PORT
from commissioner import TLV_TYPE_NMKP_UDP_PORT
from commissioner import TLV_TYPE_ACTIVE_TIMESTAMP
from commissioner import TLV_TYPE_CHANNEL
from commissioner import TLV_TYPE_CHANNEL_MASK
from commissioner import TLV_TYPE_EXTENDED_PAN_ID
from commissioner import TLV_TYPE_MESH_LOCAL_PREFIX
from commissioner import TLV_TYPE_NETWORK_MASTER_KEY
from commissioner import TLV_TYPE_NETWORK_NAME
from commissioner import TLV_TYPE_PAN_ID
from commissioner import TLV_TYPE_PSKC
from commissioner import TLV_TYPE_SECURITY_POLICY
from commissioner import TLV_TYPE_DELAY_TIMER
from commissioner import TLV_TYPE_PENDING_TIMESTAMP
from commissioner import TLV_TYPE_TRI_HOSTNAME
from commissioner import TLV_TYPE_REGISTRAR_HOSTNAME
from commissioner import TLV_TYPE_REGISTRAR_IPv6_ADDR
from commissioner import JOINER_TYPE_MESHCOP
from commissioner import JOINER_TYPE_CCM_AE
from commissioner import JOINER_TYPE_CCM_NMKP
from commissioner import ICommissioner

from GRLLibs.UtilityModules.enums import PlatformDiagnosticPacket_Direction, PlatformDiagnosticPacket_Type
from GRLLibs.ThreadPacket.PlatformPackets import PlatformDiagnosticPacket, PlatformPackets

NEW_LINE = re.compile(r'\r\n|\n')
COMMISSIONER_USER = 'pi'
COMMISSIONER_PASSWORD = 'raspberry'
COMMISSIONER_PROMPT = 'pi@raspberry'
COMMISSIONER_CTL = 'sudo commissioner_ctl.py'

# Command line length cannot exceed this
TTY_COLS = 4096

CONTROL_SEQUENCE = re.compile(r'\x1b'  # ESC
                              r'\['  # CSI
                              r'[0-?]*'  # Parameter bytes
                              r'[!-/]*'  # Intermediate bytes
                              r'[@-~]'  # Final byte
                             )

TLV_TYPE_TO_STRING = {
    TLV_TYPE_BORDER_AGENT_LOCATOR: 'BorderAgentLocator',
    TLV_TYPE_COMM_SESSION_ID: 'SessionId',
    TLV_TYPE_STEERING_DATA: 'SteeringData',
    TLV_TYPE_AE_STEERING_DATA: 'AeSteeringData',
    TLV_TYPE_NMKP_STEERING_DATA: 'NmkpSteeringData',
    TLV_TYPE_JOINER_UDP_PORT: 'JoinerUdpPort',
    TLV_TYPE_AE_UDP_PORT: 'AeUdpPort',
    TLV_TYPE_NMKP_UDP_PORT: 'NmkpUdpPort',
    TLV_TYPE_ACTIVE_TIMESTAMP: 'ActiveTimestamp',
    TLV_TYPE_CHANNEL: 'Channel',
    TLV_TYPE_CHANNEL_MASK: 'ChannelMask',
    TLV_TYPE_EXTENDED_PAN_ID: 'ExtendedPanId',
    TLV_TYPE_MESH_LOCAL_PREFIX: 'MeshLocalPrefix',
    TLV_TYPE_NETWORK_MASTER_KEY: 'NetworkMasterKey',
    TLV_TYPE_NETWORK_NAME: 'NetworkName',
    TLV_TYPE_PAN_ID: 'PanId',
    TLV_TYPE_PSKC: 'PSKc',
    TLV_TYPE_SECURITY_POLICY: 'SecurityPolicy',
    TLV_TYPE_DELAY_TIMER: 'DelayTimer',
    TLV_TYPE_PENDING_TIMESTAMP: 'PendingTimestamp',
    TLV_TYPE_TRI_HOSTNAME: 'TriHostname',
    TLV_TYPE_REGISTRAR_HOSTNAME: 'RegistrarHostname',
    TLV_TYPE_REGISTRAR_IPv6_ADDR: 'RegistrarIpv6Addr',
}

TLV_TYPE_FROM_STRING = {
    TLV_TYPE_TO_STRING[key]: key for key in TLV_TYPE_TO_STRING
}

JOINER_TYPE_TO_STRING = {
    JOINER_TYPE_MESHCOP: 'meshcop',
    JOINER_TYPE_CCM_AE: 'ae',
    JOINER_TYPE_CCM_NMKP: 'nmkp',
}


class OTCommissioner(ICommissioner):

    def __init__(self, config, handler, simulator=None):
        super(OTCommissioner, self).__init__(config)

        self.enable_dtls_debug_logging = True
        self.logging_level = 'debug'
        self.keep_alive_interval = 40
        self.max_connection_num = 100
        self.log_file = '/tmp/commissioner.log'

        self._simulator = simulator
        self._handler = handler
        self._lines = []

        self._command(f'stty cols {TTY_COLS}')

        config_path = f'/tmp/commissioner.{uuid.uuid4()}.json'
        self._write_config(config_path=config_path, config=config)

        response = self._command(f'{COMMISSIONER_CTL} init "{config_path}"')
        if self._command('echo $?')[0] != '0':
            raise commissioner.Error(
                f"Failed to init, error:\n{'\n'.join(response)}")

    @staticmethod
    def makeLocalCommissioner(config, simulator):
        import pexpect
        handler = pexpect.spawn("/bin/bash")
        return OTCommissioner(config, handler, simulator)

    @staticmethod
    def makeHarnessCommissioner(config, serial_handler):
        if not isinstance(serial_handler, serial.Serial):
            raise commissioner.Error("expect a serial handler")
        return OTCommissioner(config, serial_handler)

    def start(self, borderAgentAddr, borderAgentPort):
        self._command(f'sudo rm {self.log_file}')
        self._command(f'sudo touch {self.log_file}')
        self._execute_and_check(f'start {borderAgentAddr} {borderAgentPort}')

    def stop(self):
        self._execute_and_check('stop')

    def isActive(self):
        response = self._execute_and_check('active')
        if 'true' in response[0]:
            return True
        elif 'false' in response[0]:
            return False
        else:
            raise commissioner.Error(f'Unrecognized result "{response[0]}"')

    def getSessionId(self):
        response = self._execute_and_check('sessionid')
        try:
            return int(response[0])
        except ValueError as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_COMMISSIONER_GET(self, tlvTypes):
        types = ' '.join([TLV_TYPE_TO_STRING[x] for x in tlvTypes])
        command = 'commdataset get ' + types
        response = self._execute_and_check(command)

        try:
            result = json.loads(' '.join(response[:-1]))
            result = {TLV_TYPE_FROM_STRING[key]: result[key] for key in result}

            for key in [
                    TLV_TYPE_STEERING_DATA, TLV_TYPE_AE_STEERING_DATA,
                    TLV_TYPE_NMKP_STEERING_DATA
            ]:
                if key in result:
                    result[key] = OTCommissioner._hex_to_bytes(result[key])

            return result
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_COMMISSIONER_SET(self, commDataset):
        for key in [
                TLV_TYPE_STEERING_DATA, TLV_TYPE_AE_STEERING_DATA,
                TLV_TYPE_NMKP_STEERING_DATA
        ]:
            if key in commDataset:
                commDataset[key] = OTCommissioner._bytes_to_hex(
                    commDataset[key])

        dataset = {
            TLV_TYPE_TO_STRING[key]: commDataset[key] for key in commDataset
        }
        data = json.dumps(dataset)
        self._execute_and_check(f"commdataset set '{data}'")

    def enableJoiner(self, joinerType, eui64=None, password=None):
        command = ['joiner', 'enable', JOINER_TYPE_TO_STRING[joinerType]]

        if eui64:
            command.append(str(eui64))
        else:
            command[1] = 'enableall'

        if password:
            command.append(password)

        self._execute_and_check(' '.join(command))

    def disableJoiner(self, joinerType, eui64=None):
        command = ['joiner', 'disable', JOINER_TYPE_TO_STRING[joinerType]]

        if eui64:
            command.append(str(eui64))
        else:
            command[1] = 'disableall'

        self._execute_and_check(' '.join(command))

    def MGMT_ACTIVE_GET(self, tlvTypes):
        types = ' '.join([TLV_TYPE_TO_STRING[x] for x in tlvTypes])
        result = self._execute_and_check(f'opdataset get active {types}')

        try:
            return OTCommissioner._active_op_dataset_from_json(' '.join(
                result[:-1]))
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_ACTIVE_SET(self, activeOpDataset):
        self._execute_and_check(
            f"opdataset set active '{OTCommissioner._active_op_dataset_to_json(activeOpDataset)}'"
        )

    def MGMT_PENDING_GET(self, tlvTypes):
        types = ' '.join([TLV_TYPE_TO_STRING[x] for x in tlvTypes])
        result = self._execute_and_check(f'opdataset get pending {types}')

        try:
            return OTCommissioner._pending_op_dataset_from_json(' '.join(
                result[:-1]))
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_PENDING_SET(self, pendingOpDataset):
        self._execute_and_check(
            f"opdataset set pending '{OTCommissioner._pending_op_dataset_to_json(pendingOpDataset)}'"
        )

    def MGMT_BBR_GET(self, tlvTypes):
        types = ' '.join([TLV_TYPE_TO_STRING[x] for x in tlvTypes])
        result = self._execute_and_check(f'bbrdataset get {types}')

        try:
            result = json.loads(' '.join(result[:-1]))
            return {TLV_TYPE_FROM_STRING[key]: result[key] for key in result}
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_BBR_SET(self, bbrDataset):
        dataset = {
            TLV_TYPE_TO_STRING[key]: bbrDataset[key] for key in bbrDataset
        }
        dataset = json.dumps(dataset)
        self._execute_and_check(f"bbrdataset set '{dataset}'")

    def MLR(self, multicastAddrs, timeout):
        self._execute_and_check(f"mlr {' '.join(multicastAddrs)} {timeout}",
                                check=False)

    def MGMT_ANNOUNCE_BEGIN(self, channelMask, count, period, dstAddr):
        self._execute_and_check(
            f'announce {channelMask} {count} {period} {dstAddr}')

    def MGMT_PANID_QUERY(self, channelMask, panId, dstAddr, timeout):
        self._execute_and_check(f'panid query {channelMask} {panId} {dstAddr}')
        self._sleep(timeout)
        result = self._execute_and_check(f'panid conflict {panId}')
        result = int(result[0])
        return False if result == 0 else True

    def MGMT_ED_SCAN(self, channelMask, count, period, scanDuration, dstAddr,
                     timeout):
        self._execute_and_check(
            f'energy scan {channelMask} {count} {period} {scanDuration} {dstAddr}'
        )

        self._sleep(timeout)
        result = self._execute_and_check(f'energy report {dstAddr}')
        if result[0] == 'null':
            raise commissioner.Error(f'No energy report found for {dstAddr}')

        try:
            result = json.loads(' '.join(result[:-1]))

            return OTCommissioner.EnergyReport(
                channelMask=self._channel_mask_from_json_obj(
                    result['ChannelMask']),
                energyList=self._hex_to_bytes(result['EnergyList']),
            )
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_REENROLL(self, dstAddr):
        self._execute_and_check(f'reenroll {dstAddr}')

    def MGMT_DOMAIN_RESET(self, dstAddr):
        self._execute_and_check(f'domainreset {dstAddr}')

    def MGMT_NET_MIGRATE(self, dstAddr, designatedNetwork):
        self._execute_and_check(f'migrate {dstAddr} {designatedNetwork}')

    def requestCOM_TOK(self, registrarAddr, registrarPort):
        self._execute_and_check(
            f'token request {registrarAddr} {registrarPort}')

    def setCOM_TOK(self, signedCOM_TOK):
        path_token = f'/tmp/commissioner.token.{uuid.uuid4()}'
        step = 40
        for i in range(0, len(signedCOM_TOK), step):
            data = self._bytes_to_hex(signedCOM_TOK[i:i + step])
            self._command(f'echo {data} >> "{path_token}"')

        self._execute_and_check(f'token set {path_token}')

    def getCOM_TOK(self):
        result = self._execute_and_check('token print')
        return self._hex_to_bytes(result[0])

    def getCommissioningLogs(self):
        processed_logs = []
        for log in self._getThciLogs():
            if "JOIN_FIN.req:" in log:
                encrypted_packet = PlatformDiagnosticPacket()
                hex_value = encrypted_packet.split("JOIN_FIN.req:")[-1].strip()
                payload = list(bytearray.fromhex(hex_value))
                encrypted_packet.Direction = PlatformDiagnosticPacket_Direction.IN
                encrypted_packet.Type = PlatformDiagnosticPacket_Type.JOIN_FIN_req
                encrypted_packet.TLVsLength = len(payload)
                encrypted_packet.TLVs = PlatformPackets.read(
                    encrypted_packet.Type, payload)
                processed_logs.append(encrypted_packet)
            elif "JOIN_FIN.rsp:" in log:
                encrypted_packet = PlatformDiagnosticPacket()
                hex_value = encrypted_packet.split("JOIN_FIN.rsp:")[-1].strip()
                payload = list(bytearray.fromhex(hex_value))
                encrypted_packet.Direction = PlatformDiagnosticPacket_Direction.OUT
                encrypted_packet.Type = PlatformDiagnosticPacket_Type.JOIN_FIN_rsp
                encrypted_packet.TLVsLength = len(payload)
                encrypted_packet.TLVs = PlatformPackets.read(
                    encrypted_packet.Type, payload)
                processed_logs.append(encrypted_packet)
        return processed_logs

    def getMlrLogs(self):
        processed_logs = []
        for mlr in [log for log in self._getThciLogs() if "MLR.rsp" in log]:
            encrypted_packet = PlatformDiagnosticPacket()
            hex_value = mlr.split("MLR.rsp:")[-1].strip()
            payload = list(bytearray.fromhex(hex_value))
            encrypted_packet.Direction = PlatformDiagnosticPacket_Direction.OUT
            encrypted_packet.Type = PlatformDiagnosticPacket_Type.MLR_rsp
            encrypted_packet.TLVsLength = len(payload)
            encrypted_packet.TLVs = PlatformPackets.read(
                encrypted_packet.Type, payload)
            processed_logs.append(encrypted_packet)
        return processed_logs

    def _getThciLogs(self):
        return self._command(f"grep \"\\[ thci \\]\" {self.log_file}")

    def _execute_and_check(self, command, check=True):
        # Escape quotes for bash
        command = command.replace('"', r'"\""')
        response = self._command(f'{COMMISSIONER_CTL} execute "{command}"')
        if check:
            response = OTCommissioner._check_response(response)
        return response

    def _sleep(self, duration):
        time.sleep(duration)

    def _command(self, cmd, timeout=10):
        lines = self._handler.bash(cmd, timeout=timeout)
        lines = [re.sub(r'\x1b\[\d+m', '', l) for l in lines]
        lines = [l for l in lines if l]
        return lines

    def _write_config(self, config_path, config):
        data = {
            'EnableCcm': config.isCcmMode,
            'Id': config.id,
            'PSKc': binascii.hexlify(config.pskc).decode(),
            'DomainName': config.domainName,
            'EnableDtlsDebugLogging': self.enable_dtls_debug_logging,
            'LogLevel': self.logging_level,
            'KeepAliveInterval': self.keep_alive_interval,
            'MaxConnectionNum': self.max_connection_num,
            'LogFile': self.log_file,
        }

        if config.isCcmMode:
            if config.privateKey:
                path = f'/tmp/commissioner.private_key.{uuid.uuid4()}'
                self._send_file(local_path=config.privateKey, remote_path=path)
                data['PrivateKeyFile'] = path
            if config.cert:
                path = f'/tmp/commissioner.cert.{uuid.uuid4()}'
                self._send_file(local_path=config.cert, remote_path=path)
                data['CertificateFile'] = path
            if config.trustAnchor:
                path = f'/tmp/commissioner.trush_anchor.{uuid.uuid4()}'
                self._send_file(local_path=config.trustAnchor, remote_path=path)
                data['TrustAnchorFile'] = path

        self._command(f"echo '{json.dumps(data)}' >> '{config_path}'")

    def _send_file(self, local_path, remote_path):
        with open(local_path, 'rb') as f:
            b64 = base64.b64encode(f.read()).decode()
        self._command(f'echo "{b64}" | base64 -d - > "{remote_path}"')

    @staticmethod
    def _check_response(response):
        if response[-1] != '[done]':
            raise commissioner.Error(f'Error message:\n{response!r}')
        return response

    @staticmethod
    def _hex_to_bytes(hex_string):
        return bytes(bytearray.fromhex(hex_string))

    @staticmethod
    def _bytes_to_hex(byte_string):
        return binascii.hexlify(bytearray(byte_string)).decode()

    @staticmethod
    def _active_op_dataset_from_json(json_str):
        result = json.loads(json_str)
        result = {TLV_TYPE_FROM_STRING[key]: result[key] for key in result}

        if TLV_TYPE_ACTIVE_TIMESTAMP in result:
            timestamp = result[TLV_TYPE_ACTIVE_TIMESTAMP]
            result[TLV_TYPE_ACTIVE_TIMESTAMP] = \
                OTCommissioner._timestamp_from_json_obj(timestamp)

        if TLV_TYPE_CHANNEL in result:
            channel = result[TLV_TYPE_CHANNEL]
            result[TLV_TYPE_CHANNEL] = OTCommissioner.Channel(
                number=channel['Number'],
                page=channel['Page'],
            )

        if TLV_TYPE_CHANNEL_MASK in result:
            result[TLV_TYPE_CHANNEL_MASK] = \
                OTCommissioner._channel_mask_from_json_obj(
                    result[TLV_TYPE_CHANNEL_MASK])

        if TLV_TYPE_EXTENDED_PAN_ID in result:
            xpanid = result[TLV_TYPE_EXTENDED_PAN_ID]
            result[TLV_TYPE_EXTENDED_PAN_ID] = OTCommissioner._hex_to_bytes(
                xpanid)

        if TLV_TYPE_NETWORK_MASTER_KEY in result:
            key = result[TLV_TYPE_NETWORK_MASTER_KEY]
            result[TLV_TYPE_NETWORK_MASTER_KEY] = OTCommissioner._hex_to_bytes(
                key)

        if TLV_TYPE_SECURITY_POLICY in result:
            security_policy = result[TLV_TYPE_SECURITY_POLICY]
            result[TLV_TYPE_SECURITY_POLICY] = OTCommissioner.SecurityPolicy(
                flags=security_policy['Flags'],
                rotationTime=security_policy['RotationTime'],
            )

        return result

    @staticmethod
    def _active_op_dataset_to_json(dataset):
        if TLV_TYPE_ACTIVE_TIMESTAMP in dataset:
            dataset[TLV_TYPE_ACTIVE_TIMESTAMP] = \
                OTCommissioner._timestamp_to_json_obj(
                    dataset[TLV_TYPE_ACTIVE_TIMESTAMP])

        if TLV_TYPE_CHANNEL in dataset:
            channel = dataset[TLV_TYPE_CHANNEL]
            dataset[TLV_TYPE_CHANNEL] = {
                'Number': channel.number,
                'Page': channel.page,
            }

        if TLV_TYPE_CHANNEL_MASK in dataset:
            dataset[TLV_TYPE_CHANNEL_MASK] = \
                OTCommissioner._channel_mask_to_json_obj(
                    dataset[TLV_TYPE_CHANNEL_MASK])

        if TLV_TYPE_EXTENDED_PAN_ID in dataset:
            xpanid = dataset[TLV_TYPE_EXTENDED_PAN_ID]
            dataset[TLV_TYPE_EXTENDED_PAN_ID] = OTCommissioner._bytes_to_hex(
                xpanid)

        if TLV_TYPE_NETWORK_MASTER_KEY in dataset:
            key = dataset[TLV_TYPE_NETWORK_MASTER_KEY]
            dataset[TLV_TYPE_NETWORK_MASTER_KEY] = OTCommissioner._bytes_to_hex(
                key)

        if TLV_TYPE_SECURITY_POLICY in dataset:
            security_policy = dataset[TLV_TYPE_SECURITY_POLICY]
            dataset[TLV_TYPE_SECURITY_POLICY] = {
                'Flags': security_policy.flags,
                'RotationTime': security_policy.rotationTime,
            }

        dataset = {TLV_TYPE_TO_STRING[key]: dataset[key] for key in dataset}
        return json.dumps(dataset)

    @staticmethod
    def _pending_op_dataset_from_json(json_str):
        result = OTCommissioner._active_op_dataset_from_json(json_str)

        if TLV_TYPE_PENDING_TIMESTAMP in result:
            timestamp = result[TLV_TYPE_PENDING_TIMESTAMP]
            result[TLV_TYPE_PENDING_TIMESTAMP] = \
                OTCommissioner._timestamp_from_json_obj(timestamp)

        return result

    @staticmethod
    def _pending_op_dataset_to_json(dataset):
        if TLV_TYPE_PENDING_TIMESTAMP in dataset:
            dataset[TLV_TYPE_PENDING_TIMESTAMP] = \
                OTCommissioner._timestamp_to_json_obj(
                    dataset[TLV_TYPE_PENDING_TIMESTAMP])

        return OTCommissioner._active_op_dataset_to_json(dataset)

    @staticmethod
    def _timestamp_from_json_obj(json_obj):
        return OTCommissioner.Timestamp(
            seconds=json_obj['Seconds'],
            ticks=json_obj['Ticks'],
            u=False if json_obj['U'] == 0 else True,
        )

    @staticmethod
    def _timestamp_to_json_obj(timestamp):
        return {
            'Seconds': timestamp.seconds,
            'Ticks': timestamp.ticks,
            'U': 1 if timestamp.u else 0,
        }

    @staticmethod
    def _channel_mask_from_json_obj(json_obj):
        result = []
        for entry in json_obj:
            result.append(
                OTCommissioner.ChannelMaskEntry(
                    masks=OTCommissioner._hex_to_bytes(entry['Masks']),
                    page=entry['Page'],
                ))
        return result

    @staticmethod
    def _channel_mask_to_json_obj(channel_mask):
        result = []
        for entry in channel_mask:
            result.append({
                'Masks': OTCommissioner._bytes_to_hex(entry.masks),
                'Page': entry.page,
            })
        return result
