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
Commissioner daemon controller
"""

from __future__ import print_function

import os
import argparse
import multiprocessing.connection
import sys

SOCKET_PATH = '/tmp/ot-commissioner/commissionerd.sock'


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(help='Choose an action', dest='action')

    init_parser = subparsers.add_parser('init')
    init_parser.add_argument('config', help='Path to config file')

    subparsers.add_parser('exit')

    execute_parser = subparsers.add_parser('execute')
    execute_parser.add_argument('command', help='Command to be executed')

    args = parser.parse_args()

    if args.action == 'init':
        message = {
            'type': 'control',
            'command': 'init',
            'config': os.path.realpath(args.config),
        }
    elif args.action == 'exit':
        message = {
            'type': 'control',
            'command': 'exit',
        }
    elif args.action == 'execute':
        message = {
            'type': 'execute',
            'command': args.command,
        }
    else:
        assert False

    client = multiprocessing.connection.Client(SOCKET_PATH, family='AF_UNIX')

    class Closer:

        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            client.close()

    with Closer():
        client.send(message)
        result = client.recv()
        if result['status'] == 'failure':
            print('Error executing command')
            for traceback in result['traceback']:
                print(traceback.replace('\r', ''), file=sys.stderr)
            sys.exit(1)
        elif result['status'] == 'success':
            print(result['result'].replace('\r', ''))


if __name__ == '__main__':
    main()
