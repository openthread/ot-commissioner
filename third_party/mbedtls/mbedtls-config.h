/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OT_COMM_MBEDTLS_CONFIG_H
#define OT_COMM_MBEDTLS_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#define MBEDTLS_DEBUG_C
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO snprintf

#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECJPAKE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED
#define MBEDTLS_MD_C
#define MBEDTLS_NET_C
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_DTLS_ANTI_REPLAY
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_DTLS
#define MBEDTLS_SSL_TLS_C

#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_SRV_C

#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

#define MBEDTLS_BASE64_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE

#define MBEDTLS_BASE64_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_WRITE_C

#define MBEDTLS_MPI_WINDOW_SIZE            1 /**< Maximum windows size used. */
#define MBEDTLS_MPI_MAX_SIZE              32 /**< Maximum number of bytes for usable MPIs. */
#define MBEDTLS_ECP_MAX_BITS             256 /**< Maximum bit size of groups */
#define MBEDTLS_ECP_WINDOW_SIZE            2 /**< Maximum window size used */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM      0 /**< Enable fixed-point speed-up */
#define MBEDTLS_ENTROPY_MAX_SOURCES        1 /**< Maximum number of sources supported */

#define MBEDTLS_SSL_MAX_CONTENT_LEN      1024 /**< Maxium fragment length in bytes */

#define MBEDTLS_SSL_IN_CONTENT_LEN       MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN      MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_CIPHERSUITES         MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

#include "mbedtls/version.h"

#if (MBEDTLS_VERSION_NUMBER >= 0x03050000)
    // This is from build_info.h of 3.X mbedtls to workaround building issues
#if defined(MBEDTLS_PSA_CRYPTO_CONFIG) /* PSA_WANT_xxx influences MBEDTLS_xxx */ || \
    defined(MBEDTLS_PSA_CRYPTO_C) /* MBEDTLS_xxx influences PSA_WANT_xxx */
    #include "mbedtls/config_psa.h"
#endif

    // Include in the legacy config name adjustment file for mbedtls >= 3.5.0
    #include "mbedtls/config_adjust_legacy_crypto.h"
#endif

#include "mbedtls/check_config.h"

#endif /* OT_COMM_MBEDTLS_CONFIG_H */
