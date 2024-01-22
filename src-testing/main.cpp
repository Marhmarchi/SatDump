/**********************************************************************
 * This file is used for testing random stuff without running the
 * whole of SatDump, which comes in handy for debugging individual
 * elements before putting them all together in modules...
 *
 * If you are an user, ignore this file which will not be built by
 * default, and if you're a developper in need of doing stuff here...
 * Go ahead!
 *
 * Don't judge the code you might see in there! :)
 **********************************************************************/

#include "common/cli_utils.h"
#include "common/dsp/buffer.h"
#include "common/dsp/filter/fir.h"
#include "common/dsp/filter/firdes.h"
#include "logger.h"
#include <complex>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <memory>
#include <string>
#include <volk/volk.h>
#include <volk/volk_complex.h>
#include "common/image/image.h"
#include "common/dsp/complex.h"
#include "nlohmann/json.hpp"
#include "common/dsp/block.h"

#define BUFFER_SIZE 8192


int main(int argc, char *argv[])
{
    initLogger();
    completeLoggerInit();

    std::ifstream data_in(argv[1], std::ios::binary);
    std::ofstream data_out(argv[2], std::ios::binary);

    size_t max_buffer_size;
    max_buffer_size = dsp::STREAM_BUFFER_SIZE;


    complex_t input_buffer[BUFFER_SIZE];
    complex_t output_buffer[BUFFER_SIZE];

    complex_t delayed_output[BUFFER_SIZE];
    complex_t not_delayed_output[BUFFER_SIZE];


    complex_t last_samp = 0;

    nlohmann::json parameters = parse_common_flags(argc - 3, &argv[3]);

    if (parameters.contains("multiply_conjugate"))
    {
	    logger->info("Multiply conjugate selected!");
    }
    if (parameters.contains("exponentiate"))
    {
	    logger->info("Exponentiate selected!");
	    uint8_t exponent;
	    try {
		    exponent = parameters["exponentiate"].get<uint8_t>();
	    }
	    catch (std::exception &e)
	    {
		    logger->error("Error parsing argument! %s", e.what());
	    }
	    if (exponent <= 1)
	    {
		    logger->error("Exponent number must be > 1!!");
	    }
    }
    if (parameters.contains("LPF"))
    {
	    logger->info("Low pass filter selected!");
    }

    while (!data_in.eof())
    {
	    data_in.read((char *)input_buffer, BUFFER_SIZE * sizeof(complex_t));


	    //if (std::string(argv[3]) == "multiply_conjugate")
	    if (parameters.contains("multiply_conjugate"))
	    {

		    // Delay 1 sample
		    for (int i = 0; i < BUFFER_SIZE; i++)
	    	    {
	    	            delayed_output[i] = i == 0 ? last_samp : input_buffer[i - 1];
	    	            not_delayed_output[i] = input_buffer[i];
	    	    }
            	    
	    	    last_samp = input_buffer[BUFFER_SIZE - 1];
            	    
		    // Multiply conjugate
	    	    volk_32fc_x2_multiply_conjugate_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)not_delayed_output, (lv_32fc_t *)delayed_output, BUFFER_SIZE);
	    }
	    //data_out.write((char *)output_buffer, BUFFER_SIZE * sizeof(complex_t));

	    //else if (std::string(argv[3]) == "exponential")
	    if (parameters.contains("exponentiate"))
	    {
		    uint8_t exponent;

		    try {
		    exponent = parameters["exponentiate"].get<uint8_t>();
		    }
		    catch (std::exception &e)
		    {
			    logger->error("Error parsing argument! %s", e.what());
		    }
			    volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)input_buffer, (lv_32fc_t *)input_buffer, BUFFER_SIZE);
			    for (int i = 2; i < exponent; i++) {
				    volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)output_buffer, (lv_32fc_t *)input_buffer, BUFFER_SIZE);
			    }
	    }

	    if (parameters.contains("LPF"))
	    {
//		    std::shared_ptr<dsp::FIRBlock<complex_t>> lpf;
//		    //std::shared_ptr<dsp::FIRBlock<complex_t>> lpf;
//		    //lpf = std::make_shared<dsp::FIRBlock<complex_t>>(input_buffer, dsp::firdes::low_pass(1.0, 8000000, 200000, 2000));
//		    //lpf = std::make_shared<dsp::FIRBlock<std::complex_t>>(output_stream, dsp::firdes::low_pass(1.0, 8000000, 200000, 2000));
//
//		    lpf = std::make_shared<dsp::FIRBlock<complex_t>>(output_buffer, dsp::firdes::low_pass(1.0, 8000000, 50000, 100));
//		    lpf->input_stream->read();
		    
	    }

	    data_out.write((char *)output_buffer, BUFFER_SIZE * sizeof(complex_t));
    }
}
