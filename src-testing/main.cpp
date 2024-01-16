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

#include "common/dsp/buffer.h"
#include "logger.h"
#include <cstdint>
#include <fstream>
#include <cstring>
#include <volk/volk.h>
#include <volk/volk_complex.h>
#include "common/image/image.h"
#include "common/dsp/complex.h"

#define BUFFER_SIZE 8192

int main(int /*argc*/, char *argv[])
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


    while (!data_in.eof())
    {
	    data_in.read((char *)input_buffer, BUFFER_SIZE * sizeof(complex_t));

	    for (int i = 0; i < BUFFER_SIZE; i++)
	    {
		    delayed_output[i] = i == 0 ? last_samp : input_buffer[i - 1];
		    not_delayed_output[i] = input_buffer[i];
	    }

	    last_samp = input_buffer[BUFFER_SIZE - 1];

	    volk_32fc_x2_multiply_conjugate_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)not_delayed_output, (lv_32fc_t *)delayed_output, BUFFER_SIZE);


	    //volk_32fc_x2_multiply_32fc((lv_32fc_t *)output_buffer, (lv_32fc_t *)input_buffer, (lv_32fc_t *)input_buffer, BUFFER_SIZE);

	    data_out.write((char *)output_buffer, BUFFER_SIZE * sizeof(complex_t));
    }
}
