#include "plugin.hpp"


struct MIDICC_CV_64 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CC_OUTPUT, 64),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;

	/** [cc][channel] */
	int8_t ccValues[128][16];
	/** When LSB is enabled for CC 0-31, the MSB is stored here until the LSB is received.
	[cc][channel]
	*/
	int8_t msbValues[32][16];
	int learningId;
	int8_t learnedCcs[64];
	/** [cell][channel] */
	dsp::ExponentialFilter valueFilters[64][16];
	bool smooth;
	bool mpeMode;
	bool lsbMode;

	MIDICC_CV_64() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int id = 0; id < 64; id++)
			configOutput(CC_OUTPUT + id, string::f("Cell %d", id + 1));

		for (int id = 0; id < 64; id++) {
			for (int c = 0; c < 16; c++) {
				valueFilters[id][c].setTau(1 / 30.f);
			}
		}
		onReset();
	}

	void onReset() override {
		for (uint8_t cc = 0; cc < 128; cc++) {
			for (int c = 0; c < 16; c++) {
				ccValues[cc][c] = 0;
			}
		}
		for (uint8_t cc = 0; cc < 32; cc++) {
			for (int c = 0; c < 16; c++) {
				msbValues[cc][c] = 0;
			}
		}
		learningId = -1;
		for (int id = 0; id < 64; id++) {
			learnedCcs[id] = id;
		}
		midiInput.reset();
		smooth = true;
		mpeMode = false;
		lsbMode = false;
	}

	void process(const ProcessArgs& args) override {
		midi::Message msg;
		while (midiInput.tryPop(&msg, args.frame)) {
			processMessage(msg);
		}

		int channels = mpeMode ? 16 : 1;

		for (int id = 0; id < 64; id++) {
			if (!outputs[CC_OUTPUT + id].isConnected())
				continue;
			outputs[CC_OUTPUT + id].setChannels(channels);

			int8_t cc = learnedCcs[id];
			if (cc < 0) {
				outputs[CC_OUTPUT + id].clearVoltages();
				continue;
			}

			for (int c = 0; c < channels; c++) {
				int16_t cellValue = int16_t(ccValues[cc][c]) * 128;
				if (lsbMode && cc < 32)
					cellValue += ccValues[cc + 32][c];
				// Maximum value for 14-bit CC should be MSB=127 LSB=0, not MSB=127 LSB=127, because this is the maximum value that 7-bit controllers can send.
				float value = float(cellValue) / (128 * 127);
				// Support negative values because the gamepad MIDI driver generates nonstandard 8-bit CC values.
				value = clamp(value, -1.f, 1.f);

				// Detect behavior from MIDI buttons.
				if (smooth && std::fabs(valueFilters[id][c].out - value) < 1.f) {
					// Smooth value with filter
					valueFilters[id][c].process(args.sampleTime, value);
				}
				else {
					// Jump value
					valueFilters[id][c].out = value;
				}
				outputs[CC_OUTPUT + id].setVoltage(valueFilters[id][c].out * 10.f, c);
			}
		}
	}

	void processMessage(const midi::Message& msg) {
		switch (msg.getStatus()) {
			// cc
			case 0xb: {
				processCC(msg);
			} break;
			default: break;
		}
	}

	void processCC(const midi::Message& msg) {
		uint8_t c = mpeMode ? msg.getChannel() : 0;
		uint8_t cc = msg.getNote();
		if (msg.bytes.size() < 2)
			return;
		// Allow CC to be negative if the 8th bit is set.
		// The gamepad driver abuses this, for example.
		// Cast uint8_t to int8_t
		int8_t value = msg.bytes[2];
		// Learn
		if (learningId >= 0 && ccValues[cc][c] != value) {
			setLearnedCc(learningId, cc);
			learningId = -1;
		}

		if (lsbMode && cc < 32) {
			// Don't set MSB yet. Wait for LSB to be received.
			msbValues[cc][c] = value;
		}
		else if (lsbMode && 32 <= cc && cc < 64) {
			// Apply MSB when LSB is received
			ccValues[cc - 32][c] = msbValues[cc - 32][c];
			ccValues[cc][c] = value;
		}
		else {
			ccValues[cc][c] = value;
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
		for (int i = 0; i < 64; i++) {
			json_array_append_new(ccsJ, json_integer(learnedCcs[i]));
		}
		json_object_set_new(rootJ, "ccs", ccsJ);

		// Remember values so users don't have to touch MIDI controller knobs when restarting Rack
		json_t* valuesJ = json_array();
		for (int i = 0; i < 128; i++) {
			// Note: Only save channel 0. Since MPE mode won't be commonly used, it's pointless to save all 16 channels.
			json_array_append_new(valuesJ, json_integer(ccValues[i][0]));
		}
		json_object_set_new(rootJ, "values", valuesJ);

		json_object_set_new(rootJ, "midi", midiInput.toJson());

		json_object_set_new(rootJ, "smooth", json_boolean(smooth));
		json_object_set_new(rootJ, "mpeMode", json_boolean(mpeMode));
		json_object_set_new(rootJ, "lsbMode", json_boolean(lsbMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* ccsJ = json_object_get(rootJ, "ccs");
		if (ccsJ) {
			for (int i = 0; i < 64; i++) {
				json_t* ccJ = json_array_get(ccsJ, i);
				if (ccJ)
					setLearnedCc(i, json_integer_value(ccJ));
			}
		}

		json_t* valuesJ = json_object_get(rootJ, "values");
		if (valuesJ) {
			for (int i = 0; i < 128; i++) {
				json_t* valueJ = json_array_get(valuesJ, i);
				if (valueJ) {
					ccValues[i][0] = json_integer_value(valueJ);
				}
			}
		}

		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ)
			midiInput.fromJson(midiJ);

		json_t* smoothJ = json_object_get(rootJ, "smooth");
		if (smoothJ)
			smooth = json_boolean_value(smoothJ);

		json_t* mpeModeJ = json_object_get(rootJ, "mpeMode");
		if (mpeModeJ)
			mpeMode = json_boolean_value(mpeModeJ);

		json_t* lsbEnabledJ = json_object_get(rootJ, "lsbMode");
		if (lsbEnabledJ)
			lsbMode = json_boolean_value(lsbEnabledJ);
	}
};


struct MIDICC_CV_64Widget : ModuleWidget {
	MIDICC_CV_64Widget(MIDICC_CV_64* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MIDICCCV64.svg"), 
			asset::plugin(pluginInstance, "res/MIDICCCV64-dark.svg")));
		
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
			addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(x, y)), module, MIDICC_CV_64::CC_OUTPUT + port_num));
		    }
		}

		typedef Grid64MidiDisplay<CcChoice<MIDICC_CV_64>> TMidiDisplay;
		TMidiDisplay* display = createWidget<TMidiDisplay>(mm2px(Vec(0, 13.039)));
		display->box.size = mm2px(Vec(194, 55.88));
		display->setMidiPort(module ? &module->midiInput : NULL);
		display->setModule(module);
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		MIDICC_CV_64* module = dynamic_cast<MIDICC_CV_64*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Smooth CC", "", &module->smooth));

		menu->addChild(createBoolPtrMenuItem("MPE mode", "", &module->mpeMode));

		menu->addChild(createBoolPtrMenuItem("14-bit CC 0-31 / 32-63", "", &module->lsbMode));
	}
};


// Use legacy slug for compatibility
Model* modelMIDICCCV64 = createModel<MIDICC_CV_64, MIDICC_CV_64Widget>("MIDICCCV64");

