#include "module_analysis_psk.h"
#include "common/dsp/fft/fft_pan.h"
#include "common/dsp/io/wav_writer.h"
#include "common/dsp/path/splitter.h"
#include "common/utils.h"
#include "common/widgets/fft_plot.h"
#include "core/module.h"
#include "imgui/imgui_markdown.h"
#include "logger.h"
#include "imgui/imgui.h"

#include "common/dsp/filter/firdes.h"
#include <complex.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <volk/volk.h>
#include <volk/volk_complex.h>

#define BUFFER_SIZE 8192

namespace analysis
{
	AnalysisPsk::AnalysisPsk(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
		: BaseDemodModule(input_file, output_file_hint, parameters)//,
		//d_cutoff_freq(parameters["cutoff_freq"].get<double>()),
		//d_transition_bw(parameters["transition_bw"].get<double>())
		//d_samplerate(parameters["samplerate"].get<long>())
	{
		if (parameters.count("cutoff_freq") >0)
			d_cutoff_freq = parameters["cutoff_freq"].get<double>();
		else
			throw std::runtime_error("Cutoff frequency must be present!");

		if (parameters.count("transition_bw") >0)
			d_transition_bw = parameters["transition_bw"].get<double>();
		else
			throw std::runtime_error("Transition bandwidth must be present!");


		show_freq = true;
		
		MIN_SPS = 1;
		MAX_SPS = 1000.0;

	}

	void AnalysisPsk::init()
	{
		BaseDemodModule::initb();

		// Low pass filter
		lpf = std::make_shared<dsp::FIRBlock<complex_t>>(agc->output_stream, dsp::firdes::low_pass(1.0, d_symbolrate, d_cutoff_freq, d_transition_bw));
		// Resampler
		res = std::make_shared<dsp::RationalResamplerBlock<complex_t>>(lpf->output_stream, 2, 1/*d_symbolrate, final_samplerate*/);


		//std::shared_ptr<dsp::stream<complex_t>> input_data_final = (d_frequency_shift != 0 ? freq_shift->output_stream : input_stream);

		fft_splitter = std::make_shared<dsp::SplitterBlock>(res->output_stream);
		fft_splitter->add_output("lowPassFilter");
		fft_splitter->set_enabled("lowPassFilter", true);

		fft_proc = std::make_shared<dsp::FFTPanBlock>(fft_splitter->get_output("lowPassFilter"));
		fft_proc->set_fft_settings(8192, final_samplerate, 120);
		fft_proc->avg_num = 10;
		fft_plot->enable_freq_scale;
		fft_plot = std::make_shared<widgets::FFTPlot>(fft_proc->output_stream->writeBuf, 8192, -10, 10, 10);

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
	
		logger->info("Using input baseband" + d_input_file);
		logger->info("Saving processed to " + d_output_file_hint + ".f32");
		logger->info("Buffer size : " + std::to_string(d_buffer_size));

		time_t lastTime = 0;

		// Start
		BaseDemodModule::start();
		res->start();
		lpf->start();
		fft_splitter->start();
		fft_proc->start();

		//Buffer
		complex_t *output_buffer = new complex_t[d_buffer_size * 200];
		//complex_t *input_buffer = new complex_t[d_buffer_size];
		complex_t *fc_buffer = new complex_t[d_buffer_size * 200];

		complex_t *expTwo_output = new complex_t[d_buffer_size * 200];
		complex_t *expFour_output = new complex_t[d_buffer_size * 200];
		complex_t *expEight_output = new complex_t[d_buffer_size * 200];

		complex_t *multConj_output = new complex_t[d_buffer_size * 200];
		float *compMag_output = new float[d_buffer_size * 100];

		complex_t *delayed_output = new complex_t[d_buffer_size * 200];
		complex_t *not_delayed_output = new complex_t[d_buffer_size * 200];

		complex_t last_samp = 0;

		//float *real_buffer = new float[d_buffer_size * 100];

		float *imag = new float[d_buffer_size * 100];
		float *real = new float[d_buffer_size * 100];

		int exponent = 2;

		//int16_t *output_wav_buffer = new int16_t[d_buffer_size * 100];
		int final_data_size = 0;
	//	dsp::WavWriter wave_writer(data_out);
	//	if (output_data_type == DATA_FILE)
	//		wave_writer.write_header(d_symbolrate, 2);
	
		int dat_size = 0;
		while (demod_should_run())
		{
			dat_size = res->output_stream->read();

			if (dat_size <= 0)
			{
				res->output_stream->flush();
				continue;
			}


			//volk_32f_s32f_convert_16i(output_wav_buffer, (float *)lpf->output_stream->readBuf, 65535 * 0.68, dat_size * 2);

			//for (int i = 0; i < dat_size; i++)
			//{
			//	imag[i] = lpf->output_stream->readBuf[i].imag;
			//	real[i] = lpf->output_stream->readBuf[i].real;
			//}
			//
			//// F32 to CF32
			//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc_buffer, (float *)real, (float *)imag, dat_size/* * sizeof(complex_t)*/);

			// Exponentiate
			volk_32fc_x2_multiply_32fc((lv_32fc_t *)expTwo_output, (lv_32fc_t *)fc_buffer, (lv_32fc_t *)fc_buffer, dat_size);
			for (int i = 2; i < exponent; i++) {
				volk_32fc_x2_multiply_32fc((lv_32fc_t *)expTwo_output, (lv_32fc_t *)fc_buffer, (lv_32fc_t *)fc_buffer, dat_size);
			}


			// Delay 1 sample
			//for (int i = 0; i < dat_size; i++)
			//{
			//	delayed_output[i] = i == 0 ? last_samp : fc_buffer[i - 1];
			//	not_delayed_output[i] = fc_buffer[i];
			//}

			//last_samp = fc_buffer[dat_size - 1];

			// Multiply conjugate
			//volk_32fc_x2_multiply_conjugate_32fc((lv_32fc_t *)multConj_output, (lv_32fc_t *)not_delayed_output, (lv_32fc_t *)delayed_output, dat_size);
			// Complex to Mag
			//volk_32fc_magnitude_32f((float *)compMag_output, (lv_32fc_t *)multConj_output, dat_size);



			if (output_data_type == DATA_FILE)
			{
				//data_out.write((char *)output_buffer, dat_size * sizeof(complex_t));
				//////data_out.write((char *)expTwo_output, dat_size * sizeof(complex_t));
				//data_out.write((char *)multConj_output, dat_size * sizeof(complex_t));
				//data_out.write((char *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				logger->trace("%f", res->output_stream->readBuf);
				//final_data_size += dat_size * sizeof(int16_t);
				final_data_size += dat_size * sizeof(complex_t);
			}
			else 
			{
				//output_fifo->write((uint8_t *)output_buffer, dat_size * sizeof(complex_t));
				//////output_fifo->write((uint8_t *)expTwo_output, dat_size * sizeof(complex_t));
				//output_fifo->write((uint8_t *)multConj_output, dat_size * sizeof(complex_t));
				//output_fifo->write((uint8_t *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				logger->trace("%f", res->output_stream->readBuf);
			}

			res->output_stream->flush();

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
		res->stop();
		lpf->stop();
		lpf->output_stream->stopReader();
		fft_splitter->stop();
		fft_proc->stop();

		if (output_data_type == DATA_FILE)
			data_out.close();
	}
	
	void AnalysisPsk::drawUI(bool /*window*/)
	{
		ImGui::Begin("Analysis Module", NULL, ImGuiWindowFlags_NoScrollbar);
		ImGui::SetWindowSize(ImVec2(800,700), ImGuiCond_FirstUseEver);
		
		ImGui::BeginTabBar("##analysistabbar");
		{
			if (ImGui::BeginTabItem("Symbol Rate"))
			{

				ImGui::BeginGroup();

				ImGui::Button("Multiply Conjugate", {ImGui::GetWindowWidth() / 2 * ui_scale, 20 * ui_scale});
				ImGui::SameLine();
				ImGui::Button("Offset Multiply Conjugate", {ImGui::GetWindowWidth() / 2 * ui_scale, 20 * ui_scale});

				if (ImGui::BeginChild("##MulConj", ImVec2(ImGui::GetWindowWidth() / 2 * ui_scale, 400), false, ImGuiWindowFlags_None))
            			{
            			    fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});
            			
            			    // Find "actual" left edge of FFT, before frequency shift.
            			    // Inset by 10% (819), then account for > 100% freq shifts via modulo
            			    int pos = (abs((float)d_frequency_shift / (float)final_samplerate/*d_samplerate*/) * (float)8192) + 819;
            			    pos %= 8192;
            			
            			    // Compute min and max of the middle 80% of original baseband
            			    float min = 1000;
            			    float max = -1000;
            			    for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    {
            			        if (fft_proc->output_stream->writeBuf[pos] < min)
            			            min = fft_proc->output_stream->writeBuf[pos];
            			        if (fft_proc->output_stream->writeBuf[pos] > max)
            			            max = fft_proc->output_stream->writeBuf[pos];
            			        pos++;
            			        if (pos >= 8192)
            			            pos = 0;
            			    }
				    ImGui::EndChild();
            			}
				ImGui::SameLine();


				if (ImGui::BeginChild("##MulConjOffset", ImVec2(ImGui::GetWindowWidth() / 2 * ui_scale, 400), false, ImGuiWindowFlags_None))
            			{
            			    fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});
            			
            			    // Find "actual" left edge of FFT, before frequency shift.
            			    // Inset by 10% (819), then account for > 100% freq shifts via modulo
            			    int pos = (abs((float)d_frequency_shift / (float)final_samplerate/*d_samplerate*/) * (float)8192) + 819;
            			    pos %= 8192;
            			
            			    // Compute min and max of the middle 80% of original baseband
            			    float min = 1000;
            			    float max = -1000;
            			    for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    {
            			        if (fft_proc->output_stream->writeBuf[pos] < min)
            			            min = fft_proc->output_stream->writeBuf[pos];
            			        if (fft_proc->output_stream->writeBuf[pos] > max)
            			            max = fft_proc->output_stream->writeBuf[pos];
            			        pos++;
            			        if (pos >= 8192)
            			            pos = 0;
            			    }
				    ImGui::EndChild();
            			}

				ImGui::EndGroup();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Modulation"))
			{


				ImGui::BeginGroup();

				ImGui::Button("Exponentiate", {ImGui::GetWindowWidth() * ui_scale, 20 * ui_scale});
				if (ImGui::BeginChild("##Exponen", ImVec2(ImGui::GetWindowWidth() * ui_scale, 400), false, ImGuiWindowFlags_None))
            			{
            			    fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});
            			
            			    // Find "actual" left edge of FFT, before frequency shift.
            			    // Inset by 10% (819), then account for > 100% freq shifts via modulo
            			    int pos = (abs((float)d_frequency_shift / (float)final_samplerate/*d_samplerate*/) * (float)8192) + 819;
            			    pos %= 8192;
            			
            			    // Compute min and max of the middle 80% of original baseband
            			    float min = 1000;
            			    float max = -1000;
            			    for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    {
            			        if (fft_proc->output_stream->writeBuf[pos] < min)
            			            min = fft_proc->output_stream->writeBuf[pos];
            			        if (fft_proc->output_stream->writeBuf[pos] > max)
            			            max = fft_proc->output_stream->writeBuf[pos];
            			        pos++;
            			        if (pos >= 8192)
            			            pos = 0;
            			    }
				    ImGui::EndChild();
            			}

