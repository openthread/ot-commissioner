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
Commissioner daemon process
"""

from __future__ import print_function

import os
import multiprocessing.connection
import sys
import traceback
import argparse
import logging
import re
import pexpect
import signal

SOCKET_DIR = '/tmp/ot-commissioner'
SOCKET_PATH = os.path.join(SOCKET_DIR, 'commissionerd.sock')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--cli', dest='cli', required=True)
    parser.add_argument('-t', '--timeout', dest='timeout', type=int, default=60)
    args = parser.parse_args()

    with Application(args.cli, args.timeout) as app:
        app.run()


class Application(object):

    def __init__(self, cli_path, timeout):
        self._cli_path = cli_path
        self._timeout = timeout

        try:
            if os.path.exists(SOCKET_PATH):
                os.remove(SOCKET_PATH)
            if not os.path.exists(SOCKET_DIR):
                os.makedirs(SOCKET_DIR)
        except OSError:
            pass
        self._listener = multiprocessing.connection.Listener(SOCKET_PATH,
                                                             family='AF_UNIX')

        self._reset_states()

    def run(self):
        while True:
            connection = self._listener.accept()

            class Closer:

                def __enter__(self):
                    return self

                def __exit__(self, exc_type, exc_val, exc_tb):
                    connection.close()

            with Closer():
                while True:
                    try:
                        message = connection.recv()
                    except EOFError:
                        break

                    try:
                        result = self._handle_message(message)
                    except self._AppException:
                        result = {
                            'status':
                                'failure',
                            'traceback':
                                traceback.format_exception(*sys.exc_info()),
                        }

                    print(result)
                    connection.send(result)

    def _handle_message(self, message):
        result = None
        if message['type'] == 'control':
            if message['command'] == 'init':
                try:
                    result = {
                        'status': 'success',
                        'result': self._init(message['config']),
                    }
                except pexpect.exceptions.EOF:
                    # Process didn't start successfully
                    self._discard_process_with_error()
            elif message['command'] == 'exit':
                result = {
                    'status': 'success',
                    'result': self._exit(),
                }
        elif message['type'] == 'execute':
            command = message['command']
            if re.match(r'^\s*exit(\s+.*)?', command):
                result = {
                    'status': 'success',
                    'result': self._exit(),
                }
            else:
                result = {
                    'status': 'success',
                    'result': self._execute_command(message['command'])
                }

        assert result is not None
        return result

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._listener.close()
        if self._process:
            try:
                self._exit()
            except Exception as e:
                logging.exception(e)
                try:
                    self._process.terminate(force=True)
                except Exception as e:
                    logging.exception(e)

    def _init(self, config_path):
        if self._process:
            self._exit()

        self._process = pexpect.spawn(self._cli_path,
                                      args=[config_path],
                                      timeout=self._timeout,
                                      echo=False)
        self._process.expect(r'> $')
        return self._process.before.decode()

    def _execute_command(self, command, wait_for_result=True):
        if not self._process:
            raise self._AppException('CLI not started yet')

        print('Executing: "{}"'.format(command))
        self._process.sendline(command)
        if wait_for_result:
            try:
                self._process.expect(r'> $')
                return self._process.before.decode()
            except pexpect.exceptions.TIMEOUT:
                # Abort the command
                self._process.kill(sig=signal.SIGINT)
                try:
                    self._process.expect(r'> $', timeout=1)
                    return 'Timed out executing "{}"\n{}'.format(
                        command, self._process.before.decode())
                except pexpect.exceptions.TIMEOUT as e:
                    raise self._AppException(e)

    def _exit(self):
        if not self._process:
            raise self._AppException('CLI not started yet')

        self._execute_command('exit', wait_for_result=False)
        try:
            self._process.expect_exact(pexpect.EOF)
        except pexpect.exceptions.TIMEOUT as e:
            self._process.terminate(force=True)
            self._reset_states()
            raise self._AppException(e)
        result = self._process.before.decode()
        self._reset_states()
        return result

    def _discard_process_with_error(self):
        message = self._process.before.decode()
        self._reset_states()
        raise self._AppException(
            'CLI process exited with error:\n{}'.format(message))

    def _reset_states(self):
        self._process = None

    class _AppException(Exception):
        # Should be passed to controller and displayed to user
        pass


if __name__ == '__main__':
    main()
