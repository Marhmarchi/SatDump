#pragma once

#include "common/dsp/filter/fir.h"
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


namespace analysis_support
{
	class AnalysisWork : public ProcessingModule
	{
		protected:
			std::atomic<uint64_t> filesize;
			std::atomic<uint64_t> progress;

			double d_cutoff_freq;
			double d_transition_bw;
			long d_samplerate;

			std::shared_ptr<dsp::FIRBlock<complex_t>> lpf;
			std::shared_ptr<dsp::AGC2Block<complex_t>> agc2;

			std::ifstream data_in;
			std::ofstream data_out;

		public:
			AnalysisWork(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
			~AnalysisWork();
			void process();
			void drawUI(bool window);
			std::vector<ModuleDataType> getInputTypes();
			std::vector<ModuleDataType> getOutputTypes();

		public:
			static std::string getID();
			virtual std::string getIDM() { return getID(); };
			static std::vector<std::string> getParameters();
			static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
    };
}
