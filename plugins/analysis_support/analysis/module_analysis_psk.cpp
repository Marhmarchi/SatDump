#include "module_analysis_psk.h"
#include "common/dsp/buffer.h"
#include "common/dsp/fft/fft_pan.h"
#include "common/dsp/io/wav_writer.h"
#include "common/dsp/path/splitter.h"
#include "common/dsp/utils/real_to_complex.h"
#include "common/utils.h"
#include "common/widgets/fft_plot.h"
#include "core/module.h"
#include "imgui/imgui_markdown.h"
#include "logger.h"
#include "imgui/imgui.h"
#include "core/config.h"


#include "common/dsp/filter/firdes.h"
#include <complex.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <volk/volk.h>
#include <volk/volk_complex.h>

//#define BUFFER_SIZE 8192

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

		//exponent = satdump::config::main_cfg["user_interface"]["exponent"]["value"].get<int>();

		//enable_freq_scale;
		show_freq = true;
		
		MIN_SPS = 1;
		MAX_SPS = 1000.0;

	}

	void AnalysisPsk::init()
	{
		BaseDemodModule::initb();
//
//		int interpol = 2;
//
//		logger->critical("Final samplerate BEFORE everything: " + std::to_string(final_samplerate));
//		logger->critical("Normal samplerate BEFORE everything: " + std::to_string(d_samplerate));
//
//
//		// Resampler
//		res = std::make_shared<dsp::RationalResamplerBlock<complex_t>>(agc->output_stream, interpol, 1/*d_symbolrate, final_samplerate*/);
//
//		//final_samplerate = d_samplerate * interpol;
//		logger->critical("Final samplerate after res: " + std::to_string(final_samplerate));
//		logger->critical("Normal input samplerate after res : " + std::to_string(d_samplerate));
//
//
//		// Low pass filter
		lpf = std::make_shared<dsp::FIRBlock<complex_t>>(nullptr, dsp::firdes::low_pass(1.0, final_samplerate, d_cutoff_freq, d_transition_bw));
//
//		logger->critical("Final samplerate after lpf: " + std::to_string(final_samplerate));
//		logger->critical("Normal input samplerate after lpf : " + std::to_string(d_samplerate));
//
//
//		// Real To Complex
//		//rtc = std::make_shared<dsp::RealToComplexBlock>(res->output_stream);
//
//
//		//std::shared_ptr<dsp::stream<complex_t>> input_data_final = (d_frequency_shift != 0 ? freq_shift->output_stream : input_stream);
//
//		fft_splitter = std::make_shared<dsp::SplitterBlock>(lpf->output_stream);
//		fft_splitter->add_output("lowPassFilter");
//		fft_splitter->set_enabled("lowPassFilter", true);
//
//		fft_proc = std::make_shared<dsp::FFTPanBlock>(fft_splitter->get_output("lowPassFilter"));
//		fft_proc->set_fft_settings(32768, final_samplerate, 120);
//		//fft_proc->avg_num = 10;
//		//fft_plot->enable_freq_scale;
//		//fft_plot->bandwidth = final_samplerate;
//		fft_plot = std::make_shared<widgets::FFTPlot>(fft_proc->output_stream->writeBuf, 32768, -20, 10, 10);
//
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
		dsp::RationalResamplerBlock<complex_t> res(nullptr, 2, 1/*d_symbolrate, final_samplerate*/);
		//dsp::FIRBlock<complex_t> lpf(nullptr, dsp::firdes::low_pass(1.0, final_samplerate * 2, d_cutoff_freq, d_transition_bw));
		complex_t *work_buffer_in = dsp::create_volk_buffer<complex_t>(d_buffer_size);
		complex_t *work_buffer_out = dsp::create_volk_buffer<complex_t>(d_buffer_size);
		//res->start();
		lpf->start();
		//fft_splitter->start();
		//fft_proc->start();

		//Buffer
		complex_t *output_buffer = new complex_t[d_buffer_size * 200];
		//complex_t *input_buffer = new complex_t[d_buffer_size];
		complex_t *fc_buffer = new complex_t[d_buffer_size * 200];

		complex_t *exp_output = new complex_t[d_buffer_size * 200];

		complex_t *multConj_output = new complex_t[d_buffer_size * 200];
		float *compMag_output = new float[d_buffer_size * 100];

		complex_t *delayed_output = new complex_t[d_buffer_size * 200];
		complex_t *not_delayed_output = new complex_t[d_buffer_size * 200];

		complex_t last_samp = 0;

		//float *real_buffer = new float[d_buffer_size * 100];
		
		// Make it standard/easier while deving
		complex_t *fc32 = new complex_t[d_buffer_size * 200];
		float *f32_real = new float[d_buffer_size * 100];
		float *f32_imag = new float[d_buffer_size * 100];

		float *f32_delayed = new float[d_buffer_size * 100];
		float *f32_not_delayed = new float[d_buffer_size * 100];
		float f32_last_samp = 0;

		complex_t *fc32_delayed = new complex_t[d_buffer_size * 200];
		complex_t *fc32_not_delayed = new complex_t[d_buffer_size * 200];

		complex_t *fc32_mul_n_conj = new complex_t[d_buffer_size * 200];
		float *f32_mag = new float[d_buffer_size * 100];
		complex_t *fc32_mul_n_conj_offset = new complex_t[d_buffer_size * 200];



		int exponent = 2;

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

			int nout = res.process(lpf->output_stream->readBuf, dat_size, work_buffer_in);
			{
			volk_32fc_x2_multiply_32fc((lv_32fc_t *)exp_output, (lv_32fc_t *)work_buffer_out, (lv_32fc_t *)work_buffer_in, nout);
			}
			//nout = lpf.process(work_buffer_in, nout, work_buffer_out);


			//volk_32f_s32f_convert_16i(output_wav_buffer, (float *)lpf->output_stream->readBuf, 65535 * 0.68, dat_size * 2);

			// Complex to Float - Real and Imag
			//for (int i = 0; i < dat_size; i++)
			//{
			//	real[i] = lpf->output_stream->readBuf[i].real;
			//	imag[i] = lpf->output_stream->readBuf[i].imag;
			//}
			
			//real = lpf->output_stream->readBuf;
			//imag = lpf->output_stream->readBuf;


			// fc32 to f32 Real & f32 Imag
			//volk_32fc_deinterleave_32f_x2((float *)f32_real, (float *)f32_imag, (lv_32fc_t *)fc32, dat_size);



			//// f32 to fc32
			//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc_buffer, (float *)real, (float *)imag, dat_size/* * sizeof(complex_t)*/);
			//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc_buffer, (float *)lpf->output_stream->readBuf, (float *)lpf->output_stream->readBuf, dat_size/* * sizeof(complex_t)*/);


			// Exponentiate
			//volk_32fc_x2_multiply_32fc((lv_32fc_t *)exp_output, (lv_32fc_t *)lpf->output_stream->readBuf, (lv_32fc_t *)lpf->output_stream->readBuf, dat_size);
			//for (int i = 2; i < exponent; i++) {
			//	volk_32fc_x2_multiply_32fc((lv_32fc_t *)exp_output, (lv_32fc_t *)exp_output, (lv_32fc_t *)lpf->output_stream->readBuf, dat_size);
			//}


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





			// Multiply conjugate - Offset //
			//
			// Complex to Float
			//volk_32fc_deinterleave_32f_x2((float *)f32_real, (float *)f32_imag, (lv_32fc_t *)lpf->output_stream->readBuf, dat_size);
                        //
                        //
			//// Delay imaginary branch by 1 sample
			//for (int i = 0; i < dat_size; i++)
			//{
			//	f32_delayed[i] = i == 0 ? f32_last_samp : f32_imag[i - 1];
			//	//f32_non_delayed[i] = f32_imag[i];
			//}
                        //
			//f32_last_samp = f32_imag[dat_size - 1];
                        //
                        //
			//// Convert the delyed imag float and (non delayed) real float to complex 
			//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc32, (float *)f32_real, (float *)f32_delayed, dat_size);
                        //
			//// Now we perform the normal Multiply Conjugate with the offsetted input fc32
			////
			//// Duplicate and delay one stream
			//for (int i = 0; i < dat_size; i++)
			//{
			//	fc32_delayed[i] = i == 0 ? last_samp : fc32[i - 1];
			//	// Get not delayed stream to avoid desync as a sanity measure
			//	fc32_not_delayed[i] = fc32[i];
			//}
                        //
			//last_samp = fc32[dat_size - 1];
                        //
			//// Multiply conjugate
			//volk_32fc_x2_multiply_conjugate_32fc((lv_32fc_t *)fc32_mul_n_conj, (lv_32fc_t *)fc32_not_delayed, (lv_32fc_t *)fc32_delayed, dat_size);
                        //
			//// Complex to Mag
			//volk_32fc_magnitude_32f((float *)f32_mag, (lv_32fc_t *)fc32_mul_n_conj, dat_size / 2);
                        //
			//// Real to Complex
			//volk_32f_x2_interleave_32fc((lv_32fc_t *)fc32_mul_n_conj_offset, (float *)f32_mag, (float *)f32_mag, dat_size);
		







			if (output_data_type == DATA_FILE)
			{
				data_out.write((char *)exp_output, dat_size * sizeof(complex_t));
				//////data_out.write((char *)expTwo_output, dat_size * sizeof(complex_t));
				//data_out.write((char *)multConj_output, dat_size * sizeof(complex_t));
				//data_out.write((char *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				//logger->trace("%f", lpf->output_stream->readBuf);
				//final_data_size += dat_size * sizeof(int16_t);
				final_data_size += dat_size * sizeof(complex_t);
			}
			else 
			{
				//output_fifo->write((uint8_t *)output_buffer, dat_size * sizeof(complex_t));
				//////output_fifo->write((uint8_t *)expTwo_output, dat_size * sizeof(complex_t));
				//output_fifo->write((uint8_t *)multConj_output, dat_size * sizeof(complex_t));
				//output_fifo->write((uint8_t *)output_wav_buffer, dat_size * sizeof(int16_t) * 2);
				//logger->trace("%f", lpf->output_stream->readBuf);
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

		//delete[] output_buffer;

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
		//lpf->output_stream->stopReader();
		res->stop();
		lpf->stop();
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
            			    //int pos = (abs((float)d_frequency_shift / (float)final_samplerate * 2/*d_samplerate*/) * (float)8192) + 819;
            			    //pos %= 8192;
            			    //
            			    //// Compute min and max of the middle 80% of original baseband
            			    //float min = 1000;
            			    //float max = -1000;
            			    //for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    //{
            			    //    if (fft_proc->output_stream->writeBuf[pos] < min)
            			    //        min = fft_proc->output_stream->writeBuf[pos];
            			    //    if (fft_proc->output_stream->writeBuf[pos] > max)
            			    //        max = fft_proc->output_stream->writeBuf[pos];
            			    //    pos++;
            			    //    if (pos >= 8192)
            			    //        pos = 0;
            			    //}
				    ImGui::EndChild();
            			}
				ImGui::SameLine();


				if (ImGui::BeginChild("##MulConjOffset", ImVec2(ImGui::GetWindowWidth() / 2 * ui_scale, 400), false, ImGuiWindowFlags_None))
            			{
            			    fft_plot->draw({float(ImGui::GetWindowSize().x - 0), float(ImGui::GetWindowSize().y - 40 * ui_scale) * float(1.0)});
            			
            			    // Find "actual" left edge of FFT, before frequency shift.
            			    // Inset by 10% (819), then account for > 100% freq shifts via modulo
            			    //int pos = (abs((float)d_frequency_shift / (float)final_samplerate * 2/*d_samplerate*/) * (float)8192) + 819;
            			    //pos %= 8192;
            			
            			    //// Compute min and max of the middle 80% of original baseband
            			    //float min = 1000;
            			    //float max = -1000;
            			    //for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    //{
            			    //    if (fft_proc->output_stream->writeBuf[pos] < min)
            			    //        min = fft_proc->output_stream->writeBuf[pos];
            			    //    if (fft_proc->output_stream->writeBuf[pos] > max)
            			    //        max = fft_proc->output_stream->writeBuf[pos];
            			    //    pos++;
            			    //    if (pos >= 8192)
            			    //        pos = 0;
            			    //}
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
            			    //int pos = (abs((float)d_frequency_shift / (float)final_samplerate * 2/*d_samplerate*/) * (float)8192) + 819;
            			    //pos %= 8192;
            			
            			    //// Compute min and max of the middle 80% of original baseband
            			    //float min = 1000;
            			    //float max = -1000;
            			    //for (int i = 0; i < 6554; i++) // 8192 * 80% = 6554
            			    //{
            			    //    if (fft_proc->output_stream->writeBuf[pos] < min)
            			    //        min = fft_proc->output_stream->writeBuf[pos];
            			    //    if (fft_proc->output_stream->writeBuf[pos] > max)
            			    //        max = fft_proc->output_stream->writeBuf[pos];
            			    //    pos++;
            			    //    if (pos >= 8192)
            			    //        pos = 0;
            			    //}
				    ImGui::EndChild();
            			}

				ImGui::Button("Exponent number", {ImGui::GetWindowWidth() * ui_scale, 20 * ui_scale});
				enum exponent { exp_2, exp_4, exp_8, exp_16, exp_32, exp_64, expo_COUNT};
				static int expo = exp_2;
				const char* expon_numbers[expo_COUNT] = { "2", "4", "8", "16", "32", "64" };
				const char* expo_number = (expo >= 0 && expo < expo_COUNT) ? expon_numbers[expo] : "Unkown Exponent";
				ImGui::SliderInt("Exponent number", &expo, 0, expo_COUNT - 1, expo_number);

				//val = std::make_shared<int>(atoi(expo_number));

				//int exponent = atoi(expo_number);

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
