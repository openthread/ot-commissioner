#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   Copyright (c) 2019, The OpenThread Authors.
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

from abc import abstractmethod
from future.utils import raise_

import serial
import time
import socket
import re
import uuid
import json
import base64
import logging
import binascii
import sys
import os

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

NEW_LINE = re.compile(r'\r\n|\n')
COMMISSIONER_USER = 'pi'
COMMISSIONER_PASSWORD = 'raspberry'
COMMISSIONER_PROMPT = 'pi@raspberry'
COMMISSIONER_CTL = 'sudo commissioner_ctl.py'

# Command line length cannot exceed this
TTY_COLS = 4096

CONTROL_SEQUENCE = re.compile(
    r'\x1b'  # ESC
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

TLV_TYPE_FROM_STRING = {TLV_TYPE_TO_STRING[key]: key for key in
                        TLV_TYPE_TO_STRING}

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

        config_path = '/tmp/commissioner.{}.json'.format(uuid.uuid4())
        self._write_config(config_path=config_path, config=config)

        response = self._command(
            '{} init "{}"'.format(COMMISSIONER_CTL, config_path))
        if self._command('echo $?')[0] != '0':
            raise commissioner.Error(
                'Failed to init, error:\n{}'.format('\n'.join(response)))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        try:
            self._command('{} exit'.format(COMMISSIONER_CTL))
        except Exception as e:
            logging.exception(e)

        if isinstance(self._handler, serial.Serial):
            self._write_line('logout')
        else:
            self._handler.terminate(force=True)

    @staticmethod
    def make_local_commissioner(config, simulator):
        import pexpect
        handler = pexpect.spawn("/bin/bash")
        return OTCommissioner(config, handler, simulator)

    @staticmethod
    def make_harness_commissioner(config, serial_handler):
        if not isinstance(serial_handler, serial.Serial):
            raise commissioner.Error("expect a serial handler")
        return OTCommissioner(config, serial_handler)

    def start(self, borderAgentAddr, borderAgentPort):
        self._execute_and_check('start {} {}'.format(
            borderAgentAddr,
            borderAgentPort,
        ))

    def stop(self):
        self._execute_and_check('stop')

    def isActive(self):
        response = self._execute_and_check('active')
        if response[0].startswith('true'):
            return True
        elif response[0].startswith('false'):
            return False
        else:
            raise commissioner.Error(
                'Unrecognized result "{}"'.format(response[0]))

    def getSessionId(self):
        response = self._execute_and_check('sessionid')
        try:
            return int(response[0])
        except ValueError as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_COMMISSIONER_GET(self, tlvTypes):
        types = ' '.join(map(lambda x: TLV_TYPE_TO_STRING[x], tlvTypes))
        command = 'commdataset get ' + types
        response = self._execute_and_check(command)

        try:
            result = json.loads(' '.join(response[:-1]))
            result = {TLV_TYPE_FROM_STRING[key]: result[key] for key in result}

            for key in [TLV_TYPE_STEERING_DATA,
                        TLV_TYPE_AE_STEERING_DATA,
                        TLV_TYPE_NMKP_STEERING_DATA]:
                if key in result:
                    result[key] = OTCommissioner._hex_to_bytes(result[key])

            return result
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_COMMISSIONER_SET(self, commDataset):
        for key in [TLV_TYPE_STEERING_DATA,
                    TLV_TYPE_AE_STEERING_DATA,
                    TLV_TYPE_NMKP_STEERING_DATA]:
            if key in commDataset:
                commDataset[key] = OTCommissioner._bytes_to_hex(
                    commDataset[key])

        dataset = {TLV_TYPE_TO_STRING[key]: commDataset[key] for key in
                   commDataset}
        data = json.dumps(dataset)
        self._execute_and_check("commdataset set '{}'".format(data))

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
        types = ' '.join(map(lambda x: TLV_TYPE_TO_STRING[x], tlvTypes))
        result = self._execute_and_check(
            'opdataset get active {}'.format(types))

        try:
            return OTCommissioner._active_op_dataset_from_json(
                ' '.join(result[:-1]))
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_ACTIVE_SET(self, activeOpDataset):
        self._execute_and_check("opdataset set active '{}'".format(
            OTCommissioner._active_op_dataset_to_json(activeOpDataset)))

    def MGMT_PENDING_GET(self, tlvTypes):
        types = ' '.join(map(lambda x: TLV_TYPE_TO_STRING[x], tlvTypes))
        result = self._execute_and_check(
            'opdataset get pending {}'.format(types))

        try:
            return OTCommissioner._pending_op_dataset_from_json(
                ' '.join(result[:-1]))
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_PENDING_SET(self, pendingOpDataset):
        self._execute_and_check("opdataset set pending '{}'".format(
            OTCommissioner._pending_op_dataset_to_json(pendingOpDataset)))

    def MGMT_BBR_GET(self, tlvTypes):
        types = ' '.join(map(lambda x: TLV_TYPE_TO_STRING[x], tlvTypes))
        result = self._execute_and_check('bbrdataset get {}'.format(types))

        try:
            result = json.loads(' '.join(result[:-1]))
            return {TLV_TYPE_FROM_STRING[key]: result[key] for key in result}
        except Exception as e:
            raise_(commissioner.Error, repr(e), sys.exc_info()[2])

    def MGMT_BBR_SET(self, bbrDataset):
        dataset = {TLV_TYPE_TO_STRING[key]: bbrDataset[key] for key in
                   bbrDataset}
        dataset = json.dumps(dataset)
        self._execute_and_check("bbrdataset set '{}'".format(dataset))

    def MLR(self, multicastAddrs, timeout):
        self._execute_and_check('mlr {} {}'.format(
            ' '.join(multicastAddrs),
            timeout,
        ))

    def MGMT_ANNOUNCE_BEGIN(self, channelMask, count, period, dstAddr):
        self._execute_and_check('announce {} {} {} {}'.format(
            channelMask,
            count,
            period,
            dstAddr,
        ))

    def MGMT_PANID_QUERY(self, channelMask, panId, dstAddr, timeout):
        self._execute_and_check('panid query {} {} {}'.format(
            channelMask,
            panId,
            dstAddr,
        ))
        self._sleep(timeout)
        result = self._execute_and_check('panid conflict {}'.format(panId))
        result = int(result[0])
        return False if result == 0 else True

    def MGMT_ED_SCAN(self, channelMask, count, period, scanDuration, dstAddr,
                     timeout):
        self._execute_and_check('energy scan {} {} {} {} {}'.format(
            channelMask,
            count,
            period,
            scanDuration,
            dstAddr,
        ))

        self._sleep(timeout)
        result = self._execute_and_check('energy report {}'.format(dstAddr))
        if result[0] == 'null':
            raise commissioner.Error(
                'No energy report found for {}'.format(dstAddr))

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
        self._execute_and_check('reenroll {}'.format(dstAddr))

    def MGMT_DOMAIN_RESET(self, dstAddr):
        self._execute_and_check('domainreset {}'.format(dstAddr))

    def MGMT_NET_MIGRATE(self, dstAddr, designatedNetwork):
        self._execute_and_check('migrate {} {}'.format(
            dstAddr,
            designatedNetwork,
        ))

    def requestCOM_TOK(self, registrarAddr, registrarPort):
        self._execute_and_check('token request {} {}'.format(
            registrarAddr,
            registrarPort,
        ))

    def setCOM_TOK(self, signedCOM_TOK, verifyCert):
        path_token = '/tmp/commissioner.token.{}'.format(uuid.uuid4())
        step = 40
        for i in range(0, len(signedCOM_TOK), step):
            data = self._bytes_to_hex(signedCOM_TOK[i: i + step])
            self._command('echo {} >> "{}"'.format(data, path_token))

        path_cert = '/tmp/commissioner.token_cert.{}'.format(uuid.uuid4())
        self._command('echo "{}" | base64 -d - > "{}"'.format(
            base64.b64encode(verifyCert).decode(),
            path_cert,
        ))

        self._execute_and_check('token set {} {}'.format(
            path_token,
            path_cert,
        ))

    def getCOM_TOK(self):
        result = self._execute_and_check('token print')
        return self._hex_to_bytes(result[0])

    def _execute_and_check(self, command):
        # Escape quotes for bash
        command = command.replace('"', r'"\""')

        return OTCommissioner._check_response(self._command(
            '{} execute "{}"'.format(COMMISSIONER_CTL, command)))

    def _expect(self, expected, timeout=10):
        logging.info('Expecting [{}]'.format(expected))

        while timeout > 0:
            line = self._read_line()
            logging.info('Got line [{}]'.format(line))

            if line == expected:
                logging.info('Expected [{}]'.format(expected))
                return

            self._sleep(0.1)

        raise commissioner.Error(
            'Failed to find expected string [{}]'.format(expected))

    def _sleep(self, seconds):
        if isinstance(self._handler, serial.Serial):
            time.sleep(seconds)
        else:
            while seconds > 0:
                self._simulator.go(0)
                seconds -= 0.1

    def _read_line(self):
        logging.debug('Reading line')
        if len(self._lines) > 1:
            line = self._lines.pop(0)
            line = CONTROL_SEQUENCE.sub('', line)
            return line

        tail = ''
        if len(self._lines):
            tail = self._lines.pop()

        try:
            tail += self._read()
        except socket.error:
            logging.debug('No new data')

        self._lines += NEW_LINE.split(tail)

    def _read(self, size=512):
        if isinstance(self._handler, serial.Serial):
            return self._handler.read(size).decode()
        else:
            try:
                return self._handler.read_nonblocking(size, timeout=0)
            except pexpect.TIMEOUT as e:
                return ""

    def _write_line(self, line):
        logging.debug('Writing line')
        self._lines = []
        try:
            self._read()
        except socket.error:
            logging.debug('Nothing cleared')

        logging.info('Writing [{}]'.format(line))
        self._write(line + '\r\n')
        self._lines = []
        self._sleep(0.1)

    def _write(self, data):
        if isinstance(self._handler, serial.Serial):
            self._handler.write(data.encode())
        else:
            self._handler.send(data)

    def _command(self, cmd, timeout=10):
        if len(cmd) + len(COMMISSIONER_PROMPT) >= TTY_COLS:
            raise commissioner.Error('Command too long: "{}"'.format(cmd))

        self._write_line(cmd)
        self._expect(cmd, timeout)
        response = []

        while timeout > 0:
            line = self._read_line()
            logging.info('Read line: [{}]'.format(line))
            if line:
                if re.match(COMMISSIONER_PROMPT, line):
                    break
                response.append(line)

            self._sleep(0.1)
        if timeout <= 0:
            raise Exception(
                'Failed to find end of response'.format(self._port))

        logging.info('Send command[{}] done!'.format(cmd))
        return response

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
                path = '/tmp/commissioner.private_key.{}'.format(uuid.uuid4())
                self._send_file(local_path=config.privateKey, remote_path=path)
                data['PrivateKeyFile'] = path
            if config.cert:
                path = '/tmp/commissioner.cert.{}'.format(uuid.uuid4())
                self._send_file(local_path=config.cert, remote_path=path)
                data['CertificateFile'] = path
            if config.trustAnchor:
                path = '/tmp/commissioner.trush_anchor.{}'.format(uuid.uuid4())
                self._send_file(local_path=config.trustAnchor, remote_path=path)
                data['TrustAnchorFile'] = path

        self._command("echo '{}' >> '{}'".format(json.dumps(data), config_path))

    def _send_file(self, local_path, remote_path):
        with open(local_path, 'rb') as f:
            b64 = base64.b64encode(f.read()).decode()
        self._command('echo "{}" | base64 -d - > "{}"'.format(b64,
                                                              remote_path))

    @staticmethod
    def _check_response(response):
        if response[-1] != '[done]':
            raise commissioner.Error(
                'Error message:\n{}'.format('\n'.join(response)))
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
            result.append(OTCommissioner.ChannelMaskEntry(
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
