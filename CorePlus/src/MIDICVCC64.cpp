#include "plugin.hpp"


struct CCMidiOutput : midi::Output {
	uint8_t lastValues[128];
	int64_t frame = -1;

	CCMidiOutput() {
		reset();
	}

	void reset() {
		for (uint8_t n = 0; n < 128; n++) {
			lastValues[n] = -1;
		}
		Output::reset();
	}

	void setValue(uint8_t value, uint8_t cc) {
		if (value == lastValues[cc])
			return;
		lastValues[cc] = value;
		// CC
		midi::Message m;
		m.setStatus(0xb);
		m.setNote(cc);
		m.setValue(value);
		m.setFrame(frame);
		sendMessage(m);
	}

	void setFrame(int64_t frame) {
		this->frame = frame;
	}
};


struct MIDICV_CC_64 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CC_INPUTS, 64),
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	CCMidiOutput midiOutput;
	dsp::Timer rateLimiterTimer;
	int learningId = -1;
	int8_t learnedCcs[64] = {};

	MIDICV_CC_64() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int id = 0; id < 64; id++)
			configInput(CC_INPUTS + id, string::f("Cell %d", id + 1));
		onReset();
	}

	void onReset() override {
		for (int id = 0; id < 64; id++) {
			learnedCcs[id] = id;
		}
		learningId = -1;
		midiOutput.reset();
		midiOutput.midi::Output::reset();
	}

	void process(const ProcessArgs& args) override {
		const float rateLimiterPeriod = 1 / 200.f;
		bool rateLimiterTriggered = (rateLimiterTimer.process(args.sampleTime) >= rateLimiterPeriod);
		if (rateLimiterTriggered)
			rateLimiterTimer.time -= rateLimiterPeriod;
		else
			return;

		midiOutput.setFrame(args.frame);

		for (int id = 0; id < 64; id++) {
			if (learnedCcs[id] < 0)
				continue;

			uint8_t value = (uint8_t) clamp(std::round(inputs[CC_INPUTS + id].getVoltage() / 10.f * 127), 0.f, 127.f);
			midiOutput.setValue(value, learnedCcs[id]);
		}
	}

	void setLearnedCc(int id, int8_t cc) {
		// Unset IDs of similar CCs
		if (cc >= 0) {
			for (int id = 0; id < 64; id++) {
				if (learnedCcs[id] == cc)
					learnedCcs[id] = -1;
			}
		}
		learnedCcs[id] = cc;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* ccsJ = json_array();
		for (int id = 0; id < 64; id++) {
			json_array_append_new(ccsJ, json_integer(learnedCcs[id]));
		}
		json_object_set_new(rootJ, "ccs", ccsJ);

		json_object_set_new(rootJ, "midi", midiOutput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* ccsJ = json_object_get(rootJ, "ccs");
		if (ccsJ) {
			for (int id = 0; id < 64; id++) {
				json_t* ccJ = json_array_get(ccsJ, id);
				if (ccJ)
					setLearnedCc(id, json_integer_value(ccJ));
			}
		}

		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ)
			midiOutput.fromJson(midiJ);
	}
};


struct MIDICV_CC_64Widget : ModuleWidget {
	MIDICV_CC_64Widget(MIDICV_CC_64* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MIDICVCC64.svg"),
			asset::plugin(pluginInstance, "res/MIDICVCC64-dark.svg")));
	

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		
		float space_x = 11.8598;
		float first_x = 8.189;
		
		float space_y = 11.532;
		float first_y = 78.431;

		for (int row = 0; row < 4; row++) {
		    float y = first_y + (row * space_y);
		    for (int col = 0; col < 16; col++) {
	    	        float x = first_x + (col * space_x);
	    	        int port_num = (row * 16) + col;
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x, y)), module, MIDICV_CC_64::CC_INPUTS + port_num));
		    }
		}

		typedef Grid64MidiDisplay<CcChoice<MIDICV_CC_64>> TMidiDisplay;
		TMidiDisplay* display = createWidget<TMidiDisplay>(mm2px(Vec(0, 13.039)));
		display->box.size = mm2px(Vec(194, 55.88));
		display->setMidiPort(module ? &module->midiOutput : NULL);
		display->setModule(module);
		addChild(display);
	}
};


// Use legacy slug for compatibility
Model* modelMIDICVCC64 = createModel<MIDICV_CC_64, MIDICV_CC_64Widget>("MIDICVCC64");

