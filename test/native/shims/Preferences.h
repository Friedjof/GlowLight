#ifndef GLOW_SHIM_PREFERENCES_H
#define GLOW_SHIM_PREFERENCES_H

#include <map>
#include <string>

#include "Arduino.h"

namespace glow_shim {
extern std::map<std::string, std::string> preferencesValues;
extern bool preferencesBeginShouldFail;
extern bool preferencesPutShouldFail;
void resetPreferences();
}  // namespace glow_shim

class Preferences {
 public:
  bool begin(const char* name, bool readOnly = false) {
    if (glow_shim::preferencesBeginShouldFail || name == nullptr) return false;
    this->name = name;
    this->readOnly = readOnly;
    this->opened = true;
    return true;
  }

  String getString(const char* key, const String& defaultValue = String()) const {
    if (!this->opened || key == nullptr) return defaultValue;
    auto found = glow_shim::preferencesValues.find(this->qualified(key));
    return found == glow_shim::preferencesValues.end() ? defaultValue
                                                        : String(found->second);
  }

  size_t putString(const char* key, const String& value) {
    if (!this->opened || this->readOnly || key == nullptr ||
        glow_shim::preferencesPutShouldFail) return 0;
    glow_shim::preferencesValues[this->qualified(key)] = value.c_str();
    return value.length();
  }

  bool clear() {
    if (!this->opened || this->readOnly) return false;
    std::string prefix = this->name + ":";
    for (auto item = glow_shim::preferencesValues.begin();
         item != glow_shim::preferencesValues.end();) {
      if (item->first.rfind(prefix, 0) == 0) item = glow_shim::preferencesValues.erase(item);
      else ++item;
    }
    return true;
  }

  void end() { this->opened = false; }

 private:
  std::string qualified(const char* key) const {
    return this->name + ":" + (key == nullptr ? "" : key);
  }

  std::string name;
  bool readOnly = false;
  bool opened = false;
};

#endif
