#include "module_analysis_psk.h"
#include "common/dsp/io/wav_writer.h"
#include "common/utils.h"
#include "core/module.h"
#include "logger.h"
#include "imgui/imgui.h"

#include "common/dsp/filter/firdes.h"
#include <complex.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <volk/volk.h>
#include <volk/volk_complex.h>

#define BUFFER_SIZE 8192

namespace analysis
{
	AnalysisPsk::AnalysisPsk(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
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

	void AnalysisPsk::init()
	{
		BaseDemodModule::initb();

		// Resampler
		//res = std::make_shared<dsp::RationalResamplerBlock<complex_t>>(agc->output_stream, d_symbolrate, final_samplerate);

		// Low pass filter
		lpf = std::make_shared<dsp::FIRBlock<complex_t>>(agc->output_stream, dsp::firdes::low_pass(1.0, d_symbolrate, d_cutoff_freq, d_transition_bw));
	}

	AnalysisPsk::~AnalysisPsk()
	{
	}

	void AnalysisPsk::process()
	{
		if (input_data_type == DATA_FILE)
			filesize = file_source->getFilesize();
		else
			filesize = 0;

		if (output_data_type == DATA_FILE)
		{
			data_out = std::ofstream(d_output_file_hint + ".f32", std::ios::binary);
			d_output_files.push_back(d_output_file_hint + ".f32");
		}
	
		//std::ifstream data_in;

		//{
		//	data_out = std::ofstream(d_output_file_hint + ".f32", std::ios::binary);
		//	d_output_files.push_back(d_output_file_hint + ".f32");
		//}

		//else
		//	filesize = 0;

		//if (input_data_type == DATA_FILE)
		//{
		//}

		logger->info("Using input baseband" + d_input_file);
		logger->info("Saving processed to " + d_output_file_hint + ".f32");
		logger->info("Buffer size : " + std::to_string(d_buffer_size));

		time_t lastTime = 0;

		// Start
		BaseDemodModule::start();
		//res->start();
		lpf->start();
		//fft_splitter->start();
		//fft_proc->start();

		//Buffer
		complex_t *output_buffer = new complex_t[d_buffer_size * 100];
		//complex_t *input_buffer = new complex_t[d_buffer_size];
		//complex_t *fc_buffer = new complex_t[d_buffer_size * 100];

		//float *real_buffer = new float[d_buffer_size * 100];

		float *imag = new float[d_buffer_size * 100];
		float *real = new float[d_buffer_size * 100];

		//int16_t *output_wav_buffer = new int16_t[d_buffer_size * 100];
		int final_data_size = 0;
	//	dsp::WavWriter wave_writer(data_out);
	//	if (output_data_type == DATA_FILE)
	//		wave_writer.write_header(d_symbolrate, 2);
	
		int dat_size = 0;
		while (demod_should_run())
		{
			dat_size = lpf->output_stream->read();

			if (dat_size <= 0)
			{
				lpf->output_stream->flush();
				continue;
			}


			//volk_32f_s32f_convert_16i(output_wav_buffer, (float *)lpf->output_stream->readBuf, 65535 * 0.68, dat_size * 2);

			for (int i = 0; i < dat_size; i++)
			{
				imag[i] = lpf->output_stream->readBuf[i].imag;
				real[i] = lpf->output_stream->readBuf[i].real;
				//output_stream->writeBuf[i] = complex_t(real, imag);
			}
			
			volk_32f_x2_interleave_32fc((lv_32fc_t *)output_buffer, (float *)real, (float *)imag, dat_size * sizeof(complex_t));

			
			//for (int i = 0; i < dat_size; i++)
			//{
				//real_buffer = &lpf->output_stream->readBuf[i].real;

				//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc_buffer, &lpf->output_stream->readBuf[i].real, &lpf->output_stream->readBuf[i].imag, dat_size * sizeof(complex_t));
			//}

			//volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)lpf->output_stream->readBuf, (lv_32fc_t *)lpf->output_stream->readBuf, dat_size);


			if (output_data_type == DATA_FILE)
			{
				data_out.write((char *)output_buffer, dat_size * sizeof(complex_t));
				//data_out.write((char *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				//logger->trace("%f", lpf->output_stream->readBuf);
				//final_data_size += dat_size * sizeof(int16_t);
				final_data_size += dat_size * sizeof(complex_t);
			}
			else 
			{
				//output_fifo->write((uint8_t *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				output_fifo->write((uint8_t *)output_buffer, dat_size * sizeof(complex_t));
				logger->trace("%f", lpf->output_stream->readBuf);
			}

			lpf->output_stream->flush();

			//fft_splitter->start();
			//fft_proc->start();
			//fft_splitter->add_output("analysis_fft");
			//fft_splitter->set_enabled("analysis_fft", true);
			//fft_proc = std::make_unique<dsp::FFTPanBlock>(fft_splitter->get_output("analysis_fft"));
                        //
			//fft_proc->set_fft_settings(8192, d_symbolrate, 120);
			//fft_proc->avg_num = 50;
			//fft_plot = std::make_unique<widgets::FFTPlot>(fft_proc->output_stream->writeBuf, 8192, -10, 20, 10);
			//fft_proc->on_fft = [this](float *v){};

			if (input_data_type == DATA_FILE)
				progress = file_source->getPosition();

			if (time(NULL) % 10 == 0 && lastTime != time(NULL))
			{
				lastTime = time(NULL);
				logger->info("Progress " + std::to_string(round(((double)progress / (double)filesize) * 1000.0) / 10.0) + "%%");
			}
		}

		delete[] output_buffer;

		// Finish up WAV
	//	if (output_data_type == DATA_FILE)
	//		wave_writer.finish_header(final_data_size);
	//	delete[] output_wav_buffer;

		if (input_data_type == DATA_FILE)
			stop();
	}

	void AnalysisPsk::stop()
	{
		BaseDemodModule::stop();
		//res->stop();
		lpf->stop();
		lpf->output_stream->stopReader();
		//fft_proc->stop();
		//fft_splitter->stop();

		if (output_data_type == DATA_FILE)
			data_out.close();
	}
	
	void AnalysisPsk::drawUI(bool /*window*/)
	{
		ImGui::Begin("Analysis", NULL, ImGuiWindowFlags_NoScrollbar);

		ImGui::BeginGroup();
		{
			// Show SNR information
			//ImGui::Button("Signal", {200 * ui_scale, 20 * ui_scale});
			//snr_plot.draw(snr, peak_snr);
			if (!streamingInput)
				if (ImGui::Checkbox("Show FFT", &show_fft))
					fft_splitter->set_enabled("fft", show_fft);
		}
		ImGui::EndGroup();

		//ImGui::Begin("Analysis Plugin (very vey WIP)", NULL, window ? 0 : NOWINDOW_FLAGS);
		if (input_data_type == DATA_FILE)
			ImGui::ProgressBar((double)progress / (double)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));


		ImGui::BeginTabBar("##analysistabbar");
		{
			if (ImGui::BeginTabItem("Symbol Rate"))
			{
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Order"))
			{
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();

		drawStopButton();

		ImGui::End();

		drawFFT();
		//drawAnaFFT();
	}

	//void AnalysisPsk::drawAnaFFT()
	//{
	//	ImGui::SetNextWindowSize({400 * (float)ui_scale, (float)200 * (float)ui_scale});
	//	if (ImGui::Begin("Testing", NULL, ImGuiWindowFlags_NoScrollbar))
	//	{
	//		fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});

	//		int pos = (abs((float)d_frequency_shift / (float)d_samplerate) * (float)8192) + 819;
	//		pos %= 8192;

	//		float min = 1000;
	//		float max = -1000;
	//		for (int i = 0; i < 6554; i++)
	//		{
	//			if (fft_proc->output_stream->writeBuf[pos] < min)
	//				min = fft_proc->output_stream->writeBuf[pos];
	//			if (fft_proc->output_stream->writeBuf[pos] > max)
	//				max = fft_proc->output_stream->writeBuf[pos];
	//			pos++;
	//			if (pos >= 8192)
	//				pos = 0;
	//		}
	//	}
	//	ImGui::End();
	//}

	std::string AnalysisPsk::getID()
	{
		return "analysis_psk";
	}

	std::vector<std::string> AnalysisPsk::getParameters()
	{
		return {"cutoff_freq", "transition_bw"};
	}

	std::shared_ptr<ProcessingModule> AnalysisPsk::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
	{
		return std::make_shared<AnalysisPsk>(input_file, output_file_hint, parameters);
	}

}
