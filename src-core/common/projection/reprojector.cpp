#include "reprojector.h"
#include "logger.h"
#include "warp/warp.h"
#include "gcp_compute/gcp_compute.h"

#include "sat_proj/sat_proj.h"

// #include "projs/equirectangular.h"
// #include "projs/mercator.h"
// #include "projs/stereo.h"
// #include "projs/tpers.h"
// #include "projs/azimuthal_equidistant.h"

// #include "sat_proj/geo_projection.h"
#include "projs/tps_transform.h"
// #include "reproj/reproj.h"

#include "common/projection/projs2/proj_json.h"
#include "common/image/image_meta.h"

namespace satdump
{
    namespace reprojection
    {
        inline void transposePixel(image::Image<uint16_t> &in, image::Image<uint16_t> &out, int ix, int iy, int ox, int oy)
        {
            if (ix >= (int)in.width() || iy >= (int)in.height() || ix < 0 || iy < 0)
                return;
            if (ox >= (int)out.width() || oy >= (int)out.height() || ox < 0 || oy < 0)
                return;

            if (in.channels() == 4)
            {
                for (int c = 0; c < in.channels(); c++)
                    out.channel(c)[oy * out.width() + ox] = in.channel(c)[iy * in.width() + ix];
            }
            else if (in.channels() == 3)
            {
                for (int c = 0; c < in.channels(); c++)
                    out.channel(c)[oy * out.width() + ox] = c == 3 ? 65535 : in.channel(c)[iy * in.width() + ix];
                if (out.channels() == 4)
                    out.channel(3)[oy * out.width() + ox] = 65535;
            }
            else if (in.channels() == 1) //|| in.channels() == 2)
            {
                for (int c = 0; c < out.channels(); c++)
                    out.channel(c)[oy * out.width() + ox] = in.channel(0)[iy * in.width() + ix];
                if (out.channels() == 4)
                    out.channel(3)[oy * out.width() + ox] = 65535;
            }
            else
            {
                for (int c = 0; c < in.channels(); c++)
                    out.channel(c)[oy * out.width() + ox] = c == 3 ? 65535 : in.channel(0)[iy * in.width() + ix];
                if (out.channels() == 4)
                    out.channel(3)[oy * out.width() + ox] = 65535;
            }
        }

