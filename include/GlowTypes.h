#ifndef GLOWTYPES_H
#define GLOWTYPES_H

#include <Arduino.h>

// ---- Enums ----
enum RegistryMessageType {
  SET = 0,
  GET = 1
};

enum LinkMessageType {
  HEARTBEAT = 0,
  ECHO = 1,
  DATA = 2,
  COMMAND = 3
};

enum Command {
  NAMESPACE = 0,
  SYNC = 1,
  ACK = 2
};

// ---- Structs ----
typedef struct {
  uint64_t uptime;
  uint64_t lastSeen;
} PeerInfo;

#endif // GLOWTYPES_H