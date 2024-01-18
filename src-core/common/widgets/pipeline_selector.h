#pragma once
#include <string>
#include "core/params.h"
#include "core/pipeline.h"
#include "imgui/pfd/widget.h"

namespace satdump
{
    class PipelineUISelector
    {
    public:
        PipelineUISelector(bool live_mode);
        void updateSelectedPipeline();
        void renderSelectionBox(double width = -1);
        void drawMainparams();
        void drawMainparamsLive();
        void renderParamTable();
        nlohmann::json getParameters();
        void setParameters(nlohmann::json params);
        void select_pipeline(std::string id);
        std::string get_name(int index);

        FileSelectWidget inputfileselect = FileSelectWidget("Input File", "Select Input File");
        FileSelectWidget outputdirselect = FileSelectWidget("Output Directory", "Select Output Directory", true);
        int pipeline_id = 0;
        int pipelines_levels_select_id = -1;

    private:
        bool contains(std::vector<int> tm, int n);
        void getParamsFromInput();
        void try_set_param(std::string name, nlohmann::json v);

        bool live_mode;
        std::string text = u8"\uf006";
        std::vector<int> favourite;
        std::mutex pipeline_mtx;

        std::string pipeline_levels_str;
        std::string pipeline_search_in;

        std::vector<std::pair<std::string, satdump::params::EditableParameter>> parameters_ui;
        std::vector<std::pair<std::string, satdump::params::EditableParameter>> parameters_ui_pipeline;
    };
}