        image::Image<uint16_t> reproject(ReprojectionOperation &op, float *progress)
        {
            image::Image<uint16_t> result_img;

            if (op.img.size() == 0)
                throw std::runtime_error("Can't reproject an empty image!");
            if (!image::has_metadata_proj_cfg(op.img))
                throw std::runtime_error("Can't reproject an image with no proj config!");

            result_img.init(op.output_width, op.output_height, 4);

            // Attempt to init target proj
            proj::projection_t trg_proj;
            bool trg_proj_err = false;
            try
            {
                trg_proj = op.target_prj_info;
            }
            catch (std::exception &e)
            {
                trg_proj_err = true;
            }

            if (proj::projection_setup(&trg_proj) || trg_proj_err)
            {
                logger->error("Invalid target projection? Could not init! %d : \n%s", (int)trg_proj_err, op.target_prj_info.dump(4).c_str());
                return result_img;
            }

            // Attempt to init source proj
            proj::projection_t src_proj;
            bool src_proj_err = false;
            try
            {
                src_proj = image::get_metadata_proj_cfg(op.img);
            }
            catch (std::exception &e)
            {
                src_proj_err = true;
            }

            if (!proj::projection_setup(&src_proj) && !src_proj_err)
            { // If the input is a standard projection
                for (int x = 0; x < (int)result_img.width(); x++)
                {
                    for (int y = 0; y < (int)result_img.height(); y++)
                    {
                        double lon, lat, x2, y2; // To allow OMP...
                        if (proj::projection_perform_inv(&trg_proj, x, y, &lon, &lat))
                            continue;
                        if (proj::projection_perform_fwd(&src_proj, lon, lat, &x2, &y2))
                            continue;
                        transposePixel(op.img, result_img, x2, y2, x, y);
                    }
                    if (progress != nullptr)
                        *progress = float(x) / float(result_img.width());
                }

                proj::projection_free(&src_proj);
            }
            else
            { // Otherwise we assume it to be a satellite warp
                if (op.use_old_algorithm)
                { // This is garbage, but robust to noise garbage!
                    logger->info("Using old algorithm...!");

                    auto src_proj_cfg = image::get_metadata_proj_cfg(op.img);

                    // Init
                    std::function<std::pair<int, int>(float, float, int, int)> projectionFunction = setupProjectionFunction(result_img.width(),
                                                                                                                            result_img.height(),
                                                                                                                            op.target_prj_info,
                                                                                                                            {});

                    nlohmann::json mtd = src_proj_cfg.contains("metadata") ? src_proj_cfg["metadata"] : nlohmann::json();
                    int img_x_offset = 0;
                    if (mtd.contains("img_offset_x"))
                        img_x_offset = mtd["img_offset_x"];

                    TLE tle;
                    if (mtd.contains("tle"))
                        tle = mtd["tle"];
                    else
                        throw std::runtime_error("Reproj : Could not get TLE!");

                    nlohmann::ordered_json timestamps;
                    if (mtd.contains("timestamps"))
                        timestamps = mtd["timestamps"];
                    else
                        throw std::runtime_error("Reproj : Could not get timestamps!");

                    // Reproj
                    // ""TPS""" (Mostly LEO)
                    {
                        std::shared_ptr<SatelliteProjection> sat_proj_src = get_sat_proj(src_proj_cfg, tle, timestamps);
                        bool direction = false;

                        double ratio_x = round((double)sat_proj_src->img_size_x / (double)op.img.width());
                        double ratio_y = round((double)sat_proj_src->img_size_y / (double)op.img.height());

                        for (int currentScan = 0; currentScan < (int)op.img.height(); currentScan++)
                        {
                            // Now compute each pixel's lat / lon and plot it
                            for (double px = 0; px < op.img.width() - 1; px += 1)
                            {
                                geodetic::geodetic_coords_t coords1, coords2, coords3;
                                bool ret1 = sat_proj_src->get_position(px * ratio_x, currentScan * ratio_y, coords1);
                                bool ret2 = sat_proj_src->get_position((px + 1) * ratio_x, currentScan * ratio_y, coords2);
                                sat_proj_src->get_position((px - img_x_offset) * ratio_x, (currentScan + 1) * ratio_y, coords3);

                                if (ret1 || ret2)
                                    continue;

                                direction = coords1.lat > coords3.lat;

                                std::pair<float, float> map_cc1 = projectionFunction(coords1.lat, coords1.lon, result_img.height(), result_img.width());
                                std::pair<float, float> map_cc2 = projectionFunction(coords2.lat, coords2.lon, result_img.height(), result_img.width());

                                uint16_t color[4] = {0, 0, 0, 0};
                                if (op.img.channels() >= 3)
                                {
                                    color[0] = op.img.channel(0)[currentScan * op.img.width() + int(px)];
                                    color[1] = op.img.channel(1)[currentScan * op.img.width() + int(px)];
                                    color[2] = op.img.channel(2)[currentScan * op.img.width() + int(px)];
                                    color[3] = 65535;
                                }
                                else
                                {
                                    color[0] = op.img[currentScan * op.img.width() + int(px)];
                                    color[1] = op.img[currentScan * op.img.width() + int(px)];
                                    color[2] = op.img[currentScan * op.img.width() + int(px)];
                                    color[3] = 65535;
                                }

                                // if (color[0] == 0 && color[1] == 0 && color[2] == 0) // Skip Black
                                //     continue;

                                // This seems to glitch out sometimes... Need to check
                                double maxSize = result_img.width() / 100.0;
                                if (abs(map_cc1.first - map_cc2.first) < maxSize && abs(map_cc1.second - map_cc2.second) < maxSize)
                                {
                                    /*
                                    Using a circle and radius from the next sample is most likely not the best way, computing the instrument's
                                    actual FOV at each pixel and so would be a better approach... But well, this one has the benefit of being
                                    fast.
                                    I guess time will tell how reliable that approximation is.
                                    */
                                    double circle_radius = sqrt(pow(int(map_cc1.first - map_cc2.first), 2) + pow(int(map_cc1.second - map_cc2.second), 2));
                                    result_img.draw_circle(map_cc1.first, map_cc1.second + (direction ? (circle_radius * 2.0) : (circle_radius * -2.0)), ceil(circle_radius), color, true); //, 0.4 * opacity);
                                }

                                // projected_image.draw_point(map_cc1.first, map_cc1.second, color, opacity);
                            }

                            if (progress != nullptr)
                                *progress = float(currentScan) / float(op.img.height());
                            // logger->critical("%d/%d", currentScan, image.height());
                        }
                    }
                }
                else
                {
                    warp::WarpOperation operation;
                    operation.ground_control_points = satdump::gcp_compute::compute_gcps(image::get_metadata_proj_cfg(op.img), op.img.width(), op.img.height());
                    operation.input_image = op.img;
                    operation.output_rgba = true;
                    // TODO : CHANGE!!!!!!
                    int l_width = std::max<int>(op.img.width(), 512) * 10;
                    operation.output_width = l_width;
                    operation.output_height = l_width / 2;

                    logger->trace("Warping size %dx%d", l_width, l_width / 2);

                    satdump::warp::WarpResult result = satdump::warp::performSmartWarp(operation, progress);

                    src_proj = proj::projection_t();
                    src_proj.type = proj::ProjType_Equirectangular;
                    src_proj.proj_offset_x = result.top_left.lon;
                    src_proj.proj_offset_y = result.top_left.lat;
                    src_proj.proj_scalar_x = (result.bottom_right.lon - result.top_left.lon) / double(result.output_image.width());
                    src_proj.proj_scalar_y = (result.bottom_right.lat - result.top_left.lat) / double(result.output_image.height());

                    if (!proj::projection_setup(&src_proj) && !src_proj_err)
                    { // If the input is a standard projection
                        for (int x = 0; x < (int)result_img.width(); x++)
                        {
                            for (int y = 0; y < (int)result_img.height(); y++)
                            {
                                double lon, lat, x2, y2; // To allow OMP...
                                if (proj::projection_perform_inv(&trg_proj, x, y, &lon, &lat))
                                    continue;
                                if (proj::projection_perform_fwd(&src_proj, lon, lat, &x2, &y2))
                                    continue;
                                transposePixel(result.output_image, result_img, x2, y2, x, y);
                            }
                            if (progress != nullptr)
                                *progress = float(x) / float(result_img.width());
                        }

                        proj::projection_free(&src_proj);
                    }
                }
            }

            image::set_metadata_proj_cfg(result_img, trg_proj);

            proj::projection_free(&trg_proj);
            return result_img;
        }

