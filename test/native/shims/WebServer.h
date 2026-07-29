#ifndef GLOW_SHIM_WEBSERVER_H
#define GLOW_SHIM_WEBSERVER_H

#include <functional>
#include <map>
#include <string>

#include "Arduino.h"

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };
enum HTTPUploadStatus {
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED,
};

struct HTTPUpload {
  HTTPUploadStatus status = UPLOAD_FILE_START;
  uint8_t* buf = nullptr;
  size_t currentSize = 0;
};

namespace glow_shim {
inline bool webAuthenticated = false;
inline int webBeginCalls = 0;
inline int webStopCalls = 0;
inline int webAuthChallenges = 0;
inline int webLastStatus = 0;
inline std::string webLastBody;

inline void resetWebServer() {
  webAuthenticated = false;
  webBeginCalls = 0;
  webStopCalls = 0;
  webAuthChallenges = 0;
  webLastStatus = 0;
  webLastBody.clear();
}
}  // namespace glow_shim

class WebServer {
 public:
  explicit WebServer(int) {}

  void on(const char*, HTTPMethod, std::function<void()> handler) {
    this->handler = handler;
  }
  void on(const char*, HTTPMethod, std::function<void()> handler,
          std::function<void()> uploadHandler) {
    this->handler = handler;
    this->uploadHandler = uploadHandler;
  }
  void begin() { ++glow_shim::webBeginCalls; }
  void stop() { ++glow_shim::webStopCalls; }
  void handleClient() {}
  void collectHeaders(const char*[], size_t) {}
  bool hasHeader(const char* name) const {
    return this->headers.find(name == nullptr ? "" : name) != this->headers.end();
  }
  String header(const char* name) const {
    auto found = this->headers.find(name == nullptr ? "" : name);
    return found == this->headers.end() ? String() : String(found->second);
  }
  bool authenticate(const char*, const char*) const {
    return glow_shim::webAuthenticated;
  }
  void requestAuthentication(HTTPAuthMethod, const char*) {
    ++glow_shim::webAuthChallenges;
  }
  void sendHeader(const char*, const char*, bool = false) {}
  void send(int status, const char*, const String& body = String()) {
    glow_shim::webLastStatus = status;
    glow_shim::webLastBody = body.c_str();
  }
  void send_P(int status, const char*, const char*) {
    glow_shim::webLastStatus = status;
  }
  HTTPUpload& upload() { return this->currentUpload; }
  String arg(const char* name) const {
    auto found = this->arguments.find(name == nullptr ? "" : name);
    return found == this->arguments.end() ? String() : String(found->second);
  }

  HTTPUpload currentUpload;
  std::map<std::string, std::string> arguments;
  std::map<std::string, std::string> headers;
  std::function<void()> handler;
  std::function<void()> uploadHandler;
};

#endif
