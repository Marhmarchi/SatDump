#pragma once

#include "modules/demod/module_demod_base.h"
#include "nlohmann/json.hpp"
#include "common/dsp/filter/fir.h"
#include "common/dsp/resamp/rational_resampler.h"
#include "core/module.h"
#include "common/dsp/fft/fft_pan.h"
#include "common/widgets/fft_plot.h"
#include "common/dsp/path/splitter.h"
#include <memory>


namespace analysis
{
	class AnalysisPsk : public demod::BaseDemodModule
	{
		protected:
			//std::atomic<uint64_t> filesize;
			//std::atomic<uint64_t> progress;

			double d_cutoff_freq;
			double d_transition_bw;
			//long d_samplerate;

			std::shared_ptr<dsp::RationalResamplerBlock<complex_t>> res;
			std::shared_ptr<dsp::FIRBlock<complex_t>> lpf;
			//std::shared_ptr<dsp::AGC2Block<complex_t>> agc2;

			std::ifstream data_in;
			std::ofstream data_out;

			std::shared_ptr<dsp::SplitterBlock> fft_splitter;
			std::shared_ptr<dsp::FFTPanBlock> fft_proc;
			std::shared_ptr<widgets::FFTPlot> fft_plot;
			//bool fft_is_enabled = true;

			void drawAnaFFT();

			complex_t *input_buffer;
			complex_t *output_buffer;

		public:
			AnalysisPsk(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
			~AnalysisPsk();
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
