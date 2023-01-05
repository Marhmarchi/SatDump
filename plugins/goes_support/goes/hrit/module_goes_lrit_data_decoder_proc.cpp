#include "module_goes_lrit_data_decoder.h"
#include "logger.h"
#include "libs/miniz/miniz.h"
#include "libs/miniz/miniz_zip.h"
#include "imgui/imgui_image.h"
#include "common/lrit/utils.h"

#include "products/image_products.h"

namespace goes
{
    namespace hrit
    {
#if 0
        struct wip_products
        {
            std::vector<int> has_channels;
            std::vector<std::string> paths;
            time_t timestamp;
            std::string previous_path;
        };

        std::map<std::string, std::map<std::string, wip_products>> products_wip;
#endif

        void GOESLRITDataDecoderModule::save_imagery_file(image::Image<uint8_t> &img, ImageInfo &info)
        {
            std::string file_name = lrit::getHRITImageFilename(&info.timestamp, info.sat_prefix, info.channel) + ".png";

            std::string path_pattern = image_path_pattern;
            path_pattern = replaceAllStrings(path_pattern, "{SATNAME}", info.sat_name);
            path_pattern = replaceAllStrings(path_pattern, "{REGION}", info.region);
            path_pattern = replaceAllStrings(path_pattern, "{TIMESTAMP}", lrit::getHRITTimestamp(&info.timestamp));
            path_pattern = replaceAllStrings(path_pattern, "{CHANNEL}", info.channel);

            std::string base_path = directory + "/IMAGES/" + path_pattern + "/";

            if (!std::filesystem::exists(base_path))
                std::filesystem::create_directories(base_path);

            std::string fullpath = base_path + file_name;
            logger->info("Writing image " + fullpath + "...");
            if (info.is_goesn)
                img.resize(img.width(), img.height() * 1.75);
            img.save_img(fullpath);

            //////

#if 0
            {
                time_t timet = mktime_utc(&info.timestamp);
                if (products_wip.count(info.sat_name) == 0)
                    products_wip.insert({info.sat_name, std::map<std::string, wip_products>()});
                if (products_wip[info.sat_name].count(info.region) == 0)
                    products_wip[info.sat_name].insert({info.region, {{}, {}, timet, base_path}});

                wip_products &prods = products_wip[info.sat_name][info.region];

                if (prods.timestamp != timet)
                {
                    if (prods.has_channels.size() > 0)
                    {
                        logger->critical("Saving products! " + info.sat_name + " " + info.region + " " + std::to_string(prods.has_channels.size()));

                        satdump::ImageProducts images_products;
                        if (info.sat_name == "HIM")
                            images_products.instrument_name = "ahi";
                        else
                            images_products.instrument_name = "abi";
                        // images_products.has_timestamps = true;
                        // images_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                        // images_products.set_tle(satdump::general_tle_registry.get_from_norad(norad));
                        // images_products.set_timestamps(mtvza_reader.timestamps);

                        for (int i = 0; i < prods.has_channels.size(); i++)
                            images_products.images.push_back({prods.paths[i], std::to_string(prods.has_channels[i]), image::Image<uint16_t>()});

                        images_products.save(prods.previous_path);
                    }

                    prods = {{}, {}, timet, base_path};
                }

                try
                {
                    prods.has_channels.push_back(std::stoi(info.channel));
                    prods.paths.push_back(file_name);
                }
                catch (std::exception &e)
                {
                }
            }
#endif
        }

