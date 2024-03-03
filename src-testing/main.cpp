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
<<<<<<< HEAD
#include <complex>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <memory>
#include <string>
#include <volk/volk.h>
#include <volk/volk_complex.h>
=======
#include "common/projection/projs2/proj.h"

>>>>>>> master
#include "common/image/image.h"
#include "common/dsp/complex.h"
#include "nlohmann/json.hpp"
#include "common/dsp/block.h"

<<<<<<< HEAD
#define BUFFER_SIZE 8192

=======
#include "common/map/map_drawer.h"

#include "common/image/image_meta.h"
#include "common/projection/projs2/proj_json.h"
>>>>>>> master

int main(int argc, char *argv[])
{
    initLogger();
    completeLoggerInit();

<<<<<<< HEAD
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
=======
#if 0
    proj::projection_t p;

    p.type = proj::ProjType_Geos;
    //   p.proj_offset_x = 418962.397137703;
    //   p.proj_offset_y = -101148.834767705;
    //   p.proj_scalar_x = 60.5849500687633;  // 1;
    //   p.proj_scalar_y = -60.5849500687633; // 1;
    //   p.lam0 = 2 * DEG2RAD;
    //   p.phi0 = 48 * DEG2RAD;
    p.params.altitude = 300000;
>>>>>>> master

    proj::projection_setup(&p);

    double x = 10000; // 15101; // 678108.04;
    double y = 10000; // 16495; // 5496954.89;
    double lon, lat;
    proj::projection_perform_inv(&p, x, y, &lon, &lat);

    logger->info("X %f - Y %f", x, y);
    logger->info("Lon %f - Lat %f", lon, lat);

    proj::projection_perform_fwd(&p, lon, lat, &x, &y);

    logger->info("X %f - Y %f", x, y);
#else

    image::Image<uint16_t> image_test;
    image_test.load_tiff(argv[1]);
    image_test.equalize();
    image_test.normalize();
    // image_test.mirror(false, true);

    logger->info("PROC DONE");

    if (!image::has_metadata(image_test))
    {
<<<<<<< HEAD
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
=======
        logger->error("Meta error!");
        return 0;
    }

    auto jsonp = image::get_metadata(image_test);
    logger->debug("\n%s", jsonp.dump(4).c_str());
    proj::projection_t p = jsonp["proj_cfg"];
    bool v = proj::projection_setup(&p);

    if (v)
    {
        logger->error("Proj error!");
        return 0;
    }

    {
        double x = 0; // 15101; // 678108.04;
        double y = 0; // 16495; // 5496954.89;
        double lon, lat;
        proj::projection_perform_inv(&p, x, y, &lon, &lat);

        logger->info("X %f - Y %f", x, y);
        logger->info("Lon %f - Lat %f", lon, lat);

        proj::projection_perform_fwd(&p, lon, lat, &x, &y);

        logger->info("X %f - Y %f", x, y);
    }

    {
        unsigned short color[4] = {0, 65535, 0, 65535};
        map::drawProjectedMapShapefile({"resources/maps/ne_10m_admin_0_countries.shp"},
                                       image_test,
                                       color,
                                       [&p](double lat, double lon, int, int) -> std::pair<int, int>
                                       {
                                           double x, y;
                                           proj::projection_perform_fwd(&p, lon, lat, &x, &y);
                                           return {(int)x, (int)y};
                                       });
    }

    image_test.save_tiff(argv[2]);

    proj::projection_free(&p);
#endif
}
>>>>>>> master