        std::function<std::pair<int, int>(double, double, int, int)> setupProjectionFunction(int width, int height,
                                                                                             nlohmann::json params,
                                                                                             bool rotate)
        {
            rescaleProjectionScalarsIfNeeded(params, width, height);

            std::shared_ptr<proj::projection_t>
                proj(new proj::projection_t(), [](proj::projection_t *obj)
                     { proj::projection_free(obj); delete obj; });

            bool proj_err = false;
            try
            {
                *proj = params;
            }
            catch (std::exception &e)
            {
                proj_err = true;
            }

            if (!proj::projection_setup(proj.get()) && !proj_err)
            {
                return [proj](double lat, double lon, int h, int w) mutable -> std::pair<int, int>
                {
                    double x, y;
                    if (proj::projection_perform_fwd(proj.get(), lon, lat, &x, &y))
                        return {-1, -1};
                    else if (x < 0 || x >= w)
                        return {-1, -1};
                    else if (y < 0 || y >= h)
                        return {-1, -1};
                    else
                        return {(int)x, (int)y};
                };
            }
            else
            {
                auto gcps = gcp_compute::compute_gcps(params, width, height);
                std::shared_ptr<projection::TPSTransform> transform = std::make_shared<projection::TPSTransform>();
                if (transform->init(gcps, true, false))
                    std::runtime_error("Error generating TPS!");
                return [transform, rotate](double lat, double lon, int map_height, int map_width) mutable -> std::pair<int, int>
                {
                    double x, y;
                    transform->forward(lon, lat, x, y);

                    if (x < 0 || x >= map_width)
                        return {-1, -1};
                    if (y < 0 || y >= map_height)
                        return {-1, -1};

                    if (rotate)
                    {
                        x = (map_width - 1) - x;
                        y = (map_height - 1) - y;
                    }

                    return {x, y};
                };
            }

            throw std::runtime_error("Invalid projection!!!!");
        }