				ImGui::Button("Exponent number", {ImGui::GetWindowWidth() * ui_scale, 20 * ui_scale});
				enum exponent { exp_2, exp_4, exp_8, exp_16, exp_32, exp_64, expo_COUNT};
				static int expo = exp_2;
				const char* expon_numbers[expo_COUNT] = { "2", "4", "8", "16", "32", "64" };
				const char* expo_number = (expo >= 0 && expo < expo_COUNT) ? expon_numbers[expo] : "Unkown Exponent";
				ImGui::SliderInt("Exponent number", &expo, 0, expo_COUNT - 1, expo_number);

				//val = std::make_shared<int>(atoi(expo_number));

				int val = atoi(expo_number);

				//std::stringstream expo_string;
				//expo_string << expo_number;

				//int expo_value;
				//expo_string >> expo_value;


				ImGui::EndGroup();
				ImGui::EndTabItem();
			}
		}

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


		ImGui::EndTabBar();

		drawStopButton();

		//drawAnaFFT();
		ImGui::End();

		drawFFT();
	}

	
   // void AnalysisPsk::drawAnaFFT()
   //     {
   //         ImGui::SetNextWindowSize({400 * (float)ui_scale, (float)(400) * (float)ui_scale});
   //         if (ImGui::Begin("Baseband FFT", NULL, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
   //         {
   //             fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});

   //             // Find "actual" left edge of FFT, before frequency shift.
   //             // Inset by 10% (819), then account for > 100% freq shifts via modulo
   //             int pos = (abs((float)d_frequency_shift / (float)final_samplerate/*d_samplerate*/) * (float)8192) + 819;
   //             pos %= 8192;

   //             // Compute min and max of the middle 80% of original baseband
   //             float min = 1000;
   //             float max = -1000;
   //             for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
   //             {
   //                 if (fft_proc->output_stream->writeBuf[pos] < min)
   //                     min = fft_proc->output_stream->writeBuf[pos];
   //                 if (fft_proc->output_stream->writeBuf[pos] > max)
   //                     max = fft_proc->output_stream->writeBuf[pos];
   //                 pos++;
   //                 if (pos >= 8192)
   //                     pos = 0;
   //             }

   //         }

   //         ImGui::End();
   //     }



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