        void GOESLRITDataDecoderModule::process_data_IMAGE(::lrit::LRITFile &file, ::lrit::PrimaryHeader &primary_header, NOAALRITHeader &noaa_header)
        {
            const int ID_HIMAWARI = 43;

            std::string current_filename = file.filename;

            if (!std::filesystem::exists(directory + "/IMAGES"))
                std::filesystem::create_directory(directory + "/IMAGES");

            ::lrit::ImageStructureRecord image_structure_record = file.getHeader<::lrit::ImageStructureRecord>();

            ::lrit::TimeStampRecord timestamp_record = file.getHeader<::lrit::TimeStampRecord>();
            std::tm *timeReadable = gmtime(&timestamp_record.timestamp);

            std::string old_filename = current_filename;

            ImageInfo img_info;

            // GOES-R Data, from GOES-16 to 19.
            // Once again peeked in goestools for the meso detection, sorry :-)
            if (primary_header.file_type_code == 0 && (noaa_header.product_id == 16 ||
                                                       noaa_header.product_id == 17 ||
                                                       noaa_header.product_id == 18 ||
                                                       noaa_header.product_id == 19))
            {
                std::vector<std::string> cutFilename = splitString(current_filename, '-');

                if (cutFilename.size() >= 4)
                {
                    int mode = -1;
                    std::string channel;
                    int channeli = -1;

                    if (sscanf(cutFilename[3].c_str(), "M%dC%02d", &mode, &channeli) == 2)
                        channel = std::to_string(channeli);
                    else
                        channel = cutFilename[2];

                    AncillaryTextRecord ancillary_record = file.getHeader<AncillaryTextRecord>();

                    std::string region = "Others";

                    // Parse Region
                    if (ancillary_record.meta.count("Region") > 0)
                    {
                        std::string regionName = ancillary_record.meta["Region"];

                        if (regionName == "Full Disk")
                        {
                            region = "Full Disk";
                        }
                        else if (regionName == "Mesoscale")
                        {
                            if (cutFilename[2] == "CMIPM1")
                                region = "Mesoscale 1";
                            else if (cutFilename[2] == "CMIPM2")
                                region = "Mesoscale 2";
                            else
                                region = "Mesoscale";
                        }
                    }

                    // Parse scan time
                    std::tm scanTimestamp = *timeReadable;                      // Default to CCSDS timestamp normally...
                    if (ancillary_record.meta.count("Time of frame start") > 0) // ...unless we have a proper scan time
                    {
                        std::string scanTime = ancillary_record.meta["Time of frame start"];
                        strptime(scanTime.c_str(), "%Y-%m-%dT%H:%M:%S", &scanTimestamp);
                    }

                    img_info.is_sat = true;
                    img_info.sat_name = "GOES-" + std::to_string(noaa_header.product_id);
                    img_info.sat_prefix = "G" + std::to_string(noaa_header.product_id);
                    img_info.timestamp = scanTimestamp;
                    img_info.region = region;
                    img_info.channel = channel;

#if 1 // TODO, SWITCH TO PRODUCTS
                    if ((region == "Full Disk" || region == "Mesoscale 1" || region == "Mesoscale 2"))
                    {
                        std::shared_ptr<GOESRFalseColorComposer> goes_r_fc_composer;

                        if (region == "Full Disk")
                        {
                            goes_r_fc_composer = goes_r_fc_composer_full_disk;

                            /*if (channel == 2)
                            {
                                goes_r_fc_composer->push2(segmentedDecoder.image, mktime(&scanTimestamp));
                            }
                            else if (channel == 13)
                            {
                                goes_r_fc_composer->push13(segmentedDecoder.image, mktime(&scanTimestamp));
                            }*/
                        }
                        else if (region == "Mesoscale 1" || region == "Mesoscale 2")
                        {
                            if (region == "Mesoscale 1")
                                goes_r_fc_composer = goes_r_fc_composer_meso1;
                            else if (region == "Mesoscale 2")
                                goes_r_fc_composer = goes_r_fc_composer_meso2;

                            image::Image<uint8_t> image(&file.lrit_data[primary_header.total_header_length],
                                                        image_structure_record.columns_count,
                                                        image_structure_record.lines_count, 1);

                            if (channeli == 2)
                                goes_r_fc_composer->push2(image, mktime(&scanTimestamp));
                            else if (channeli == 13)
                                goes_r_fc_composer->push13(image, mktime(&scanTimestamp));
                        }

                        goes_r_fc_composer->img_info = img_info;
                        goes_r_fc_composer->img_info.channel = "FC";
                    }
#endif
                }
            }
            // GOES-N Data, from GOES-13 to 15.
            else if (primary_header.file_type_code == 0 && (noaa_header.product_id == 13 ||
                                                            noaa_header.product_id == 14 ||
                                                            noaa_header.product_id == 15))
            {
                img_info.is_goesn = true;

                int channel = -1;

                // Parse channel
                if (noaa_header.product_subid <= 10)
                    channel = 4;
                else if (noaa_header.product_subid <= 20)
                    channel = 1;
                else if (noaa_header.product_subid <= 30)
                    channel = 3;

                std::string region = "Others";

                // Parse Region. Had to peak in goestools again...
                if (noaa_header.product_subid % 10 == 1)
                    region = "Full Disk";
                else if (noaa_header.product_subid % 10 == 2)
                    region = "Northern Hemisphere";
                else if (noaa_header.product_subid % 10 == 3)
                    region = "Southern Hemisphere";
                else if (noaa_header.product_subid % 10 == 4)
                    region = "United States";
                else
                {
                    char buf[32];
                    size_t len;
                    int num = (noaa_header.product_subid % 10) - 5;
                    len = snprintf(buf, 32, "Special Interest %d", num);
                    region = std::string(buf, len);
                }

                // Parse scan time
                AncillaryTextRecord ancillary_record = file.getHeader<AncillaryTextRecord>();
                std::tm scanTimestamp = *timeReadable;                      // Default to CCSDS timestamp normally...
                if (ancillary_record.meta.count("Time of frame start") > 0) // ...unless we have a proper scan time
                {
                    std::string scanTime = ancillary_record.meta["Time of frame start"];
                    strptime(scanTime.c_str(), "%Y-%m-%dT%H:%M:%S", &scanTimestamp);
                }

                img_info.is_sat = true;
                img_info.sat_name = "GOES-" + std::to_string(noaa_header.product_id);
                img_info.sat_prefix = "G" + std::to_string(noaa_header.product_id);
                img_info.timestamp = scanTimestamp;
                img_info.region = region;
                img_info.channel = std::to_string(channel);
            }
            // Himawari-8 rebroadcast
            else if (primary_header.file_type_code == 0 && noaa_header.product_id == 43)
            {
                // Apparently the timestamp is in there for Himawari-8 data
                AnnotationRecord annotation_record = file.getHeader<AnnotationRecord>();

                std::vector<std::string> strParts = splitString(annotation_record.annotation_text, '_');
                if (strParts.size() > 3)
                {
                    strptime(strParts[2].c_str(), "%Y%m%d%H%M", timeReadable);
                    img_info.is_sat = true;
                    img_info.sat_name = "Himawari";
                    img_info.sat_prefix = "HIM";
                    img_info.timestamp = *timeReadable;
                    img_info.region = "Full Disk";
                    img_info.channel = std::to_string(noaa_header.product_subid);
                }
            }
            // NWS Images
            else if (primary_header.file_type_code == 0 && noaa_header.product_id == 6)
            {
                std::string subdir = "NWS";

                if (!std::filesystem::exists(directory + "/IMAGES/" + subdir))
                    std::filesystem::create_directories(directory + "/IMAGES/" + subdir);

                std::string back = current_filename;
                current_filename = subdir + "/" + back;
            }

            if (file.hasHeader<SegmentIdentificationHeader>())
            {
                SegmentIdentificationHeader segment_id_header = file.getHeader<SegmentIdentificationHeader>();

                if (all_wip_images.count(segment_id_header.image_identifier) == 0)
                    all_wip_images.insert({segment_id_header.image_identifier, std::make_unique<wip_images>()});

                std::unique_ptr<wip_images> &wip_img = all_wip_images[segment_id_header.image_identifier];

                wip_img->imageStatus = RECEIVING;

                if (segmentedDecoders.count(segment_id_header.image_identifier) == 0)
                    segmentedDecoders.insert({segment_id_header.image_identifier, SegmentedLRITImageDecoder()});

                SegmentedLRITImageDecoder &segmentedDecoder = segmentedDecoders[segment_id_header.image_identifier];

                if (segmentedDecoder.image_id != segment_id_header.image_identifier)
                {
                    if (segmentedDecoder.image_id != -1)
                    {
                        wip_img->imageStatus = SAVING;
                        save_imagery_file(segmentedDecoder.image, img_info);
                        wip_img->imageStatus = RECEIVING;

#if 1 // TODO

                        // Check if this is GOES-R
                        if (primary_header.file_type_code == 0 && (noaa_header.product_id == 16 ||
                                                                   noaa_header.product_id == 17 ||
                                                                   noaa_header.product_id == 18 ||
                                                                   noaa_header.product_id == 19))
                        {
                            int mode = -1;
                            int channel = -1;
                            std::vector<std::string> cutFilename = splitString(old_filename, '-');
                            if (cutFilename.size() > 3)
                            {
                                if (sscanf(cutFilename[3].c_str(), "M%dC%02d", &mode, &channel) == 2)
                                {
                                    if (goes_r_fc_composer_full_disk->hasData && (channel == 2 || channel == 2))
                                    {
                                        goes_r_fc_composer_full_disk->imageStatus = SAVING;
                                        save_imagery_file(goes_r_fc_composer_full_disk->falsecolor, goes_r_fc_composer_full_disk->img_info);
                                        logger->warn("This false color LUT was made by Harry Dove-Robinson, see resources/goes/abi/wxstar/README.md for more infos.");
                                        goes_r_fc_composer_full_disk->hasData = false;
                                        goes_r_fc_composer_full_disk->imageStatus = IDLE;
                                    }
                                }
                            }
                        }
#endif
                    }

                    segmentedDecoder = SegmentedLRITImageDecoder(segment_id_header.max_segment,
                                                                 image_structure_record.columns_count,
                                                                 image_structure_record.lines_count,
                                                                 segment_id_header.image_identifier);
                    segmentedDecoder.img_info = img_info;
                }

                if (noaa_header.product_id == ID_HIMAWARI)
                    segmentedDecoder.pushSegment(&file.lrit_data[primary_header.total_header_length], segment_id_header.segment_sequence_number - 1);
                else
                    segmentedDecoder.pushSegment(&file.lrit_data[primary_header.total_header_length], segment_id_header.segment_sequence_number);

                // Check if this is GOES-R, if yes, MS
                if (primary_header.file_type_code == 0 && (noaa_header.product_id == 16 ||
                                                           noaa_header.product_id == 17 ||
                                                           noaa_header.product_id == 18 ||
                                                           noaa_header.product_id == 19))
                {
                    int mode = -1;
                    int channel = -1;
                    std::vector<std::string> cutFilename = splitString(old_filename, '-');
                    if (cutFilename.size() > 3)
                    {
                        if (sscanf(cutFilename[3].c_str(), "M%dC%02d", &mode, &channel) == 2)
                        {
                            AncillaryTextRecord ancillary_record = file.getHeader<AncillaryTextRecord>();

                            // Parse scan time
                            std::tm scanTimestamp = *timeReadable;                      // Default to CCSDS timestamp normally...
                            if (ancillary_record.meta.count("Time of frame start") > 0) // ...unless we have a proper scan time
                            {
                                std::string scanTime = ancillary_record.meta["Time of frame start"];
                                strptime(scanTime.c_str(), "%Y-%m-%dT%H:%M:%S", &scanTimestamp);
                            }

#if 1 // TODO
                            if (channel == 2)
                                goes_r_fc_composer_full_disk->push2(segmentedDecoder.image, mktime(&scanTimestamp));
                            else if (channel == 13)
                                goes_r_fc_composer_full_disk->push13(segmentedDecoder.image, mktime(&scanTimestamp));
#endif
                        }
                    }
                }

                // If the UI is active, update texture
                if (wip_img->textureID > 0)
                {
                    // Downscale image
                    wip_img->img_height = 1000;
                    wip_img->img_width = 1000;
                    image::Image<uint8_t> imageScaled = segmentedDecoder.image;
                    imageScaled.resize(wip_img->img_width, wip_img->img_height);
                    uchar_to_rgba(imageScaled.data(), wip_img->textureBuffer, wip_img->img_height * wip_img->img_width);
                    wip_img->hasToUpdate = true;
                }

                if (segmentedDecoder.isComplete())
                {
                    wip_img->imageStatus = SAVING;
                    save_imagery_file(segmentedDecoder.image, img_info);
                    segmentedDecoder = SegmentedLRITImageDecoder();
                    wip_img->imageStatus = IDLE;

#if 1 // TODO

                    // Check if this is GOES-R
                    if (primary_header.file_type_code == 0 && (noaa_header.product_id == 16 ||
                                                               noaa_header.product_id == 17 ||
                                                               noaa_header.product_id == 18 ||
                                                               noaa_header.product_id == 19))
                    {
                        int mode = -1;
                        int channel = -1;
                        std::vector<std::string> cutFilename = splitString(old_filename, '-');
                        if (cutFilename.size() > 3)
                        {
                            if (sscanf(cutFilename[3].c_str(), "M%dC%02d", &mode, &channel) == 2)
                            {
                                if (goes_r_fc_composer_full_disk->hasData && (channel == 2 || channel == 2))
                                {
                                    goes_r_fc_composer_full_disk->imageStatus = SAVING;
                                    save_imagery_file(goes_r_fc_composer_full_disk->falsecolor, goes_r_fc_composer_full_disk->img_info);
                                    logger->warn("This false color LUT was made by Harry Dove-Robinson, see resources/goes/abi/wxstar/README.md for more infos.");
                                    goes_r_fc_composer_full_disk->hasData = false;
                                    goes_r_fc_composer_full_disk->imageStatus = IDLE;
                                }
                            }
                        }
                    }
#endif
                }
            }
            else
            {
                if (noaa_header.noaa_specific_compression == 5) // Gif?
                {
                    logger->info("Writing file " + directory + "/IMAGES/" + current_filename + ".gif" + "...");

                    int offset = primary_header.total_header_length;

                    // Write file out
                    std::ofstream fileo(directory + "/IMAGES/" + current_filename + ".gif", std::ios::binary);
                    fileo.write((char *)&file.lrit_data[offset], file.lrit_data.size() - offset);
                    fileo.close();
                }
                else // Write raw image dats
                {
                    image::Image<uint8_t> image(&file.lrit_data[primary_header.total_header_length], image_structure_record.columns_count, image_structure_record.lines_count, 1);

                    if (!img_info.is_sat) // Not to mess things up with possible NWS stuff
                        image.save_png(std::string(directory + "/IMAGES/" + current_filename + ".png").c_str());
                    else
                        save_imagery_file(image, img_info);

#if 1
                    // Check if this is GOES-R
                    if (primary_header.file_type_code == 0 && (noaa_header.product_id == 16 ||
                                                               noaa_header.product_id == 17 ||
                                                               noaa_header.product_id == 18 ||
                                                               noaa_header.product_id == 19))
                    {
                        if (goes_r_fc_composer_meso1->hasData)
                        {
                            goes_r_fc_composer_meso1->imageStatus = SAVING;
                            save_imagery_file(goes_r_fc_composer_meso1->falsecolor, goes_r_fc_composer_meso1->img_info);
                            logger->warn("This false color LUT was made by Harry Dove-Robinson, see resources/goes/abi/wxstar/README.md for more infos.");
                            goes_r_fc_composer_meso1->hasData = false;
                            goes_r_fc_composer_meso1->imageStatus = IDLE;
                        }
                        if (goes_r_fc_composer_meso2->hasData)
                        {
                            goes_r_fc_composer_meso2->imageStatus = SAVING;
                            save_imagery_file(goes_r_fc_composer_meso2->falsecolor, goes_r_fc_composer_meso2->img_info);
                            logger->warn("This false color LUT was made by Harry Dove-Robinson, see resources/goes/abi/wxstar/README.md for more infos.");
                            goes_r_fc_composer_meso2->hasData = false;
                            goes_r_fc_composer_meso2->imageStatus = IDLE;
                        }
                    }
#endif
                }
            }
        }

