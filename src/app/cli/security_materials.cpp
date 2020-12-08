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

#include "app/cli/security_materials.hpp"
#include <string>
#include "app/file_util.hpp"
#include "common/error_macros.hpp"
#include "common/utils.hpp"

namespace ot {

namespace commissioner {

namespace sm {

using SMPair = std::pair<std::string, ByteArray *>;

static Error GetNetworkSM_impl(const std::string  aNwkFolder, // a folder to start from
                               const std::string  aAlias,     // network id or network name
                               bool               aCCM,
                               SecurityMaterials &aSM);

static class SMRoot
{
    std::string mRootPath;

public:
    void        Set(const std::string aRoot) { mRootPath = aRoot; }
    std::string Get() const { return mRootPath; }
} smRoot;

Error Init(const Config &aDefaultConfig)
{
    std::string root = aDefaultConfig.mThreadSMRoot;

    if (root.empty())
    {
        root = getenv("THREAD_SM_ROOT");
        if (root.empty())
        {
            return ERROR_INVALID_ARGS("ThreadSMRoot is not available");
        }
        smRoot.Set(root);
    }
    else
    {
        smRoot.Set(root);
    }
    return ERROR_NONE;
}

Error GetDomainSM(const std::string aDid, SecurityMaterials &aSM)
{
    Error               error;
    std::string         domPath;
    std::vector<SMPair> smElements{
        {"cert.pem", &aSM.mCertificate}, {"priv.pem", &aSM.mPrivateKey}, {"ca.pem", &aSM.mTrustAnchor}};

    VerifyOrExit(!smRoot.Get().empty(),
                 error = ERROR_INVALID_ARGS("ThreadSMRoot is not known. Unable to access Security Materials."));

    domPath = smRoot.Get().append("/dom/").append(aDid).append("/");
    SuccessOrExit(error = PathExists(domPath));
    for (auto element : smElements)
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
            // log error
        }
        *element.second = bytes;
    }
exit:
    return error;
}

Error GetDefaultDomainSM(const std::string aAlias, bool aCCM, SecurityMaterials &aSM)
{
    return GetNetworkSM_impl("/dom/DefaultDomain/", aAlias, aCCM, aSM);
}

Error GetNetworkSM(const std::string aAlias, bool aCCM, SecurityMaterials &aSM)
{
    return GetNetworkSM_impl("/nwk/", aAlias, aCCM, aSM);
}

static Error GetNetworkSM_impl(const std::string  aNwkFolder,
                               const std::string  aAlias,
                               bool               aCCM,
                               SecurityMaterials &aSM)
{
    Error       error;
    std::string nwkPath;

    VerifyOrExit(!smRoot.Get().empty(),
                 error = ERROR_INVALID_ARGS("ThreadSMRoot is not known. Unable to access Security Materials."));
    nwkPath = smRoot.Get().append(aNwkFolder).append(aAlias).append("/");
    SuccessOrExit(error = PathExists(nwkPath));
    if (aCCM)
    {
        std::vector<SMPair> smElements{
            {"cert.pem", &aSM.mCertificate}, {"priv.pem", &aSM.mPrivateKey}, {"ca.pem", &aSM.mTrustAnchor}};

        for (auto element : smElements)
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
                // log error
            }
            *element.second = bytes;
        }
    }
    else
    {
        std::string data;
        std::string path = nwkPath + "pskc.txt";

        SuccessOrExit(error = PathExists(path));
        SuccessOrExit(error = ReadFile(data, path));
        aSM.mPSKc = ByteArray{data.begin(), data.end()};
    }
exit:
    return error;
}

} // namespace sm

} // namespace commissioner

} // namespace ot
