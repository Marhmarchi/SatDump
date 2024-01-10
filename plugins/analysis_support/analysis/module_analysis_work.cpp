#include "module_analysis_work.h"
#include "common/dsp/buffer.h"
#include "common/dsp/filter/fir.h"
#include "common/dsp/resamp/rational_resampler.h"
#include "common/dsp/utils/agc2.h"
#include "common/utils.h"
#include "core/module.h"
#include "imgui/imgui_flags.h"
#include "logger.h"
#include "imgui/imgui.h"


#include "common/dsp/filter/firdes.h"
#include "modules/demod/module_demod_base.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <bits/types/time_t.h>
#include <complex.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <volk/volk.h>
#include <volk/volk_complex.h>

#define BUFFER_SIZE 8192

namespace analysis
{
	AnalysisWork::AnalysisWork(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
		: BaseDemodModule(input_file, output_file_hint, parameters),
		d_cutoff_freq(parameters["cutoff_freq"].get<double>()),
		d_transition_bw(parameters["transition_bw"].get<double>())
		//d_samplerate(parameters["samplerate"].get<long>())
	{
		show_freq = true;
		//input_buffer = new complex_t[BUFFER_SIZE];
		//output_buffer = new complex_t[BUFFER_SIZE];
		
		MIN_SPS = 1;
		MAX_SPS = 1000.0;
	}

	void AnalysisWork::init()
	{
		BaseDemodModule::initb();

		// Resampler
		res = std::make_shared<dsp::RationalResamplerBlock<complex_t>>(agc->output_stream, d_symbolrate, final_samplerate);

		// Low pass filter
		lpf = std::make_shared<dsp::FIRBlock<complex_t>>(agc->output_stream, dsp::firdes::low_pass(1.0, final_samplerate, d_cutoff_freq, d_transition_bw));
	}

	AnalysisWork::~AnalysisWork()
	{
	}

	void AnalysisWork::process()
	{
		if (input_data_type == DATA_FILE)
			filesize = file_source->getFilesize();
		else
			filesize = 0;

		if (input_data_type == DATA_FILE)
		{
			data_out = std::ofstream(d_output_file_hint + ".F32", std::ios::binary);
			d_output_files.push_back(d_output_file_hint + ".F32");
		}

		logger->info("Using input baseband" + d_input_file);
		logger->info("Saving processed to " + d_output_file_hint + ".F32");
		logger->info("Buffer size : " + std::to_string(d_buffer_size));

		time_t lastTime = 0;

		// Start
		BaseDemodModule::start();
		res->start();
		lpf->start();

		//Buffer
		complex_t *output_buffer = new complex_t[d_buffer_size];
		int final_data_size = 0;

		int dat_size = 0;
		while (demod_should_run())
		{
			dat_size = lpf->output_stream->read();

			if (dat_size <= 0)
			{
				lpf->output_stream->flush();
				continue;
			}

			for (int i = 0; i < dat_size; i++)
			{
				lpf->output_stream->readBuf[i];
			}

			volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)lpf->output_stream->readBuf, (lv_32fc_t *)lpf->output_stream->readBuf, dat_size);

			if (output_data_type == DATA_FILE)
			{
				data_out.write((char *)output_buffer, dat_size * sizeof(complex_t));
				final_data_size += dat_size * sizeof(complex_t);
			}
			else 
			{
				output_fifo->write((uint8_t *)output_buffer, dat_size * sizeof(complex_t));
			}

			lpf->output_stream->flush();

			if (input_data_type == DATA_FILE)
				progress = file_source->getPosition();

			if (time(NULL) % 10 == 0 && lastTime != time(NULL))
			{
				lastTime = time(NULL);
				logger->info("Progress " + std::to_string(round(((double)progress / (double)filesize) * 1000.0) / 10.0) + "%%");
			}
		}

		delete[] output_buffer;

		if (input_data_type == DATA_FILE)
			stop();
	}

	void AnalysisWork::stop()
	{
		BaseDemodModule::stop();
		res->start();
		lpf->stop();

		if (output_data_type == DATA_FILE)
			data_out.close();
	}
	
	void AnalysisWork::drawUI(bool window)
	{
		ImGui::Begin(name.c_str(), NULL, window ? 0 : NOWINDOW_FLAGS);

		ImGui::BeginGroup();
		{
			// Show SNR information
			ImGui::Button("Signal", {200 * ui_scale, 20 * ui_scale});
			snr_plot.draw(snr, peak_snr);
			if (!streamingInput)
				if (ImGui::Checkbox("Show FFT", &show_fft))
					fft_splitter->set_enabled("fft", show_fft);
		}
		ImGui::EndGroup();

		//ImGui::Begin("Analysis Plugin (very vey WIP)", NULL, window ? 0 : NOWINDOW_FLAGS);
		if (input_data_type == DATA_FILE)
			ImGui::ProgressBar((double)progress / (double)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

		drawStopButton();

		ImGui::End();

		drawFFT();
	}

	std::string AnalysisWork::getID()
	{
		return "analysis_work";
	}

	std::vector<std::string> AnalysisWork::getParameters()
	{
		return {"cutoff_freq", "transition_bw"};
	}

	std::shared_ptr<ProcessingModule> AnalysisWork::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
	{
		return std::make_shared<AnalysisWork>(input_file, output_file_hint, parameters);
	}

}
