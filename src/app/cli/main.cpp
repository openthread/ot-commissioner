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
 *   The file is the entrance of the commissioner CLI.
 */

#include <csignal>
#include <string>
#include <thread>

#include <getopt.h>

#include "app/cli/console.hpp"
#include "app/cli/interpreter.hpp"
#include "commissioner/commissioner.hpp"
#include "commissioner/error.hpp"
#include "common/utils.hpp"

using namespace ot::commissioner;

/**
 * The OT-commissioner CLI logo.
 * Generated by http://patorjk.com/software/taag
 * with font=Slant and text="OT-commissioner CLI".
 */
static const std::string kLogo =
    R"(   ____  ______                                   _           _                          ________    ____)"
    "\n"
    R"(  / __ \/_  __/   _________  ____ ___  ____ ___  (_)_________(_)___  ____  ___  _____   / ____/ /   /  _/)"
    "\n"
    R"( / / / / / /_____/ ___/ __ \/ __ `__ \/ __ `__ \/ / ___/ ___/ / __ \/ __ \/ _ \/ ___/  / /   / /    / /  )"
    "\n"
    R"(/ /_/ / / /_____/ /__/ /_/ / / / / / / / / / / / (__  |__  ) / /_/ / / / /  __/ /     / /___/ /____/ /   )"
    "\n"
    R"(\____/ /_/      \___/\____/_/ /_/ /_/_/ /_/ /_/_/____/____/_/\____/_/ /_/\___/_/      \____/_____/___/   )"
    "\n"
    R"(                                                                                                         )"
    "\n";

static void PrintUsage(const std::string &aProgram)
{
    static const std::string usage = "usage: \n"
                                     "help digest:\n    " +
                                     aProgram +
                                     " -h|--help\n"
                                     "version:\n    " +
                                     aProgram +
                                     " -v|--version\n"
                                     "common options\n    " +
                                     aProgram +
                                     " [-r|--registry <registryFileName>] [-c|--config <configFileName>]\n"
                                     "or\n    " +
                                     aProgram + " [-r|--registry <registryFileName>] [configFileName]";

    Console::Write(usage, Console::Color::kWhite);
}

static void PrintVersion()
{
    Console::Write(Commissioner::GetVersion(), Console::Color::kWhite);
}

static Interpreter gInterpreter;
static sigset_t    gSignalSet;

static void HandleSignalInterrupt()
{
    int signalNum = 0;

    while (true)
    {
        sigwait(&gSignalSet, &signalNum);
        gInterpreter.CancelCommand();
    }
}

static option gCommissionerCliOptions[] = {{"help", no_argument, nullptr, 'h'},
                                           {"version", no_argument, nullptr, 'v'},
                                           {"registry", required_argument, nullptr, 'r'},
                                           {"config", required_argument, nullptr, 'c'},
                                           {nullptr, 0, nullptr, 0}};

int main(int argc, const char *argv[])
{
    using namespace ::ot::commissioner::utils;

    Error error;

    std::string progName = argv[0];
    std::string registryFileName;
    std::string configFileName;

    int  ch;
    bool parseParams = true;

    while (parseParams)
    {
        ch = getopt_long(argc, const_cast<char *const *>(argv), "hvc:r:", gCommissionerCliOptions, nullptr);
        switch (ch)
        {
        case 'h':
            PrintUsage(progName);
            ExitNow();
            break;
        case 'v':
            PrintVersion();
            ExitNow();
            break;
        case 'r':
            registryFileName = optarg;
            break;
        case 'c':
            configFileName = optarg;
            break;
        default:
            parseParams = false;
            break;
        };
    }
    argc -= optind;
    argv += optind;

    if (configFileName.empty())
    {
        // If any option left unprocessed then it must be a config file path
        if (argc != 0)
        {
            configFileName = argv[0];
        }
    }

    // Block signals in this thread and subsequently spawned threads.
    sigemptyset(&gSignalSet);
    sigaddset(&gSignalSet, SIGINT);
    pthread_sigmask(SIG_BLOCK, &gSignalSet, nullptr);

    std::thread(HandleSignalInterrupt).detach();

    Console::Write(kLogo, Console::Color::kBlue);

    SuccessOrExit(error = gInterpreter.Init(configFileName, registryFileName));

    gInterpreter.Run();

exit:
    if (error != ErrorCode::kNone)
    {
        Console::Write("start OT-commissioner CLI failed: " + error.ToString(), Console::Color::kRed);
    }
    return error == ErrorCode::kNone ? 0 : -1;
}
