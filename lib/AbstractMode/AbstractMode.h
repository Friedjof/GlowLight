#ifndef ABSTRACTMODE_H
#define ABSTRACTMODE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArrayList.h>
#include <functional>

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

		ArrayList<option_t> options;

	protected:
		String title;
		String description;
		String author;
		String contact;
		String version;
		String license;

		LightService* lightService;
		DistanceService* distanceService;
		CommunicationService* communicationService;

		JsonDocument registry;

		result_t currentResult = {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS};
		result_t lastResult = {0, 0};

		uint16_t brightness = LED_DEFAULT_BRIGHTNESS;

		uint16_t expNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);
		uint16_t invExpNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);

		bool addOption(String title, std::function<void()> callback, bool alert = true, bool onlyOnce = false, bool disabled = false);
		bool callCurrentOption();
		bool recallCurrentOption();

	public:
		AbstractMode(LightService* lightService, DistanceService* distanceService, CommunicationService* communicationService);

		String getTitle();
		String getDescription();
		String getAuthor();
		String getContact();
		String getVersion();
		String getLicense();

		bool optionHasChanged();

		bool setBrightness();
		bool resetBrightness();
		bool updateBrightness(uint16_t brightness);

		uint16_t getBrightness();
		uint16_t getLevel();
		uint16_t getDistance();

		uint8_t getCurrentOption();
		uint8_t getNumberOfOptions();
		bool nextOption();
		bool setOption(uint8_t option);

		void setVar(String key, String value);
		void setVar(String key, uint16_t value);
		void getVar(String key, double value);
		void getVar(String key, bool value);
		String getVar(String key);
		uint16_t getVar(String key);
		double getVar(String key);
		bool getVar(String key);

		void loop();
		void first();

		virtual void setup() = 0;
		virtual void customFirst() = 0;
		virtual void customLoop() = 0;
		virtual void last() = 0;
		virtual void customClick() = 0;
};

#endif
