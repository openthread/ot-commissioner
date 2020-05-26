/*
 *    Copyright (c) 2019, The OpenThread Commissioner Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file defines test cases for COSE.
 */

#include "library/cose.hpp"

#include <catch2/catch.hpp>

#include <mbedtls/base64.h>

#include "library/token_manager.hpp"

namespace ot {

namespace commissioner {

namespace cose {

static const char kCertificate[] = "-----BEGIN CERTIFICATE-----\r\n"
                                   "MIICATCCAaegAwIBAgIIJU8KN/Bcw4cwCgYIKoZIzj0EAwIwGDEWMBQGA1UEAwwN\r\n"
                                   "VGhyZWFkR3JvdXBDQTAeFw0xOTA2MTkyMTM2MTFaFw0yNDA2MTcyMTM2MTFaMBox\r\n"
                                   "GDAWBgNVBAMMD1RocmVhZFJlZ2lzdHJhcjBZMBMGByqGSM49AgEGCCqGSM49AwEH\r\n"
                                   "A0IABCAwhVvoRpELPssVyvhXLT61Zb3GVKFe+vbt66qLnhYIxckQyTogho/IUE03\r\n"
                                   "Dxsm+pdZ9nmDu3iGPtqay+pRJPajgdgwgdUwDwYDVR0TBAgwBgEB/wIBAjALBgNV\r\n"
                                   "HQ8EBAMCBeAwbAYDVR0RBGUwY6RhMF8xCzAJBgNVBAYTAlVTMRUwEwYDVQQKDAxU\r\n"
                                   "aHJlYWQgR3JvdXAxFzAVBgNVBAMMDlRlc3QgUmVnaXN0cmFyMSAwHgYJKoZIhvcN\r\n"
                                   "AQkBFhFtYXJ0aW5Ac3Rva29lLm5ldDBHBgNVHSMEQDA+gBSS6nZAQEqPq08nC/O8\r\n"
                                   "N52GzXKA+KEcpBowGDEWMBQGA1UEAwwNVGhyZWFkR3JvdXBDQYIIc5C+m8ijatIw\r\n"
                                   "CgYIKoZIzj0EAwIDSAAwRQIgbI7Vrg348jGCENRtT3GbV5FaEqeBaVTeHlkCA99z\r\n"
                                   "RVACIQDGDdZSWXAR+AlfmrDecYnmp5Vgz8eTyjm9ZziIFXPUwA==\r\n"
                                   "-----END CERTIFICATE-----\r\n";

static const char kPrivateKey[] = "-----BEGIN PRIVATE KEY-----\r\n"
                                  "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgYJ/MP0dWA9BkYd4W\r\n"
                                  "s6oRY62hDddaEmrAVm5dtAXE/UGhRANCAAQgMIVb6EaRCz7LFcr4Vy0+tWW9xlSh\r\n"
                                  "Xvr27euqi54WCMXJEMk6IIaPyFBNNw8bJvqXWfZ5g7t4hj7amsvqUST2\r\n"
                                  "-----END PRIVATE KEY-----\r\n";

TEST_CASE("cose-sign-and-verify", "[cose]")
{
    ByteArray content{1, 2, 3, 4, 5, 6};
    ByteArray externalData{6, 5, 4, 3, 2, 1};

    mbedtls_pk_context publicKey;
    mbedtls_pk_context privateKey;

    mbedtls_pk_init(&publicKey);
    mbedtls_pk_init(&privateKey);

    REQUIRE(
        TokenManager::ParsePublicKey(publicKey, ByteArray{kCertificate, kCertificate + sizeof(kCertificate)}).IsNone());
    REQUIRE(
        TokenManager::ParsePrivateKey(privateKey, ByteArray{kPrivateKey, kPrivateKey + sizeof(kPrivateKey)}).IsNone());

    SECTION("cose sign without external data")
    {
        ByteArray    signature;
        Sign1Message msg;

        REQUIRE(msg.Init(kInitFlagsNone).IsNone());
        REQUIRE(msg.AddAttribute(kHeaderAlgorithm, kAlgEcdsaWithSha256, kProtectOnly).IsNone());
        REQUIRE(msg.SetContent(content).IsNone());
        REQUIRE(msg.Sign(privateKey).IsNone());

        REQUIRE(msg.Serialize(signature).IsNone());
        msg.Free();

        REQUIRE(Sign1Message::Deserialize(msg, signature).IsNone());
        REQUIRE(msg.Validate(publicKey).IsNone());
    }

    SECTION("cose sign with external data")
    {
        ByteArray    signature;
        Sign1Message msg;

        REQUIRE(msg.Init(kInitFlagsNone).IsNone());
        REQUIRE(msg.AddAttribute(kHeaderAlgorithm, kAlgEcdsaWithSha256, kProtectOnly).IsNone());
        REQUIRE(msg.SetContent({}).IsNone());
        REQUIRE(msg.SetExternalData(externalData).IsNone());
        REQUIRE(msg.Sign(privateKey).IsNone());

        REQUIRE(msg.Serialize(signature).IsNone());
        msg.Free();

        REQUIRE(Sign1Message::Deserialize(msg, signature).IsNone());
        REQUIRE(msg.SetExternalData(externalData).IsNone());
        REQUIRE(msg.Validate(publicKey).IsNone());
    }

    SECTION("cose key construction")
    {
        ByteArray keyId = {};
        ByteArray encodedCoseKey;
        CborMap   coseKey;

        REQUIRE(MakeCoseKey(encodedCoseKey, publicKey, keyId).IsNone());
        REQUIRE(CborMap::Deserialize(coseKey, &encodedCoseKey[0], encodedCoseKey.size()).IsNone());

        uint8_t buf[1024];
        size_t  bufLength = 0;
        REQUIRE(coseKey.Serialize(buf, bufLength, sizeof(buf)).IsNone());

        int keyType = 0;
        REQUIRE(coseKey.Get(cose::kKeyType, keyType).IsNone());
        REQUIRE(keyType == cose::kKeyTypeEC2);

        int ec2Curve = 0;
        REQUIRE(coseKey.Get(cose::kKeyEC2Curve, ec2Curve).IsNone());
        REQUIRE(ec2Curve == cose::kKeyEC2CurveP256);

        const uint8_t *x;
        size_t         xlen;
        REQUIRE(coseKey.Get(cose::kKeyEC2X, x, xlen).IsNone());
        INFO(utils::Hex(ByteArray{x, x + xlen}));
        INFO(xlen);

        uint8_t dump[1024];
        size_t  len;
        REQUIRE(mbedtls_base64_encode(dump, sizeof(dump), &len, x, xlen) == 0);

        INFO(std::string(dump, dump + len));

        const uint8_t *y;
        size_t         ylen;
        REQUIRE(coseKey.Get(cose::kKeyEC2Y, y, ylen).IsNone());
        INFO(utils::Hex(ByteArray{y, y + ylen}));
        INFO(ylen);

        REQUIRE(mbedtls_base64_encode(dump, sizeof(dump), &len, y, ylen) == 0);

        INFO(std::string(dump, dump + len));

        INFO(utils::Hex(ByteArray{buf, buf + bufLength}));

        coseKey.Free();
    }
}

} // namespace cose

} // namespace commissioner

} // namespace ot
