#include "support.h"

#include <cstdio>
#include <cstring>

#include "esp_now.h"
#include "esp_system.h"

namespace glowtest {
namespace {

std::vector<TestCase>& testRegistry() {
  static std::vector<TestCase> registry;
  return registry;
}

}  // namespace

void registerTest(const char* name, std::function<void()> body) {
  testRegistry().push_back({name, std::move(body)});
}

void failAssertion(const char* file, int line, const std::string& message) {
  const char* shortFile = strrchr(file, '/');
  throw AssertionFailure{std::string(shortFile != nullptr ? shortFile + 1 : file) + ":" +
                         std::to_string(line) + ": " + message};
}

int runAllTests(const char* filter) {
  int failures = 0;
  int executed = 0;

  for (const TestCase& test : testRegistry()) {
    if (filter != nullptr && strstr(test.name, filter) == nullptr) continue;
    ++executed;
    try {
      test.body();
      printf("  ok   %s\n", test.name);
    } catch (const AssertionFailure& failure) {
      printf("  FAIL %s\n         %s\n", test.name, failure.message.c_str());
      ++failures;
    } catch (const std::exception& error) {
      printf("  FAIL %s\n         unexpected exception: %s\n", test.name, error.what());
      ++failures;
    }
  }

  printf("\n%d test(s) run, %d failed\n", executed, failures);
  return failures == 0 ? 0 : 1;
}

Bytes testGroupKey() { return fromHex(GLOW_GROUP_KEY_HEX); }

Device::Device(const std::string& macHex, uint64_t rngSeed)
    : groupKey_(testGroupKey()), rngSeed_(rngSeed) {
  Bytes mac = fromHex(macHex);
  memcpy(this->mac_.data(), mac.data(), kMacSize);
  glow_shim::clockMillis = 0;
  glow_shim::serialLog.clear();
  glow_shim::seedRandom(rngSeed);
  this->boot();
}

Device::~Device() {
  this->service_.reset();
  glow_shim::resetEspNow();
}

void Device::boot() {
  this->service_.reset();
  glow_shim::resetEspNow();
  memcpy(glow_shim::localMac, this->mac_.data(), kMacSize);

  this->service_ = std::make_unique<CommunicationService>();
  this->service_->onNewConnection(
      [this](uint32_t nodeId) { this->connections.push_back(nodeId); });
  this->service_->onReceived(
      [this](uint32_t from, JsonDocument message, MessageType type) {
        String serialized;
        serializeJson(message, serialized);
        this->messages.emplace_back(from, std::string(serialized.c_str()));
        this->messageTypes.push_back(type);
      });
  this->service_->setup();
}

void Device::reboot() {
  // A power cycle keeps the MAC but draws a new boot id from the RNG.
  this->boot();
}

void Device::pump(int iterations) {
  for (int i = 0; i < iterations; ++i) this->service_->loop();
}

void Device::advance(uint32_t milliseconds, int iterations) {
  glow_shim::clockMillis += milliseconds;
  this->pump(iterations);
}

void Device::deliver(const Bytes& frame) {
  uint8_t sourceMac[kMacSize] = {};
  if (frame.size() >= kOffsetMac + kMacSize)
    memcpy(sourceMac, frame.data() + kOffsetMac, kMacSize);
  glow_shim::deliverFrame(sourceMac, frame.data(), frame.size());
}

void Device::deliver(const Bytes& frame, const std::string& sourceMacHex) {
  Bytes sourceMac = fromHex(sourceMacHex);
  glow_shim::deliverFrame(sourceMac.data(), frame.data(), frame.size());
}

std::vector<ParsedFrame> Device::outbox() {
  Peer observer("000000000000", std::string(32, '0'), this->groupKey_);
  std::vector<ParsedFrame> frames;
  for (const glow_shim::CapturedFrame& captured : glow_shim::sentFrames) {
    ParsedFrame parsed;
    if (observer.open(captured.data, &parsed)) frames.push_back(parsed);
  }
  return frames;
}

std::vector<ParsedFrame> Device::outbox(Frame type) {
  std::vector<ParsedFrame> filtered;
  for (const ParsedFrame& frame : this->outbox())
    if (frame.frameType() == type) filtered.push_back(frame);
  return filtered;
}

size_t Device::rawOutboxSize() const { return glow_shim::sentFrames.size(); }

void Device::clearOutbox() { glow_shim::sentFrames.clear(); }

std::array<uint8_t, kBootIdSize> Device::bootId() {
  this->pump(1);
  std::vector<ParsedFrame> frames = this->outbox();
  std::array<uint8_t, kBootIdSize> bootId{};
  for (const ParsedFrame& frame : frames) {
    if (memcmp(frame.mac.data(), this->mac_.data(), kMacSize) == 0) {
      bootId = frame.bootId;
      break;
    }
  }
  return bootId;
}

bool Device::knows(uint32_t nodeId) const {
  const ArrayList<GlowNode>& nodes = this->service_->getNodes();
  for (size_t i = 0; i < nodes.size(); ++i)
    if (nodes.get(i).id == nodeId) return true;
  return false;
}

size_t Device::nodeCount() const {
  return static_cast<size_t>(this->service_->getNodes().size());
}

bool findChallengeFor(Device& device, const Peer& peer, Bytes* challenge) {
  // Scan backwards: the most recent challenge is the one still valid.
  std::vector<ParsedFrame> frames = device.outbox(Frame::CHALLENGE);
  for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame) {
    if (frame->plaintext.size() != kMacSize + kBootIdSize + kChallengeSize) continue;
    if (memcmp(frame->plaintext.data(), peer.mac().data(), kMacSize) != 0) continue;
    if (memcmp(frame->plaintext.data() + kMacSize, peer.bootId().data(), kBootIdSize) != 0)
      continue;
    challenge->assign(frame->plaintext.begin() + kMacSize + kBootIdSize,
                      frame->plaintext.end());
    return true;
  }
  return false;
}

bool handshake(Device& device, Peer& peer) {
  device.deliver(peer.build(Frame::HELLO, {}));
  device.pump();

  Bytes challenge;
  if (!findChallengeFor(device, peer, &challenge)) return false;

  device.deliver(peer.build(Frame::PROOF, challenge));
  device.pump();
  return device.knows(peer.nodeId());
}

}  // namespace glowtest
