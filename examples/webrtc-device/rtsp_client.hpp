#pragma once
#include <memory>
#include <iostream>
#include <functional>

#include <curl/curl.h>

#include <sys/socket.h>
typedef int SOCKET;
const int RTSP_BUFFER_SIZE = 2048;
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

namespace nabto {

class RtspClient: public std::enable_shared_from_this<RtspClient> {
public:
    RtspClient(const std::string& url)
    : url_(url){}

    bool init() {
        if (!setupCurl()) {
            return false;
        }

        return setupRtsp();
    }

    void stop()
    {
      stopped_ = true;
      if (sock_ != 0) {
        close(sock_);
      }

    }

    void run(std::function<void (char*, int)> sender) {
        if (!rtspPlay()) {
            return;
        };
		sock_ = socket(AF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		addr.sin_port = htons(45222);
		if (bind(sock_, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0)
			throw std::runtime_error("Failed to bind UDP socket on 127.0.0.1:45222");

		int rcvBufSize = 212992;
		setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcvBufSize),
		           sizeof(rcvBufSize));
		// Receive from UDP
		char buffer[RTSP_BUFFER_SIZE];
		int len;
        int count = 0;
		while ((len = recv(sock_, buffer, RTSP_BUFFER_SIZE, 0)) >= 0 && !stopped_) {
            count++;
            if (count%100 == 0) {
                std::cout << ".";
            }
            if (count%1600 == 0) {
                std::cout << std::endl;
                count = 0;
            }
//            std::cout << "Received packet size: " << len << std::endl;
            sender(buffer, len);
		}

        // wait forever
        // rtsp_teardown()
        // curl cleanup
    }

private:
    bool setupCurl() {
        CURLcode res;
        res = curl_global_init(CURL_GLOBAL_ALL);
        if(res != CURLE_OK) {
            std::cout << "FAILURE POINT 1" << std::endl;
            return false;
        }
        curl_ = curl_easy_init();
        if(!curl_) {
            std::cout << "FAILURE POINT 2" << std::endl;
            return false;
        }

        res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 3" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 4" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, stdout);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 5" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 6" << std::endl;
            return false;
        }
        return true;
    }

    bool setupRtsp() {
        CURLcode res = CURLE_OK;
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, url_.c_str());
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 7" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 8" << std::endl;
            return false;
        }
        res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 9" << std::endl;
            return false;
        }

        // TODO: This part writes curl data to a file just to read it again immidiately. Replace this with something which writes to a buffer.

        FILE *sdp_fp = fopen("test-video.sdp", "wb");
        if(!sdp_fp) {
            std::cout << "FAILURE POINT 10" << std::endl;
            sdp_fp = stdout;
        } else {
            std::cout << "Writing SDP to 'test-video.sdp'" << std::endl;
        }

        res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, sdp_fp);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 11" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 12" << std::endl;
            return false;
        }
        res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 13" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, stdout);
        if(sdp_fp != stdout) {
            fclose(sdp_fp);
        }

        char control[256];
        char s[256];
        sdp_fp = fopen("test-video.sdp", "rb");
        control[0] = '\0';
        if(sdp_fp) {
            while(fgets(s, 254, sdp_fp)) {
                sscanf(s, " a = control: %32s", control);
            }
            fclose(sdp_fp);
        }

        std::string ctrlAttr(control);
        std::string setupUri = url_ + "/" + ctrlAttr;
        std::string transport = "RTP/AVP;unicast;client_port=45222-45223";


        res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, setupUri.c_str());
          if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 14" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_TRANSPORT, transport.c_str());
          if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 15" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
          if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 16" << std::endl;
            return false;
        }
        res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 17" << std::endl;
            return false;
        }
        return true;
    }

    bool rtspPlay() {
        const char *range = "npt=now-";
        CURLcode res = CURLE_OK;
        std::string uri = url_ + "/";
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, uri.c_str());
          if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 18" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RANGE, range);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 19" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 20" << std::endl;
            return false;
        }
        res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 21" << std::endl;
            return false;
        }

        // switch off using range again
        res = curl_easy_setopt(curl_, CURLOPT_RANGE, NULL);
          if (res != CURLE_OK) {
            std::cout << "FAILURE POINT 22" << std::endl;
            return false;
        }

        return true;
    }

    CURL* curl_;
	SOCKET sock_ = 0;
    std::string url_;
    bool stopped_ = false;
};


} // namespace



