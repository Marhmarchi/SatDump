#include "autotrack.h"
#include "logger.h"
#include "common/utils.h"

AutoTrackApp::AutoTrackApp(nlohmann::json settings, nlohmann::json parameters, std::string output_folder)
    : d_settings(settings), d_parameters(parameters), d_output_folder(output_folder)
{
    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// Initial settings parsing
    ///////////////////////////////////////////////////////////////////////////////////

    uint64_t samplerate;
    uint64_t initial_frequency;
    std::string handler_id;
    uint64_t hdl_dev_id = 0;

    try
    {
        samplerate = parameters["samplerate"].get<uint64_t>();
        initial_frequency = parameters["initial_frequency"].get<uint64_t>();
        handler_id = parameters["source"].get<std::string>();
        if (parameters.contains("source_id"))
            hdl_dev_id = parameters["source_id"].get<uint64_t>();
    }
    catch (std::exception &e)
    {
        logger->error("Error parsing arguments! %s", e.what());
        exit(1);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// SDR Search
    ///////////////////////////////////////////////////////////////////////////////////

    dsp::registerAllSources();
    std::vector<dsp::SourceDescriptor> source_tr = dsp::getAllAvailableSources();

    for (dsp::SourceDescriptor src : source_tr)
        logger->debug("Device " + src.name);

    // Try to find it and check it's usable
    bool src_found = false;
    for (dsp::SourceDescriptor src : source_tr)
    {
        if (handler_id == src.source_type)
        {
            if (parameters.contains("source_id"))
            {
#ifdef _WIN32 // Windows being cursed. TODO investigate further? It's uint64_t everywhere come on!
                char cmp_buff1[100];
                char cmp_buff2[100];

                snprintf(cmp_buff1, sizeof(cmp_buff1), "%" PRIu64, hdl_dev_id);
                std::string cmp1 = cmp_buff1;
                snprintf(cmp_buff2, sizeof(cmp_buff2), "%" PRIu64, src.unique_id);
                std::string cmp2 = cmp_buff2;
                if (cmp1 == cmp2)
#else
                if (hdl_dev_id == src.unique_id)
#endif
                {
                    selected_src = src;
                    src_found = true;
                }
            }
            else
            {
                selected_src = src;
                src_found = true;
            }
        }
    }

    if (!src_found)
    {
        logger->error("Could not find a handler for source type : %s!", handler_id.c_str());
        exit(1);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// SDR Open & Init, Main DSP Setup
    ///////////////////////////////////////////////////////////////////////////////////

    // SDR
    source_ptr = getSourceFromDescriptor(selected_src);
    source_ptr->open();

    set_frequency(initial_frequency);

    source_ptr->set_samplerate(samplerate);
    source_ptr->set_settings(parameters);

    // Splitter
    splitter = std::make_unique<dsp::SplitterBlock>(source_ptr->output_stream);
    splitter->set_main_enabled(false);
    splitter->add_output("record");
    splitter->add_output("live");

    // Optional FFT
    if (parameters.contains("fft_enable") && parameters["fft_enable"])
    {
        if (parameters.contains("fft_size"))
            fft_size = parameters["fft_size"].get<int>();
        if (parameters.contains("fft_rate"))
            fft_rate = parameters["fft_rate"].get<int>();
        if (parameters.contains("fft_min"))
            fft_min = parameters["fft_min"].get<int>();
        if (parameters.contains("fft_max"))
            fft_max = parameters["fft_max"].get<int>();

        splitter->add_output("fft");
        fft = std::make_unique<dsp::FFTPanBlock>(splitter->get_output("fft"));
        fft->set_fft_settings(fft_size, samplerate, fft_rate);
        if (parameters.contains("fft_avgn"))
            fft->avg_num = parameters["fft_avgn"].get<float>();

        fft_plot = std::make_unique<widgets::FFTPlot>(fft->output_stream->writeBuf, fft_size, fft_min, fft_max, 40);
        logger->critical("FFT GOOD!");
        fft->start();
    }

    file_sink = std::make_shared<dsp::FileSinkBlock>(splitter->get_output("record"));
    file_sink->start();

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// Tracking modules init
    ///////////////////////////////////////////////////////////////////////////////////

    double qth_lon = settings["qth"]["lon"];
    double qth_lat = settings["qth"]["lat"];
    double qth_alt = settings["qth"]["alt"];

    logger->trace("Using QTH %f %f Alt %f", qth_lon, qth_lat, qth_alt);

    // Init Obj Tracker & scheduler
    object_tracker.setQTH(qth_lon, qth_lat, qth_alt);
    auto_scheduler.setQTH(qth_lon, qth_lat, qth_alt);

    // Other config for the tracker and scheduler
    std::vector<satdump::TrackedObject> enabled_satellites = settings["tracked_objects"];
    nlohmann::json rotator_algo_cfg;
    if (settings["tracking"].contains("rotator_algo"))
        rotator_algo_cfg = settings["tracking"]["rotator_algo"];

    auto_scheduler.setTracked(enabled_satellites);
    object_tracker.setRotatorConfig(rotator_algo_cfg);
    auto_scheduler.setAutoTrackCfg(getValueOrDefault<satdump::AutoTrackCfg>(settings["tracking"]["autotrack_cfg"], satdump::AutoTrackCfg()));

    setup_schedular_callbacks();

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// Optional source start
    ///////////////////////////////////////////////////////////////////////////////////

    if (!auto_scheduler.getAutoTrackCfg().stop_sdr_when_idle)
        start_device();

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// Rotator Setup & Start autotrack
    ///////////////////////////////////////////////////////////////////////////////////

    rotator_handler = std::make_shared<rotator::RotctlHandler>();

    if (rotator_handler)
    {
        try
        {
            rotator_handler->set_settings(settings["tracking"]["rotator_cfg"]);
        }
        catch (std::exception &)
        {
        }

        rotator_handler->connect();
        // rotator_handler->set_pos(0, 0);
        object_tracker.setRotator(rotator_handler);
        // object_tracker.setRotatorReqPos(0, 0);
        object_tracker.setRotatorEngaged(true);
        object_tracker.setRotatorTracking(true);
    }

    // Finally, start
    auto_scheduler.start();
    auto_scheduler.setEngaged(true, getTime());

    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////// WebServer
    ///////////////////////////////////////////////////////////////////////////////////

    setup_webserver();
}

AutoTrackApp::~AutoTrackApp()
{
    stop_webserver();

retry_vfo:
    for (auto &vfo : vfo_list)
    {
        del_vfo(vfo.id);
        goto retry_vfo;
    }

    stop_processing();
    stop_device();
    source_ptr->close();
    splitter->input_stream = std::make_shared<dsp::stream<complex_t>>();
    splitter->stop();
    if (fft)
        fft->stop();
    if (file_sink)
        file_sink->stop();
}

void AutoTrackApp::setup_schedular_callbacks()
{
    auto_scheduler.eng_callback = [this](satdump::AutoTrackCfg, satdump::SatellitePass, satdump::TrackedObject obj)
    {
        // logger->critical(obj.norad);
        object_tracker.setObject(object_tracker.TRACKING_SATELLITE, obj.norad);
    };

    auto_scheduler.aos_callback = [this](satdump::AutoTrackCfg autotrack_cfg, satdump::SatellitePass, satdump::TrackedObject obj)
    {
        object_tracker.setObject(object_tracker.TRACKING_SATELLITE, obj.norad);

        if (autotrack_cfg.multi_mode || obj.downlinks.size() > 1)
        {
            for (auto &dl : obj.downlinks)
            {
                if (dl.live || dl.record)
                    if (!is_started)
                        start_device();

                if (dl.live)
                {
                    std::string id = std::to_string(obj.norad) + "_" + std::to_string(dl.frequency) + "_live";
                    std::string name = std::to_string(obj.norad);
                    if (satdump::general_tle_registry.get_from_norad(obj.norad).has_value())
                        name = satdump::general_tle_registry.get_from_norad(obj.norad)->name;
                    name += " - " + format_notated(dl.frequency, "Hz");
                    add_vfo_live(id, name, dl.frequency, dl.pipeline_selector->pipeline_id, dl.pipeline_selector->getParameters());
                }

                if (dl.record)
                {
                    std::string id = std::to_string(obj.norad) + "_" + std::to_string(dl.frequency) + "_record";
                    std::string name = std::to_string(obj.norad);
                    if (satdump::general_tle_registry.get_from_norad(obj.norad).has_value())
                        name = satdump::general_tle_registry.get_from_norad(obj.norad)->name;
                    name += " - " + format_notated(dl.frequency, "Hz");
                    add_vfo_reco(id, name, dl.frequency, dsp::basebandTypeFromString(dl.baseband_format), dl.baseband_decimation);
                }
            }
        }
        else
        {
            if (obj.downlinks[0].live)
                stop_processing();
            if (obj.downlinks[0].record)
                stop_recording();

            if (obj.downlinks[0].live || obj.downlinks[0].record)
            {
                frequency_hz = obj.downlinks[0].frequency;
                if (is_started)
                    set_frequency(frequency_hz);
                else
                    start_device();

                // Catch situations where source could not start
                if (!is_started)
                {
                    logger->error("Could not start recorder/processor since the source could not be started!");
                    return;
                }
            }

            if (obj.downlinks[0].live)
            {
                pipeline_params = obj.downlinks[0].pipeline_selector->getParameters();
                pipeline_id = obj.downlinks[0].pipeline_selector->pipeline_id;
                start_processing();
            }

            if (obj.downlinks[0].record)
            {
                file_sink->set_output_sample_type(dsp::basebandTypeFromString(obj.downlinks[0].baseband_format));
                start_recording();
            }
        }
    };

    auto_scheduler.los_callback = [this](satdump::AutoTrackCfg autotrack_cfg, satdump::SatellitePass, satdump::TrackedObject obj)
    {
        if (autotrack_cfg.multi_mode || obj.downlinks.size() > 1)
        {
            for (auto &dl : obj.downlinks)
            {
                if (dl.live)
                {
                    std::string id = std::to_string(obj.norad) + "_" + std::to_string(dl.frequency) + "_live";
                    del_vfo(id);
                }

                if (dl.record)
                {
                    std::string id = std::to_string(obj.norad) + "_" + std::to_string(dl.frequency) + "_record";
                    del_vfo(id);
                }

                if (dl.live || dl.record)
                    if (is_started && vfo_list.size() == 0 && autotrack_cfg.stop_sdr_when_idle)
                        stop_device();
            }
        }
        else
        {
            if (obj.downlinks[0].record)
                stop_recording();
            if (obj.downlinks[0].live)
                stop_processing();
            if (autotrack_cfg.stop_sdr_when_idle)
                stop_device();
        }
    };
}
