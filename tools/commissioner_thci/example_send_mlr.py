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
Example usage of sending MLR.req with OT-commissioner.

Topology
The topology is simple that only a leader/PBBR is started
and connected as "/dev/ttyUSB0" (hosted by a raspberry pi).
The commissioner server resides on the same raspberry pi
and connect to the Thread network through local border agent.

Send MLR.req
The OTCommissioner instance acts as an client of the commissioner
server and requests the commissioner to send a MLR.req message
by calling `MLR(multicastAddrs, timeout)`. The whole process failed
if there are exceptions throwed, otherwise, we succeed sending
MLR.req (MLR.rsp verified as status=ST_MLR_SUCCESS).

"""

from commissioner import Configuration
from commissioner_impl import OTCommissioner

COMMISSIONER_SERVER = "/dev/ttyUSB0"
MA1 = "ff04::1234:777a:1"
MA2 = "ff05::1234:777a:2"

BORDER_AGENT_ADDR = "::"
BORDER_AGENT_PORT = 49191


def test_mlr():

    ## Setup configuration
    config = Configuration("ot-commissioner")
    config.isCcmMode = False

    config.pskc = bytearray.fromhex('3aa55f91ca47d1e4e71a08cb35e91591')

    ## Connect to commissioner server on at /dev/ttyUSB0
    with OTCommissioner(config, port=COMMISSIONER_SERVER) as comm:
        assert not comm.isActive()

        ## Start petitioning through local border agent
        comm.start(BORDER_AGENT_ADDR, BORDER_AGENT_PORT)
        assert comm.isActive()

        print(("commissioner connected, session ID = {}".format(
            comm.getSessionId())))

        ## Send MLR.req
        comm.MLR([MA1, MA2], 60)

        print("send MLR.req to PBBR, [done]")


if __name__ == "__main__":
    test_mlr()
