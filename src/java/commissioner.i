/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   This file defines the SWIG interface of the commissioner.
 *
 * See http://www.swig.org for more information about SWIG.
 *
 */

%module(directors="1") commissionerModule

%{
#include <commissioner/defines.hpp>
#include <commissioner/error.hpp>
#include <commissioner/network_data.hpp>
#include <commissioner/commissioner.hpp>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <std_vector.i>
%include <stl.i>
%include <typemaps.i>

// This is intentional.
//
// We know that Java has no unsigned integers and SWIG maps
// Java short to C++ uint8_t (it is done in `stdint.i`). But
// it is natural for Java to represent a byte sequence in byte[]
// or AbstractList<byte> rather than a AbstractList<short>.
//
// %include <stdint.i>

// This does the trick that we direct SWIG use the same typemap of
// `signed char` for `uint8_t` in only context of `std::vector<uint8_t>`.
%apply signed char { uint8_t };
%apply const signed char & { const uint8_t & };
%template(ByteArray) std::vector<uint8_t>;
// Override the typemap of `uint8_t`.
%apply unsigned char { uint8_t };
%apply const unsigned char & { const uint8_t & };
%apply unsigned char & OUTPUT { uint8_t &aStatus };

%apply unsigned short { uint16_t };
%apply const unsigned short & { const uint16_t & };
%apply unsigned int { uint32_t };
%apply const unsigned int & { const uint32_t & };
%apply unsigned long long { uint64_t };
%apply const unsigned long long & { const uint64_t & };

// Remove the 'm' prefix of all members.
%rename("%(regex:/^(m)(.*)/\\2/)s") "";

// Convert first character of function names to lowercase.
%rename("%(firstlowercase)s", %$isfunction) "";

// Insert the code of loading native shared library into generated Java class.
%pragma(java) jniclasscode=%{
    static {
        try {
            System.loadLibrary("commissioner-java");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("failed to load native commissioner library!\n" + e);
            System.exit(1);
        }
    }
%}

%feature("director") ot::commissioner::CommissionerHandler;
%feature("director") ot::commissioner::Logger;

%template(ChannelMask) std::vector<ot::commissioner::ChannelMaskEntry>;
%template(StringVector) std::vector<std::string>;

%typemap(jstype) std::string& OUTPUT "String[]"
%typemap(jtype)  std::string& OUTPUT "String[]"
%typemap(jni)    std::string& OUTPUT "jobjectArray"
%typemap(javain) std::string& OUTPUT "$javainput"
%typemap(in)     std::string& OUTPUT (std::string temp) {
    if (!$input) {
        SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
        return $null;
    }
    if (JCALL1(GetArrayLength, jenv, $input) == 0) {
        SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
    }
    $1 = &temp;
}
%typemap(argout) std::string& OUTPUT {
  jstring jvalue = JCALL1(NewStringUTF, jenv, temp$argnum.c_str()); 
  JCALL3(SetObjectArrayElement, jenv, $input, 0, jvalue);
}

%apply std::string& OUTPUT { std::string &aMeshLocalAddr }
%apply std::string& OUTPUT { std::string &aExistingCommissionerId }

%shared_ptr(ot::commissioner::Logger)
%shared_ptr(ot::commissioner::Commissioner)

namespace ot {
namespace commissioner {
    // Remove async commissioner APIs.
    %ignore Commissioner::Connect(ErrorHandler aHandler, const std::string &aAddr, uint16_t aPort);
    %ignore Commissioner::Petition(PetitionHandler aHandler, const std::string &aAddr, uint16_t aPort);
    %ignore Commissioner::Resign(ErrorHandler aHandler);
    %ignore Commissioner::GetCommissionerDataset(Handler<CommissionerDataset> aHandler, uint16_t aDatasetFlags);
    %ignore Commissioner::SetCommissionerDataset(ErrorHandler aHandler, const CommissionerDataset &aDataset);
    %ignore Commissioner::SetBbrDataset(ErrorHandler aHandler, const BbrDataset &aDataset);
    %ignore Commissioner::GetBbrDataset(Handler<BbrDataset> aHandler, uint16_t aDatasetFlags);
    %ignore Commissioner::GetRawActiveDataset(Handler<ByteArray> aHandler, uint16_t aDatasetFlags);
    %ignore Commissioner::GetActiveDataset(Handler<ActiveOperationalDataset> aHandler, uint16_t aDatasetFlags);
    %ignore Commissioner::SetActiveDataset(ErrorHandler aHandler, const ActiveOperationalDataset &aActiveDataset);
    %ignore Commissioner::GetPendingDataset(Handler<PendingOperationalDataset> aHandler, uint16_t aDatasetFlags);
    %ignore Commissioner::SetPendingDataset(ErrorHandler aHandler, const PendingOperationalDataset &aPendingDataset);
    %ignore Commissioner::SetSecurePendingDataset(ErrorHandler                     aHandler,
                                                  const std::string &              aPbbrAddr,
                                                  uint32_t                         aMaxRetrievalTimer,
                                                  const PendingOperationalDataset &aDataset);
    %ignore Commissioner::CommandReenroll(ErrorHandler aHandler, const std::string &aDstAddr);
    %ignore Commissioner::CommandDomainReset(ErrorHandler aHandler, const std::string &aDstAddr);
    %ignore Commissioner::CommandMigrate(ErrorHandler       aHandler,
                                         const std::string &aDstAddr,
                                         const std::string &aDesignatedNetwork);
    %ignore Commissioner::AnnounceBegin(ErrorHandler       aHandler,
                                        uint32_t           aChannelMask,
                                        uint8_t            aCount,
                                        uint16_t           aPeriod,
                                        const std::string &aDstAddr);
    %ignore Commissioner::PanIdQuery(ErrorHandler       aHandler,
                                     uint32_t           aChannelMask,
                                     uint16_t           aPanId,
                                     const std::string &aDstAddr);
    %ignore Commissioner::EnergyScan(ErrorHandler       aHandler,
                                     uint32_t           aChannelMask,
                                     uint8_t            aCount,
                                     uint16_t           aPeriod,
                                     uint16_t           aScanDuration,
                                     const std::string &aDstAddr);
    %ignore Commissioner::RegisterMulticastListener(Handler<uint8_t>                aHandler,
                                                    const std::string &             aPbbrAddr,
                                                    const std::vector<std::string> &aMulticastAddrList,
                                                    uint32_t                        aTimeout);
    %ignore Commissioner::RequestToken(Handler<ByteArray> aHandler, const std::string &aAddr, uint16_t aPort);
    %ignore Commissioner::CommandDiagGet(Handler<ByteArray> aHandler, uint16_t aRloc, uint64_t aDiagTlvFlags);

    // Remove operators and move constructor of Error, XpanId, PanId.
    %ignore Error::operator=(const Error &aError);
    %ignore Error::Error(Error &&aError) noexcept;
    %ignore Error::operator=(Error &&aError) noexcept;
    %ignore Error::operator==(const Error &aOther) const;
    %ignore Error::operator!=(const Error &aOther) const;
    %ignore XpanId::operator==(const XpanId &aOther) const;
    %ignore XpanId::operator!=(const uint64_t aOther) const;
    %ignore XpanId::operator<(const XpanId aOther) const;
    %ignore XpanId::operator std::string() const;
    %ignore PanId::operator=(uint16_t aValue);
    %ignore PanId::operator uint16_t() const;
    %ignore operator==(const Error &aError, const ErrorCode &aErrorCode);
    %ignore operator!=(const Error &aError, const ErrorCode &aErrorCode);
    %ignore operator==(const ErrorCode &aErrorCode, const Error &aError);
    %ignore operator!=(const ErrorCode &aErrorCode, const Error &aError);
}
}

%include <commissioner/defines.hpp>
%include <commissioner/error.hpp>
%include <commissioner/network_data.hpp>
%include <commissioner/commissioner.hpp>
%include <commissioner/commissioner.hpp>
