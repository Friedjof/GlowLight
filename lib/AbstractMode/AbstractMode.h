/*
 * AbstractMode.h
 * This is the abstract class for all modes.
 * It provides the basic structure for all modes and handles the options and brightness as basic features.
 * It also provides some helper functions for normalizing and denormalizing values.
 */

#ifndef ABSTRACTMODE_H
#define ABSTRACTMODE_H

#include <Arduino.h>
#include <ArrayList.h>
#include <functional>
#include <vector>
#include <FastLED.h>

#include "GlowRegistry.h"
#include "LightService.h"
#include "DistanceService.h"
#include "CommunicationService.h"

#include "GlowConfig.h"


struct option_t {
    String title;
    std::function<void()> callback;
    bool alert;
    bool onlyOnce;
    bool disabled;

    option_t() 
        : title(""), callback(nullptr), alert(false), onlyOnce(false), disabled(false) {}
};


class AbstractMode {
	private:
		uint8_t currentOption = 0;

		bool optionChanged = false;
		bool optionCalled = false;

		std::vector<option_t> options;

	protected:
		String title;
		String id;
		String description;
		String author;
		String contact;
		String version;
		String license;
		uint16_t stateSchemaVersion = 1;

		LightService* lightService;
		DistanceService* distanceService;
		CommunicationService* communicationService;

		GlowRegistry registry;
		JsonDocument commandDescriptors;

		result_t currentResult = {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS, 0};
		result_t lastResult = {0, 0, 0};

		uint8_t desiredBrightness = LED_DEFAULT_BRIGHTNESS;

		uint16_t expNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);
		uint16_t invExpNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);

		bool addOption(String title, std::function<void()> callback, bool alert = true, bool onlyOnce = false, bool disabled = false);
		void declareCommand(const String& command, const JsonDocument& arguments,
		                    bool groupCapable = true);
		bool markSettingReadOnly(const String& key);
		bool callCurrentOption();
		void sendCommand(const String& command, const JsonDocument& payload);
		void applyDesiredBrightness();
		void applyHardwareBrightness(uint8_t brightness);
		virtual void onStateApplied();
		virtual void onSettingChanged(const String& key);
		virtual void onStateActivated();

	public:
		AbstractMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

		String getTitle();
		String getId();
		String getDescription();
		String getAuthor();
		String getContact();
		String getVersion();
		String getLicense();
		uint16_t getStateSchemaVersion();

		bool optionHasChanged();

		bool recallCurrentOption();

		virtual void applyRemoteUpdate(uint16_t distance, uint16_t level);
		virtual bool handleRemoteCommand(const String& command, const JsonDocument& payload);

		bool updateBrightnessFromSensor();
		bool setDesiredBrightness(uint8_t brightness);

		uint8_t getDesiredBrightness();
		uint16_t getLevel();
		uint16_t getDistance();

		uint8_t getCurrentOption();
		uint8_t getNumberOfOptions();
		bool nextOption();
		bool setOption(uint8_t option);

		JsonDocument serialize();
		JsonDocument capabilities();
		bool deserialize(const JsonDocument& doc);
		bool setSetting(const String& key, JsonVariantConst value);
		bool executeCommand(const String& command, const JsonDocument& payload);
		bool acceptsCommand(const String& command, const JsonDocument& payload) const;
		bool commandSupportsGroup(const String& command);

		void loop();
		void first(bool stateRestored = false);
		void modeSetup();

		virtual void setup() = 0;
		virtual void customFirst() = 0;
		virtual void customLoop() = 0;
		virtual void last() = 0;
		virtual void customClick() = 0;
};

#endif
