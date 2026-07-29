// Host shim for the Arduino core, used by the native GlowLight test suite.
// Only the surface that CommunicationService actually touches is provided.
#ifndef GLOW_SHIM_ARDUINO_H
#define GLOW_SHIM_ARDUINO_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#define PROGMEM
#define FPSTR(value) (value)

using std::exp;
using std::log;

namespace glow_shim {
// Virtual clock. Tests advance it explicitly so timeouts are deterministic.
extern uint32_t clockMillis;
extern bool serialEcho;
extern std::vector<std::string> serialLog;
}  // namespace glow_shim

inline unsigned long millis() { return glow_shim::clockMillis; }
inline unsigned long micros() { return glow_shim::clockMillis * 1000UL; }
inline void delay(unsigned long ms) { glow_shim::clockMillis += ms; }

template <typename T>
constexpr const T& min(const T& a, const T& b) {
  return a < b ? a : b;
}

template <typename T>
constexpr const T& max(const T& a, const T& b) {
  return a > b ? a : b;
}

// Minimal stand-in for the Arduino String class. ArduinoJson talks to it
// through c_str()/length()/concat()/operator=, which is all reproduced here.
class String {
 public:
  String() = default;
  String(const char* value) {
    if (value != nullptr) this->value_ = value;
  }
  String(const std::string& value) : value_(value) {}
  String(const String&) = default;
  String& operator=(const String&) = default;

  String& operator=(const char* value) {
    this->value_ = (value != nullptr) ? value : "";
    return *this;
  }

  unsigned int length() const { return static_cast<unsigned int>(this->value_.size()); }
  const char* c_str() const { return this->value_.c_str(); }
  bool isEmpty() const { return this->value_.empty(); }
  bool startsWith(const char* prefix) const {
    return prefix != nullptr && this->value_.rfind(prefix, 0) == 0;
  }
  bool startsWith(const String& prefix) const {
    return this->value_.rfind(prefix.value_, 0) == 0;
  }
  bool endsWith(const String& suffix) const { return this->endsWith(suffix.c_str()); }
  bool endsWith(const char* suffix) const {
    if (suffix == nullptr) return false;
    std::string needle(suffix);
    return this->value_.size() >= needle.size() &&
           this->value_.compare(this->value_.size() - needle.size(), needle.size(),
                                needle) == 0;
  }
  String substring(unsigned int from) const {
    if (from >= this->value_.size()) return String();
    return String(this->value_.substr(from));
  }
  String substring(unsigned int from, unsigned int to) const {
    if (from >= this->value_.size() || from >= to) return String();
    size_t length = min(static_cast<size_t>(to - from), this->value_.size() - from);
    return String(this->value_.substr(from, length));
  }
  int indexOf(char needle) const {
    size_t position = this->value_.find(needle);
    return position == std::string::npos ? -1 : static_cast<int>(position);
  }
  int indexOf(const char* needle) const {
    if (needle == nullptr) return -1;
    size_t position = this->value_.find(needle);
    return position == std::string::npos ? -1 : static_cast<int>(position);
  }
  long toInt() const { return std::strtol(this->value_.c_str(), nullptr, 10); }
  void trim() {
    const char* whitespace = " \t\n\r\f\v";
    size_t first = this->value_.find_first_not_of(whitespace);
    if (first == std::string::npos) {
      this->value_.clear();
      return;
    }
    size_t last = this->value_.find_last_not_of(whitespace);
    this->value_ = this->value_.substr(first, last - first + 1);
  }
  void toUpperCase() {
    for (char& character : this->value_) {
      character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
  }
  void toLowerCase() {
    for (char& character : this->value_) {
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
  }
  void toCharArray(char* buffer, unsigned int length) const {
    if (buffer == nullptr || length == 0) return;
    std::snprintf(buffer, length, "%s", this->value_.c_str());
  }
  void replace(const char* from, const String& to) {
    if (from == nullptr || *from == '\0') return;
    std::string needle(from);
    size_t position = 0;
    while ((position = this->value_.find(needle, position)) != std::string::npos) {
      this->value_.replace(position, needle.length(), to.value_);
      position += to.value_.length();
    }
  }

  unsigned char concat(const char* value) {
    if (value != nullptr) this->value_ += value;
    return 1;
  }

  char operator[](size_t index) const { return this->value_[index]; }
  bool operator==(const String& other) const { return this->value_ == other.value_; }
  bool operator==(const char* other) const {
    return other != nullptr && this->value_ == other;
  }
  bool operator!=(const String& other) const { return !(*this == other); }
  bool operator!=(const char* other) const { return !(*this == other); }

  String& operator+=(const String& other) {
    this->value_ += other.value_;
    return *this;
  }
  String& operator+=(const char* other) {
    if (other != nullptr) this->value_ += other;
    return *this;
  }

  const std::string& str() const { return this->value_; }

 private:
  std::string value_;
};

inline String operator+(const String& left, const String& right) {
  String result(left);
  result += right;
  return result;
}
inline String operator+(const String& left, const char* right) {
  String result(left);
  result += right;
  return result;
}
inline String operator+(const char* left, const String& right) {
  String result(left);
  result += right;
  return result;
}

class SerialClass {
 public:
  void begin(unsigned long) {}
  void print(const char* text) { this->record(text, false); }
  void print(const String& text) { this->record(text.c_str(), false); }
  void println(const char* text) { this->record(text, true); }
  void println(const String& text) { this->record(text.c_str(), true); }
  void println() { this->record("", true); }

  void printf(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    this->record(buffer, false);
  }

 private:
  void record(const char* text, bool newline) {
    this->pending_ += text;
    if (newline) this->pending_ += '\n';

    size_t position;
    while ((position = this->pending_.find('\n')) != std::string::npos) {
      std::string line = this->pending_.substr(0, position);
      this->pending_.erase(0, position + 1);
      glow_shim::serialLog.push_back(line);
      if (glow_shim::serialEcho) fprintf(stderr, "  [serial] %s\n", line.c_str());
    }
  }

  std::string pending_;
};

extern SerialClass Serial;

namespace glow_shim {
inline int restartCalls = 0;
}

class ESPClass {
 public:
  void restart() { ++glow_shim::restartCalls; }
};

inline ESPClass ESP;

#endif
