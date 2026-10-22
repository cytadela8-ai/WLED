/* Host tests execute AudioReactive methods extracted from the firmware source.
 * Only hardware task and palette lifecycle boundaries are replaced by counters.
 * ArduinoJson is the same vendored implementation used by the firmware.
 */
#include <algorithm>
#include <cassert>
#include <iostream>
#include "wled00/src/dependencies/json/ArduinoJson-v6.h"

#define ARDUINO_ARCH_ESP32
#define FPSTR(value) value
using std::max;
using std::min;

struct um_data_t {};
struct Usermod {
  virtual bool getUMData(um_data_t**) = 0;
  virtual void readFromJsonState(JsonObject&) = 0;
};

struct AudioReactive : Usermod {
  bool enabled = true;
  bool initDone = false;
  bool addPalettes = true;
  int inputLevel = 128;
  int taskUpdates = 0;
  int paletteAdds = 0;
  int paletteRemoves = 0;
  um_data_t* um_data = nullptr;
  static constexpr auto _name = "AudioReactive";
  static constexpr auto _enabled = "enabled";
  static constexpr auto _inputLvl = "inputLevel";

  void onUpdateBegin(bool) { ++taskUpdates; }
  void createAudioPalettes() { ++paletteAdds; }
  void removeAudioPalettes() { ++paletteRemoves; }

#include "audio_methods.h"
};

static void applyState(AudioReactive& audio, const char* json) {
  DynamicJsonDocument doc(1024);
  assert(!deserializeJson(doc, json));
  JsonObject root = doc.as<JsonObject>();
  audio.readFromJsonState(root);
}

static void testUninitializedData() {
  AudioReactive audio;
  um_data_t data;
  um_data_t* result = &data;
  assert(!audio.getUMData(&result));
  assert(result == &data);
  assert(!audio.getUMData(nullptr));
  audio.initDone = true;
  audio.um_data = &data;
  assert(audio.getUMData(&result));
  assert(result == &data);
  audio.enabled = false;
  assert(!audio.getUMData(&result));
}

static void testEarlyState() {
  AudioReactive audio;
  applyState(audio, R"({"AudioReactive":{"enabled":false,"inputLevel":300}})");
  assert(!audio.enabled);
  assert(audio.inputLevel == 255);
  assert(audio.taskUpdates == 0);
  assert(audio.paletteRemoves == 1);
  applyState(audio, R"({"AudioReactive":{"enabled":true,"inputLevel":-42}})");
  assert(audio.enabled);
  assert(audio.inputLevel == 0);
  assert(audio.taskUpdates == 0);
  assert(audio.paletteAdds == 1);
}

static void testRuntimeState() {
  AudioReactive audio;
  audio.initDone = true;
  audio.addPalettes = false;
  applyState(audio, R"({"AudioReactive":{"enabled":false,"inputLevel":42}})");
  assert(!audio.enabled);
  assert(audio.inputLevel == 42);
  assert(audio.taskUpdates == 1);
  assert(audio.paletteAdds == 0 && audio.paletteRemoves == 0);
  applyState(audio, R"({"AudioReactive":{"enabled":false}})");
  assert(audio.taskUpdates == 1);
  applyState(audio, R"({"AudioReactive":{"enabled":true}})");
  assert(audio.taskUpdates == 2);
}

static void testMissingAndInvalidState() {
  AudioReactive audio;
  applyState(audio, "{}");
  applyState(audio, R"({"AudioReactive":{"enabled":"false","inputLevel":"bad"}})");
  assert(audio.enabled);
  assert(audio.inputLevel == 128);
  assert(audio.taskUpdates == 0);
  assert(audio.paletteAdds == 0 && audio.paletteRemoves == 0);
}

int main() {
  testUninitializedData();
  testEarlyState();
  testRuntimeState();
  testMissingAndInvalidState();
  std::cout << "PASS: data readiness, early settings, runtime updates, invalid input\n";
}