/***************** ORIGINAL EXAMPLE CODE ***************/
/*
 * Copyright (c) 2011 - 2021, Jim Hollinger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Jim Hollinger nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/* <DESC>
 * A basic RTSP transfer
 * </DESC>
 */
 /*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (WIN32)
#include <conio.h>  // _getch()
#else
#include <termios.h>
#include <unistd.h>

static int _getch(void)
{
  struct termios oldt, newt;
  int ch;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}
#endif

#include <curl/curl.h>

#define VERSION_STR  "V1.0"

// error handling macros
#define my_curl_easy_setopt(A, B, C)                               \
  do {                                                             \
    res = curl_easy_setopt((A), (B), (C));                         \
    if(res != CURLE_OK)                                            \
      fprintf(stderr, "curl_easy_setopt(%s, %s, %s) failed: %d\n", \
#A , #B, #C, res);                                    \
  } while(0)

#define my_curl_easy_perform(A)                                         \
  do {                                                                  \
    res = curl_easy_perform(A);                                         \
    if(res != CURLE_OK)                                                 \
      fprintf(stderr, "curl_easy_perform(%s) failed: %d\n", #A, res);   \
  } while(0)

// send RTSP OPTIONS request
static void rtsp_options(CURL *curl, const char *uri)
{
  CURLcode res = CURLE_OK;
  printf("\nRTSP: OPTIONS %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
  my_curl_easy_perform(curl);
}


// send RTSP DESCRIBE request and write sdp response to a file
static void rtsp_describe(CURL *curl, const char *uri,
                          const char *sdp_filename)
{
  CURLcode res = CURLE_OK;
  FILE *sdp_fp = fopen(sdp_filename, "wb");
  printf("\nRTSP: DESCRIBE %s\n", uri);
  if(!sdp_fp) {
    fprintf(stderr, "Could not open '%s' for writing\n", sdp_filename);
    sdp_fp = stdout;
  }
  else {
    printf("Writing SDP to '%s'\n", sdp_filename);
  }
  my_curl_easy_setopt(curl, CURLOPT_WRITEDATA, sdp_fp);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
  my_curl_easy_perform(curl);
  my_curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
  if(sdp_fp != stdout) {
    fclose(sdp_fp);
  }
}

// send RTSP SETUP request
static void rtsp_setup(CURL *curl, const char *uri, const char *transport)
{
  CURLcode res = CURLE_OK;
  printf("\nRTSP: SETUP %s\n", uri);
  printf("      TRANSPORT %s\n", transport);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
  my_curl_easy_perform(curl);
}


// send RTSP PLAY request
static void rtsp_play(CURL *curl, const char *uri, const char *range)
{
  CURLcode res = CURLE_OK;
  printf("\nRTSP: PLAY %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RANGE, range);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
  my_curl_easy_perform(curl);

  // switch off using range again
  my_curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
}


// send RTSP TEARDOWN request
static void rtsp_teardown(CURL *curl, const char *uri)
{
  CURLcode res = CURLE_OK;
  printf("\nRTSP: TEARDOWN %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
  my_curl_easy_perform(curl);
}


// convert url into an sdp filename
static void get_sdp_filename(const char *url, char *sdp_filename,
                             size_t namelen)
{
  const char *s = strrchr(url, '/');
  strcpy(sdp_filename, "video.sdp");
  if(s) {
    s++;
    if(s[0] != '\0') {
      snprintf(sdp_filename, namelen, "%s.sdp", s);
    }
  }
}


// scan sdp file for media control attribute
static void get_media_control_attribute(const char *sdp_filename,
                                        char *control)
{
  int max_len = 256;
  char *s = malloc(max_len);
  FILE *sdp_fp = fopen(sdp_filename, "rb");
  control[0] = '\0';
  if(sdp_fp) {
    while(fgets(s, max_len - 2, sdp_fp)) {
      sscanf(s, " a = control: %32s", control);
    }
    fclose(sdp_fp);
  }
  free(s);
}


// main app
int main(int argc, char * const argv[])
{
#if 1
  const char *transport = "RTP/AVP;unicast;client_port=1234-1235";  // UDP
#else
  // TCP
  const char *transport = "RTP/AVP/TCP;unicast;client_port=1234-1235";
#endif
  const char *range = "0.000-";
  int rc = EXIT_SUCCESS;
  char *base_name = NULL;

  printf("\nRTSP request %s\n", VERSION_STR);
  printf("    Project website: "
    "https://github.com/BackupGGCode/rtsprequest\n");
  printf("    Requires curl V7.20 or greater\n\n");

  // check command line
  if((argc != 2) && (argc != 3)) {
    base_name = strrchr(argv[0], '/');
    if(!base_name) {
      base_name = strrchr(argv[0], '\\');
    }
    if(!base_name) {
      base_name = argv[0];
    }
    else {
      base_name++;
    }
    printf("Usage:   %s url [transport]\n", base_name);
    printf("         url of video server\n");
    printf("         transport (optional) specifier for media stream"
           " protocol\n");
    printf("         default transport: %s\n", transport);
    printf("Example: %s rtsp://192.168.0.2/media/video1\n\n", base_name);
    rc = EXIT_FAILURE;
  }
  else {
    const char *url = argv[1];
    char *uri = malloc(strlen(url) + 32);
    char *sdp_filename = malloc(strlen(url) + 32);
    char *control = malloc(strlen(url) + 32);
    CURLcode res;
    get_sdp_filename(url, sdp_filename, strlen(url) + 32);
    if(argc == 3) {
      transport = argv[2];
    }

    // initialize curl
    res = curl_global_init(CURL_GLOBAL_ALL);
    if(res == CURLE_OK) {
      curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
      CURL *curl;
      fprintf(stderr, "    curl V%s loaded\n", data->version);

      // initialize this curl session
      curl = curl_easy_init();
      if(curl) {
        my_curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        my_curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        my_curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
        my_curl_easy_setopt(curl, CURLOPT_URL, url);

        // request server options
        snprintf(uri, strlen(url) + 32, "%s", url);
        rtsp_options(curl, uri);

        // request session description and write response to sdp file
        rtsp_describe(curl, uri, sdp_filename);

        // get media control attribute from sdp file
        get_media_control_attribute(sdp_filename, control);

        // setup media stream
        snprintf(uri, strlen(url) + 32, "%s/%s", url, control);
        rtsp_setup(curl, uri, transport);

        // start playing media stream
        snprintf(uri, strlen(url) + 32, "%s/", url);
        rtsp_play(curl, uri, range);
        printf("Playing video, press any key to stop ...");
        _getch();
        printf("\n");

        // teardown session
        rtsp_teardown(curl, uri);

        // cleanup
        curl_easy_cleanup(curl);
        curl = NULL;
      }
      else {
        fprintf(stderr, "curl_easy_init() failed\n");
      }
      curl_global_cleanup();
    }
    else {
      fprintf(stderr, "curl_global_init(%s) failed: %d\n",
              "CURL_GLOBAL_ALL", res);
    }
    free(control);
    free(sdp_filename);
    free(uri);
  }

  return rc;
}

*/
