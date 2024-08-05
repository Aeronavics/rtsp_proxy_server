/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2024, Live Networks, Inc.  All rights reserved
// LIVE555 Proxy Server
// main program

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include <string>
#include <fstream>

char const *progName;
UsageEnvironment *env;
UserAuthenticationDatabase *authDB = NULL;

// Default values of command-line parameters:
int verbosityLevel = 0;
portNumBits tunnelOverHTTPPortNum = 0;
portNumBits rtspServerPortNum = 554;
char *username = NULL;
char *password = NULL;

static RTSPServer *createRTSPServer(Port port)
{
    return RTSPServer::createNew(*env, port, authDB);
}

void usage()
{
    *env << "Usage: " << progName
         << " [-v|-V]"
         << " [-p <rtspServer-port>]"
         << " <rtsp_url_definition_file>";
    exit(1);
}

int main(int argc, char **argv)
{
    // Increase the maximum size of video frames that we can 'proxy' without truncation.
    // (Such frames are unreasonably large; the back-end servers should really not be sending frames this large!)
    OutPacketBuffer::maxSize = 400000; // bytes

    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    *env << "RTSP Proxy Server\n";

    // Check command-line arguments: optional parameters, then one or more rtsp:// URLs (of streams to be proxied):
    progName = argv[0];
    if (argc < 2)
        usage();
    while (argc > 1)
    {
        // Process initial command-line options (beginning with "-"):
        char *const opt = argv[1];
        if (opt[0] != '-')
            break; // the remaining parameters are assumed to be "rtsp://" URLs

        switch (opt[1])
        {
        case 'v':
        { // verbose output
            verbosityLevel = 1;
            break;
        }

        case 'V':
        { // more verbose output
            verbosityLevel = 2;
            break;
        }

        case 'p':
        {
            // specify a rtsp server port number
            if (argc > 2 && argv[2][0] != '-')
            {
                // The next argument is the rtsp server port number:
                if (sscanf(argv[2], "%hu", &rtspServerPortNum) == 1 && rtspServerPortNum > 0)
                {
                    ++argv;
                    --argc;
                    break;
                }
            }

            // If we get here, the option was specified incorrectly:
            usage();
            break;
        }

        default:
        {
            usage();
            break;
        }
        }

        ++argv;
        --argc;
    }
    if (argc < 2)
        usage(); // there must be at file path at the end

    // Create the RTSP server. Try first with the configured port number,
    // and then with the default port number (554) if different,
    // and then with the alternative port number (8554):
    RTSPServer *rtspServer;
    rtspServer = createRTSPServer(rtspServerPortNum);
    if (rtspServer == NULL)
    {
        if (rtspServerPortNum != 554)
        {
            *env << "Unable to create a RTSP server with port number " << rtspServerPortNum << ": " << env->getResultMsg() << "\n";
            *env << "Trying instead with the standard port numbers (554)...\n";

            rtspServerPortNum = 554;
            rtspServer = createRTSPServer(rtspServerPortNum);
        }
    }
    if (rtspServer == NULL)
    {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    // Open the rtsp list file
    std::ifstream input(argv[1]);

    if (input.is_open())
    {
        std::string line;
        // Create a proxy for each RTSP in the file
        while (std::getline(input, line))
        {
            if (line.front() == '#')
            {
                continue;
            }
            if (line == "")
            {
                continue;
            }
            std::size_t pos = line.find(" ");
            std::string url = line.substr(0, pos);
            std::string name = line.substr(pos + 1);

            char const *proxiedStreamURL = line.c_str();
            char const *streamName = name.c_str();
            ServerMediaSession *sms = ProxyServerMediaSession::createNew(*env, rtspServer,
                                                                        proxiedStreamURL, streamName,
                                                                        username, password, tunnelOverHTTPPortNum, verbosityLevel);
            rtspServer->addServerMediaSession(sms);

            char *proxyStreamURL = rtspServer->rtspURL(sms);
            *env << "RTSP stream, proxying the stream \"" << proxiedStreamURL << "\"\n";
            *env << "\tPlay this stream using the URL: " << proxyStreamURL << "\n";
            delete[] proxyStreamURL;

        }
        input.close();
    }

    // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
    // Try first with the default HTTP port (80), and then with the alternative HTTP
    // port numbers (8000 and 8080).

    // Now, enter the event loop:
    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}