        void GOESLRITDataDecoderModule::process_data_EMWIN(::lrit::LRITFile &file, ::lrit::PrimaryHeader &primary_header, NOAALRITHeader &noaa_header)
        {
            std::string clean_filename = file.filename.substr(0, file.filename.size() - 5); // Remove extensions

            if (noaa_header.noaa_specific_compression == 0) // Uncompressed TXT
            {
                if (!std::filesystem::exists(directory + "/EMWIN"))
                    std::filesystem::create_directory(directory + "/EMWIN");

                logger->info("Writing file " + directory + "/EMWIN/" + clean_filename + ".txt" + "...");

                int offset = primary_header.total_header_length;

                // Write file out
                std::ofstream fileo(directory + "/EMWIN/" + clean_filename + ".txt", std::ios::binary);
                fileo.write((char *)&file.lrit_data[offset], file.lrit_data.size() - offset);
                fileo.close();
            }
            else if (noaa_header.noaa_specific_compression == 10) // ZIP Files
            {
                if (!std::filesystem::exists(directory + "/EMWIN"))
                    std::filesystem::create_directory(directory + "/EMWIN");

                int offset = primary_header.total_header_length;

                // Init
                mz_zip_archive zipFile;
                MZ_CLEAR_OBJ(zipFile);
                if (!mz_zip_reader_init_mem(&zipFile, &file.lrit_data[offset], file.lrit_data.size() - offset, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
                {
                    logger->error("Could not open ZIP data! Discarding...");
                    return;
                }

                // Read filename
                char filenameStr[1000];
                int chs = mz_zip_reader_get_filename(&zipFile, 0, filenameStr, 1000);
                std::string filename(filenameStr, &filenameStr[chs - 1]);

                // Decompress
                size_t outSize = 0;
                uint8_t *outBuffer = (uint8_t *)mz_zip_reader_extract_to_heap(&zipFile, 0, &outSize, 0);

                // Write out
                logger->info("Writing file " + directory + "/EMWIN/" + filename + "...");
                std::ofstream file(directory + "/EMWIN/" + filename, std::ios::binary);
                file.write((char *)outBuffer, outSize);
                file.close();

                // Free memory
                zipFile.m_pFree(&zipFile, outBuffer);
            }
        }

        void GOESLRITDataDecoderModule::process_data_MESSAGE(::lrit::LRITFile &file, ::lrit::PrimaryHeader &primary_header, NOAALRITHeader &noaa_header)
        {
            std::string clean_filename = file.filename.substr(0, file.filename.size() - 5); // Remove extensions

            if (!std::filesystem::exists(directory + "/Admin Messages"))
                std::filesystem::create_directory(directory + "/Admin Messages");

            logger->info("Writing file " + directory + "/Admin Messages/" + clean_filename + ".txt" + "...");

            int offset = primary_header.total_header_length;

            // Write file out
            std::ofstream fileo(directory + "/Admin Messages/" + clean_filename + ".txt", std::ios::binary);
            fileo.write((char *)&file.lrit_data[offset], file.lrit_data.size() - offset);
            fileo.close();
        }

        void GOESLRITDataDecoderModule::process_data_DCS(::lrit::LRITFile &file, ::lrit::PrimaryHeader &primary_header, NOAALRITHeader &noaa_header)
        {
            if (!std::filesystem::exists(directory + "/DCS"))
                std::filesystem::create_directory(directory + "/DCS");

            logger->info("Writing file " + directory + "/DCS/" + file.filename + "...");

            // Write file out
            std::ofstream fileo(directory + "/DCS/" + file.filename, std::ios::binary);
            fileo.write((char *)file.lrit_data.data(), file.lrit_data.size());
            fileo.close();
        }

        void GOESLRITDataDecoderModule::processLRITFile(::lrit::LRITFile &file)
        {
            std::string current_filename = file.filename;

            ::lrit::PrimaryHeader primary_header = file.getHeader<::lrit::PrimaryHeader>();
            NOAALRITHeader noaa_header = file.getHeader<NOAALRITHeader>();

            if (write_images && primary_header.file_type_code == 0 && file.hasHeader<::lrit::ImageStructureRecord>())
                process_data_IMAGE(file, primary_header, noaa_header); // Check if this is image data, and if so also write it as an image
            else if (write_emwin && primary_header.file_type_code == 2 && (noaa_header.product_id == 9 || noaa_header.product_id == 6))
                process_data_EMWIN(file, primary_header, noaa_header); // Check if this EMWIN data
            else if (write_messages && (primary_header.file_type_code == 1 || primary_header.file_type_code == 2))
                process_data_MESSAGE(file, primary_header, noaa_header); // Check if this is message data. If we slipped to here we know it's not EMWIN
            else if (write_dcs && primary_header.file_type_code == 130)
                process_data_DCS(file, primary_header, noaa_header); // Check if this is DCS data
            else if (write_unknown)                                  // Otherwise, write as generic, unknown stuff. This should not happen
            {
                if (!std::filesystem::exists(directory + "/LRIT"))
                    std::filesystem::create_directory(directory + "/LRIT");

                logger->info("Writing file " + directory + "/LRIT/" + current_filename + "...");

                // Write file out
                std::ofstream fileo(directory + "/LRIT/" + current_filename, std::ios::binary);
                fileo.write((char *)file.lrit_data.data(), file.lrit_data.size());
                fileo.close();
            }
        }
    } // namespace avhrr
} // namespace metop