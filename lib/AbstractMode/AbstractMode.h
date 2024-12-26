#ifndef ABSTRACTMODE_H
#define ABSTRACTMODE_H

#include <Arduino.h>
#include <ArrayList.h>
#include <functional>

#include "LightService.h"
#include "DistanceService.h"

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

		uint64_t startWipe = 0;
		uint64_t stopWipe = 0;
		bool wipe = false;

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

		result_t currentResult = {DISTANCE_MAX_MM, LED_DEFAULT_BRIGHTNESS};
		result_t lastResult = {0, 0};

		uint16_t brightness = LED_DEFAULT_BRIGHTNESS;

		uint16_t expNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);
		uint16_t invExpNormalize(uint16_t input, uint16_t min, uint16_t max, uint16_t levels, double factor);

		bool addOption(String title, std::function<void()> callback, bool alert = true, bool onlyOnce = false, bool disabled = false);
		bool callCurrentOption();
		bool recallCurrentOption();

	public:
		AbstractMode(LightService* lightService, DistanceService* distanceService);
		~AbstractMode();
		
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

		void loop();
		void first();

		virtual void setup() = 0;
		virtual void customFirst() = 0;
		virtual void customLoop() = 0;
		virtual void last() = 0;
		virtual void customClick() = 0;
};

#endif
