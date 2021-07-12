/*
 *    Copyright (c) 2020, The OpenThread Commissioner Authors.
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
 *   The file implements security materials storage.
 */

#include <string>

#include "app/cli/console.hpp"
#include "app/cli/security_materials.hpp"
#include "app/file_util.hpp"
#include "common/error_macros.hpp"
#include "common/logging.hpp"
#include "common/utils.hpp"

#define SM_ERROR_MESSAGE_NO_ROOT_AVAILABLE "ThreadSMRoot value is not available"
#define SM_ERROR_MESSAGE_PEM_READ_FAILED "Failed to read security data from file {}"

namespace ot {

namespace commissioner {

namespace security_material {

using SMPair = std::pair<std::string, ByteArray *>;

static Error GetNetworkSMImpl(const std::string  aNwkFolder, // a folder to start from
                              const std::string  aAlias,     // network id or network name
                              bool               aCCM,
                              SecurityMaterials &aSM);

static class SMRoot
{
    std::string mRootPath;

public:
    void Set(const std::string aRoot)
    {
        mRootPath = aRoot;
        if (mRootPath.length() > 0 && mRootPath.back() != '/')
        {
            mRootPath += "/";
        }
    }
    std::string Get() const { return mRootPath; }
} smRoot;

Error Init(const Config &aDefaultConfig)
{
    std::string root = aDefaultConfig.mThreadSMRoot;

    if (root.empty())
    {
        root = SafeStr(getenv("THREAD_SM_ROOT"));
        if (root.empty())
        {
            if (gVerbose)
            {
                Console::Write(SM_ERROR_MESSAGE_NO_ROOT_AVAILABLE, Console::Color::kYellow);
            }
            else
            {
                LOG_DEBUG(LOG_REGION_SECURITY_MATERIALS, SM_ERROR_MESSAGE_NO_ROOT_AVAILABLE);
            }
        }
    }
    smRoot.Set(root);
    return ERROR_NONE;
}

Error GetDomainSM(const std::string aDid, SecurityMaterials &aSM)
{
    Error               error;
    std::string         domPath;
    std::vector<SMPair> smElements{
        {"cert.pem", &aSM.mCertificate}, {"priv.pem", &aSM.mPrivateKey}, {"ca.pem", &aSM.mTrustAnchor}};

    // If SM root is unset, do nothing
    VerifyOrExit(!smRoot.Get().empty());

    domPath = smRoot.Get().append("dom/").append(aDid).append("/");
    SuccessOrExit(error = PathExists(domPath));
    for (const auto &element : smElements)
    {
        std::string path = domPath + element.first;
        if (ERROR_NONE != PathExists(path))
        {
            continue;
        }
        ByteArray bytes;
        error = ReadPemFile(bytes, path);
        if (error != ERROR_NONE)
        {
            if (gVerbose)
            {
                std::string out = fmt::format(FMT_STRING(SM_ERROR_MESSAGE_PEM_READ_FAILED), path);
                Console::Write(out, Console::Color::kRed);
            }
            else
            {
                LOG_ERROR(LOG_REGION_SECURITY_MATERIALS, SM_ERROR_MESSAGE_PEM_READ_FAILED, path);
            }
        }
        else
        {
            *element.second = bytes;
        }
    }

    do
    {
        std::string path = domPath + "tok.cbor";
        if (ERROR_NONE != PathExists(path))
        {
            break;
        }
        ByteArray bytes;
        Error     tokenError = ReadHexStringFile(bytes, path);
        if (ERROR_NONE == tokenError)
        {
            aSM.mCommissionerToken = bytes;
        }
    } while (false);
exit:
    return error;
}

Error GetDefaultDomainSM(const std::string aAlias, bool aCCM, SecurityMaterials &aSM)
{
    return GetNetworkSMImpl("dom/DefaultDomain/", aAlias, aCCM, aSM);
}

Error GetNetworkSM(const std::string aAlias, bool aCCM, SecurityMaterials &aSM)
{
    return GetNetworkSMImpl("nwk/", aAlias, aCCM, aSM);
}

static Error GetNetworkSMImpl(const std::string aNwkFolder, const std::string aAlias, bool aCCM, SecurityMaterials &aSM)
{
    Error       error;
    std::string nwkPath;

    // If SM root is unset, do nothing
    VerifyOrExit(!smRoot.Get().empty());

    nwkPath = smRoot.Get().append(aNwkFolder).append(aAlias).append("/");
    SuccessOrExit(error = PathExists(nwkPath));
    if (aCCM)
    {
        std::vector<SMPair> smElements{
            {"cert.pem", &aSM.mCertificate}, {"priv.pem", &aSM.mPrivateKey}, {"ca.pem", &aSM.mTrustAnchor}};

        for (const auto &element : smElements)
        {
            std::string path = nwkPath + element.first;
            if (ERROR_NONE != PathExists(path))
            {
                continue;
            }
            ByteArray bytes;
            error = ReadPemFile(bytes, path);
            if (error != ERROR_NONE)
            {
                if (gVerbose)
                {
                    std::string out = fmt::format(FMT_STRING(SM_ERROR_MESSAGE_PEM_READ_FAILED), path);
                    Console::Write(out, Console::Color::kRed);
                }
                else
                {
                    LOG_ERROR(LOG_REGION_SECURITY_MATERIALS, SM_ERROR_MESSAGE_PEM_READ_FAILED, path);
                }
            }
            else
            {
                *element.second = bytes;
            }
        }

        do
        {
            std::string path = nwkPath + "tok.cbor";
            if (ERROR_NONE != PathExists(path))
            {
                break;
            }
            ByteArray bytes;
            Error     tokenError = ReadHexStringFile(bytes, path);
            if (ERROR_NONE == tokenError)
            {
                aSM.mCommissionerToken = bytes;
            }
        } while (false);
    }
    else
    {
        std::string path = nwkPath + "pskc.txt";

        SuccessOrExit(error = PathExists(path));
        SuccessOrExit(error = ReadHexStringFile(aSM.mPSKc, path));
    }
exit:
    return error;
}

bool SecurityMaterials::IsEmpty(bool isCCM)
{
    return (isCCM ? (mCertificate.size() == 0 || mPrivateKey.size() == 0 || mTrustAnchor.size() == 0)
                  : (mPSKc.size() == 0));
}

} // namespace security_material

} // namespace commissioner

} // namespace ot
