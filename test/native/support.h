// Test harness: assertions plus a driver around the real CommunicationService.
#ifndef GLOW_TEST_SUPPORT_H
#define GLOW_TEST_SUPPORT_H

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "CommunicationService.h"
#include "peer.h"

namespace glowtest {

// ------------------------------------------------------------- assertions ---

struct TestCase {
  const char* name;
  std::function<void()> body;
};

void registerTest(const char* name, std::function<void()> body);
int runAllTests(const char* filter);

// Thrown by the assertion macros; caught by the runner.
struct AssertionFailure {
  std::string message;
};

void failAssertion(const char* file, int line, const std::string& message);

#define GLOW_TEST(name)                                             \
  static void name();                                               \
  namespace {                                                       \
  struct name##_registrar {                                         \
    name##_registrar() { ::glowtest::registerTest(#name, name); }   \
  } name##_registrar_instance;                                      \
  }                                                                 \
  static void name()

#define CHECK(condition)                                                  \
  do {                                                                    \
    if (!(condition))                                                     \
      ::glowtest::failAssertion(__FILE__, __LINE__, "CHECK(" #condition ")"); \
  } while (0)

#define CHECK_EQ(actual, expected)                                             \
  do {                                                                         \
    auto glowActual = (actual);                                                \
    auto glowExpected = (expected);                                            \
    if (!(glowActual == glowExpected))                                         \
      ::glowtest::failAssertion(__FILE__, __LINE__,                            \
                                std::string("CHECK_EQ(" #actual ", " #expected \
                                            ") -> ") +                         \
                                    ::glowtest::describe(glowActual) + " != " + \
                                    ::glowtest::describe(glowExpected));       \
  } while (0)

inline std::string describe(const std::string& value) { return "\"" + value + "\""; }
inline std::string describe(const char* value) {
  return std::string("\"") + (value != nullptr ? value : "(null)") + "\"";
}

template <typename T>
inline std::string describe(const T& value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_unsigned_v<T>) {
    return std::to_string(static_cast<unsigned long long>(value));
  } else if constexpr (std::is_integral_v<T>) {
    return std::to_string(static_cast<long long>(value));
  } else {
    return "<value>";
  }
}

// ----------------------------------------------------------------- device ---

// Drives one lamp: the genuine CommunicationService on top of the host shims.
class Device {
 public:
  explicit Device(const std::string& macHex = "983dae52877c", uint64_t rngSeed = 12345);
  ~Device();

  // Recreates the service with a fresh boot id, as a power cycle would.
  void reboot();

  void pump(int iterations = 6);
  void advance(uint32_t milliseconds, int iterations = 2);

  CommunicationService& service() { return *this->service_; }

  // Feeds a raw frame in. The source MAC defaults to the header's MAC.
  void deliver(const Bytes& frame);
  void deliver(const Bytes& frame, const std::string& sourceMacHex);

  // Every transmitted frame, decrypted with the group key.
  std::vector<ParsedFrame> outbox();
  std::vector<ParsedFrame> outbox(Frame type);
  size_t rawOutboxSize() const;
  void clearOutbox();

  std::array<uint8_t, kBootIdSize> bootId();
  const std::array<uint8_t, kMacSize>& mac() const { return this->mac_; }
  uint32_t nodeId() const { return Peer::nodeIdFor(this->mac_.data()); }

  bool knows(uint32_t nodeId) const;
  size_t nodeCount() const;

  // Payloads handed to the application callback, in order.
  std::vector<std::pair<uint32_t, std::string>> messages;
  std::vector<MessageType> messageTypes;
  std::vector<uint32_t> connections;

 private:
  void boot();

  std::unique_ptr<CommunicationService> service_;
  std::array<uint8_t, kMacSize> mac_{};
  Bytes groupKey_;
  uint64_t rngSeed_;
};

// The group key compiled into the native test build.
Bytes testGroupKey();

// Runs HELLO -> CHALLENGE -> PROOF and returns true once the peer is authenticated.
bool handshake(Device& device, Peer& peer);

// Extracts the challenge a device issued for a specific peer, if any.
bool findChallengeFor(Device& device, const Peer& peer, Bytes* challenge);

}  // namespace glowtest

#endif
