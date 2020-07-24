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
 *   The file implements Console.
 */

#include "app/cli/console.hpp"

#include <iostream>

#include <readline/history.h>
#include <readline/readline.h>

namespace ot {

namespace commissioner {

std::string Console::Read()
{
    const char *line = "";

    while (line == nullptr || strlen(line) == 0)
    {
        line = readline("> ");
    }

    add_history(line);

    return line;
}

void Console::Write(const std::string &aLine, Color aColor)
{
    static const std::string kResetCode = "\u001b[0m";
    std::string              colorCode;

    switch (aColor)
    {
    case Color::kDefault:
    case Color::kWhite:
        colorCode = "\u001b[37m";
        break;
    case Color::kRed:
        colorCode = "\u001b[31m";
        break;
    case Color::kGreen:
        colorCode = "\u001b[32m";
        break;
    case Color::kBlue:
        colorCode = "\u001b[34m";
        break;
    }

    std::cout << colorCode << aLine << kResetCode << std::endl;
}

} // namespace commissioner

} // namespace ot
