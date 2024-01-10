#pragma once

#include "common/dsp/filter/fir.h"
#include "common/dsp/resamp/rational_resampler.h"
#include "common/dsp/utils/agc2.h"
#include "core/module.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <complex.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "modules/demod/module_demod_base.h"


namespace analysis
{
	class AnalysisWork : public demod::BaseDemodModule
	{
		protected:
			//std::atomic<uint64_t> filesize;
			//std::atomic<uint64_t> progress;

			double d_cutoff_freq;
			double d_transition_bw;
			//long d_samplerate;

			//std::shared_ptr<dsp::RationalResamplerBlock<complex_t>> res;
			std::shared_ptr<dsp::FIRBlock<complex_t>> lpf;
			//std::shared_ptr<dsp::AGC2Block<complex_t>> agc2;

			std::ifstream data_in;
			std::ofstream data_out;

			complex_t *input_buffer;
			complex_t *output_buffer;

		public:
			AnalysisWork(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
			~AnalysisWork();
			void init();
			void stop();
			void process();
			
			void drawUI(bool window);
			//std::vector<ModuleDataType> getInputTypes();
			//std::vector<ModuleDataType> getOutputTypes();

		public:
			static std::string getID();
			virtual std::string getIDM() { return getID(); };
			static std::vector<std::string> getParameters();
			static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
    };
}
