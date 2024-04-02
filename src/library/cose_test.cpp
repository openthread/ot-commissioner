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

#if OT_COMM_CONFIG_CCM_ENABLE

#include "library/cose.hpp"

#include <gtest/gtest.h>

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

static Error ParsePublicKey(mbedtls_pk_context &aPublicKey, const ByteArray &aCert)
{
    TokenManager tokenManager{event_base_new()};

    return tokenManager.ParsePublicKey(aPublicKey, aCert);
}

static Error ParsePrivateKey(mbedtls_pk_context &aPrivateKey, const ByteArray &aPrivateKeyRaw)
{
    TokenManager tokenManager{event_base_new()};

    return tokenManager.ParsePrivateKey(aPrivateKey, aPrivateKeyRaw);
}

TEST(CoseTest, CoseSignAndVerify_SignWithoutExternalData)
{
    ByteArray content{1, 2, 3, 4, 5, 6};
    ByteArray externalData{6, 5, 4, 3, 2, 1};

    mbedtls_pk_context publicKey;
    mbedtls_pk_context privateKey;

    mbedtls_pk_init(&publicKey);
    mbedtls_pk_init(&privateKey);

    EXPECT_EQ(ParsePublicKey(publicKey, ByteArray{kCertificate, kCertificate + sizeof(kCertificate)}),
              ErrorCode::kNone);
    EXPECT_EQ(ParsePrivateKey(privateKey, ByteArray{kPrivateKey, kPrivateKey + sizeof(kPrivateKey)}), ErrorCode::kNone);

    ByteArray    signature;
    Sign1Message msg;

    EXPECT_EQ(msg.Init(kInitFlagsNone), ErrorCode::kNone);
    EXPECT_EQ(msg.AddAttribute(kHeaderAlgorithm, kAlgEcdsaWithSha256, kProtectOnly), ErrorCode::kNone);
    EXPECT_EQ(msg.SetContent(content), ErrorCode::kNone);
    EXPECT_EQ(msg.Sign(privateKey), ErrorCode::kNone);

    EXPECT_EQ(msg.Serialize(signature), ErrorCode::kNone);
    msg.Free();

    EXPECT_EQ(Sign1Message::Deserialize(msg, signature), ErrorCode::kNone);
    EXPECT_EQ(msg.Validate(publicKey), ErrorCode::kNone);
}

TEST(CoseTest, CoseSignAndVerify_SignWithExternalData)
{
    ByteArray content{1, 2, 3, 4, 5, 6};
    ByteArray externalData{6, 5, 4, 3, 2, 1};

    mbedtls_pk_context publicKey;
    mbedtls_pk_context privateKey;

    mbedtls_pk_init(&publicKey);
    mbedtls_pk_init(&privateKey);

    EXPECT_EQ(ParsePublicKey(publicKey, ByteArray{kCertificate, kCertificate + sizeof(kCertificate)}),
              ErrorCode::kNone);
    EXPECT_EQ(ParsePrivateKey(privateKey, ByteArray{kPrivateKey, kPrivateKey + sizeof(kPrivateKey)}), ErrorCode::kNone);

    ByteArray    signature;
    Sign1Message msg;

    EXPECT_EQ(msg.Init(kInitFlagsNone), ErrorCode::kNone);
    EXPECT_EQ(msg.AddAttribute(kHeaderAlgorithm, kAlgEcdsaWithSha256, kProtectOnly), ErrorCode::kNone);
    EXPECT_EQ(msg.SetContent({}), ErrorCode::kNone);
    EXPECT_EQ(msg.SetExternalData(externalData), ErrorCode::kNone);
    EXPECT_EQ(msg.Sign(privateKey), ErrorCode::kNone);

    EXPECT_EQ(msg.Serialize(signature), ErrorCode::kNone);
    msg.Free();

    EXPECT_EQ(Sign1Message::Deserialize(msg, signature), ErrorCode::kNone);
    EXPECT_EQ(msg.SetExternalData(externalData), ErrorCode::kNone);
    EXPECT_EQ(msg.Validate(publicKey), ErrorCode::kNone);
}

TEST(CoseTest, CoseSignAndVerify_KeyConstruction)
{
    ByteArray content{1, 2, 3, 4, 5, 6};
    ByteArray externalData{6, 5, 4, 3, 2, 1};

    mbedtls_pk_context publicKey;
    mbedtls_pk_context privateKey;

    mbedtls_pk_init(&publicKey);
    mbedtls_pk_init(&privateKey);

    EXPECT_EQ(ParsePublicKey(publicKey, ByteArray{kCertificate, kCertificate + sizeof(kCertificate)}),
              ErrorCode::kNone);
    EXPECT_EQ(ParsePrivateKey(privateKey, ByteArray{kPrivateKey, kPrivateKey + sizeof(kPrivateKey)}), ErrorCode::kNone);

    ByteArray keyId = {};
    ByteArray encodedCoseKey;
    CborMap   coseKey;

    EXPECT_EQ(MakeCoseKey(encodedCoseKey, publicKey, keyId), ErrorCode::kNone);
    EXPECT_EQ(CborMap::Deserialize(coseKey, &encodedCoseKey[0], encodedCoseKey.size()), ErrorCode::kNone);

    uint8_t buf[1024];
    size_t  bufLength = 0;
    EXPECT_EQ(coseKey.Serialize(buf, bufLength, sizeof(buf)), ErrorCode::kNone);

    int keyType = 0;
    EXPECT_EQ(coseKey.Get(cose::kKeyType, keyType), ErrorCode::kNone);
    EXPECT_EQ(keyType, cose::kKeyTypeEC2);

    int ec2Curve = 0;
    EXPECT_EQ(coseKey.Get(cose::kKeyEC2Curve, ec2Curve), ErrorCode::kNone);
    EXPECT_EQ(ec2Curve, cose::kKeyEC2CurveP256);

    const uint8_t *x;
    size_t         xlen;
    EXPECT_EQ(coseKey.Get(cose::kKeyEC2X, x, xlen), ErrorCode::kNone);

    const uint8_t *y;
    size_t         ylen;
    EXPECT_EQ(coseKey.Get(cose::kKeyEC2Y, y, ylen), ErrorCode::kNone);

    coseKey.Free();
}

} // namespace cose

} // namespace commissioner

} // namespace ot

#endif // OT_COMM_CONFIG_CCM_ENABLE
