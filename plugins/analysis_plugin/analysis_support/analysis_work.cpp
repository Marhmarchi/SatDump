#include "analysis_work.h"
#include "common/dsp/buffer.h"
#include "common/dsp/filter/fir.h"
#include "common/dsp/utils/agc2.h"
#include "common/utils.h"
#include "core/module.h"
#include "logger.h"


#include "common/dsp/filter/firdes.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <bits/types/time_t.h>
#include <complex.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include <vector>
#include <volk/volk.h>
#include <volk/volk_complex.h>

#define BUFFER_SIZE 8192

namespace analysis_support
{
	AnalysisWork::AnalysisWork(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
		: ProcessingModule(input_file, output_file_hint, parameters),
		d_cutoff_freq(parameters["cutoff_freq"].get<double>()),
		d_transition_bw(parameters["transition_bw"].get<double>()),
		d_samplerate(parameters["samplerate"].get<long>())
	{
		input_buffer = new complex_t[BUFFER_SIZE];
		output_buffer = new complex_t[BUFFER_SIZE];
	}

	std::vector<ModuleDataType> AnalysisWork::getInputTypes()
	{
		return {DATA_FILE, DATA_STREAM};
	}

	std::vector<ModuleDataType> AnalysisWork::getOutputTypes()
	{
		return {DATA_FILE};
	}

	void AnalysisWork::process()
	{
		if (input_data_type == DATA_FILE)
			filesize = getFilesize(d_input_file);
		else
			filesize = 0;
		if (input_data_type == DATA_FILE)
		{
			data_out = std::ofstream(d_output_file_hint + ".F32", std::ios::binary);
			d_output_files.push_back(d_output_file_hint + ".F32");
		}

		logger->info("Using input baseband" + d_input_file);
		logger->info("Saving processed to " + d_output_file_hint + ".F32");

		time_t lastTime = 0;

		// Init stream source
		std::shared_ptr<dsp::stream<complex_t>> input_stream = std::make_shared<dsp::stream<complex_t>>();

		// Init blocks
		agc2 = std::make_shared<dsp::AGC2Block<complex_t>>(input_stream, 5.0, 0.01, 0.001);
		lpf = std::make_shared<dsp::FIRBlock<complex_t>>(agc2->output_stream, dsp::firdes::low_pass(1, d_samplerate, d_cutoff_freq, d_transition_bw));

		// Start everything
		agc2->start();
		lpf->start();

		while (input_data_type == DATA_FILE ? !data_in.eof() : input_active.load())
		{

			if (input_data_type == DATA_FILE)
				data_in.read((char *)input_buffer, BUFFER_SIZE * sizeof(complex_t));
			else
				input_fifo->read((uint8_t *)input_buffer, BUFFER_SIZE * sizeof(complex_t));

			volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)input_buffer, (lv_32fc_t *)input_buffer, 8192);


			if (input_data_type == DATA_FILE)
				progress = data_in.tellg();
			if (time(NULL) % 10 == 0 && lastTime != time(NULL))
			{
				lastTime = time(NULL);
				logger->info("Progress " + std::to_string(round(((double)progress / (double)filesize) * 1000.0) / 10.0) + "%%");
			}
		}
	};


			// Read a buffer
			// if (input_data_type == DATA_FILE)
			// data_in.read((char *)soft_buffer, BUFFER_SIZE * 2);
			// else
	}
}