        ProjBounds determineProjectionBounds(image::Image<uint16_t> &img)
        {
            if (!image::has_metadata(img))
                return {0, 0, 0, 0, false};

            if (!image::get_metadata(img).contains("proj_cfg"))
                return {0, 0, 0, 0, false};

            nlohmann::json params = image::get_metadata(img)["proj_cfg"];

            proj::projection_t proj;
            bool proj_err = false;
            try
            {
                proj = params;
            }
            catch (std::exception &e)
            {
                proj_err = true;
            }

            ProjBounds bounds;
            if (!proj::projection_setup(&proj) && !proj_err)
            {
                if (proj.type == proj::ProjType_Geos)
                {
                    bounds.min_lat = -90;
                    bounds.max_lat = 90;
                    bounds.min_lon = proj.lam0 * RAD2DEG - 90;
                    bounds.max_lon = proj.lam0 * RAD2DEG + 90;
                }
                else
                {
                    proj::projection_perform_inv(&proj, 0, 0, &bounds.min_lon, &bounds.max_lat);
                    proj::projection_perform_inv(&proj, img.width(), img.height(), &bounds.max_lon, &bounds.min_lat);
                }
                bounds.valid = true;
                proj::projection_free(&proj);
            }
            else
            {
                auto gcps = gcp_compute::compute_gcps(params, img.width(), img.height());

                bounds.min_lon = 180;
                bounds.max_lon = -180;
                bounds.min_lat = 90;
                bounds.max_lat = -90;
                for (auto &gcp : gcps)
                {
                    if (bounds.min_lon > gcp.lon)
                        bounds.min_lon = gcp.lon;
                    if (bounds.max_lon < gcp.lon)
                        bounds.max_lon = gcp.lon;
                    if (bounds.min_lat > gcp.lat)
                        bounds.min_lat = gcp.lat;
                    if (bounds.max_lat < gcp.lat)
                        bounds.max_lat = gcp.lat;
                }
                if (gcps.size() > 2)
                    bounds.valid = true;
            }

            bounds.min_lon -= 1;
            bounds.max_lon += 1;
            bounds.min_lat -= 1;
            bounds.max_lat += 1;

            if (bounds.min_lon < -180)
                bounds.min_lon = -180;
            if (bounds.max_lon > 180)
                bounds.max_lon = 180;
            if (bounds.min_lat < -90)
                bounds.min_lat = -90;
            if (bounds.max_lat > 90)
                bounds.max_lat = 90;

            return bounds;
        }

        void tryAutoTuneProjection(ProjBounds bounds, nlohmann::json &params)
        {
            proj::projection_t p_main = params;
            if (p_main.type == proj::ProjType_Equirectangular)
            {
                params["offset_x"] = bounds.min_lon;
                params["offset_y"] = bounds.max_lat;
                if (!params.contains("width") || !params.contains("height"))
                {
                    double scale_x = params.contains("scale_x") ? params["scale_x"].get<double>() : 0.016;
                    double scale_y = params.contains("scale_y") ? params["scale_y"].get<double>() : 0.016;
                    params["scalar_x"] = scale_x;
                    params["scalar_y"] = -scale_y;
                    params["width"] = (bounds.max_lon - bounds.min_lon) / scale_x;
                    params["height"] = (bounds.max_lat - bounds.min_lat) / scale_y;
                }
                else
                {
                    double width = params["width"];
                    double height = params["height"];
                    params["scalar_x"] = (bounds.max_lon - bounds.min_lon) / width;
                    params["scalar_y"] = -(bounds.max_lat - bounds.min_lat) / height;
                }
            }
            /*else if (p_main.type == proj::ProjType_UniversalTransverseMercator)
            {
                params["offset_x"] = 0.0;
                params["offset_y"] = 0.0;
                params["scalar_x"] = 1.0;
                params["scalar_y"] = 1.0;
                proj::projection_t proj = params;
                if (!proj::projection_setup(&proj))
                {
                    double dummy;
                    double tl_x, tl_y;
                    double br_x, br_y;
                    if (!proj::projection_perform_fwd(&proj, bounds.max_lon, bounds.max_lat, &dummy, &tl_y) &&
                        !proj::projection_perform_fwd(&proj, bounds.max_lon, bounds.min_lat, &br_x, &dummy) &&
                        !proj::projection_perform_fwd(&proj, bounds.min_lon, bounds.min_lat, &tl_x, &dummy) &&
                        !proj::projection_perform_fwd(&proj, (bounds.min_lon + bounds.max_lon) / 2.0, bounds.min_lat, &dummy, &br_y))
                    {
                        logger->trace("Final Bounds are : %f, %f - %f, %f", tl_x, tl_y, br_x, br_y);
                        params["offset_x"] = tl_x;
                        params["offset_y"] = tl_y;
                        params["scalar_x"] = -(tl_x - br_x) / vx;
                        params["scalar_y"] = -(tl_y - br_y) / vy;
                    }
                    proj::projection_free(&proj);
                }
            }*/
        }

        void rescaleProjectionScalarsIfNeeded(nlohmann::json &proj_cfg, int width, int height)
        {
            // Automatically rescale if needed
            if (proj_cfg.contains("width") && proj_cfg.contains("scalar_x") && proj_cfg["width"].get<double>() != width)
            {
                proj_cfg["scalar_x"] = proj_cfg["scalar_x"].get<double>() * (proj_cfg["width"].get<double>() / double(width));
                proj_cfg["width"] = width;
            }
            if (proj_cfg.contains("height") && proj_cfg.contains("scalar_y") && proj_cfg["height"].get<double>() != height)
            {
                proj_cfg["scalar_y"] = proj_cfg["scalar_y"].get<double>() * (proj_cfg["height"].get<double>() / double(height));
                proj_cfg["height"] = height;
            }
        }
    }
}