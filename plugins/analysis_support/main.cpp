#include "core/plugin.h"
#include "logger.h"
#include "core/module.h"

#include "analysis/module_analysis_work.h"

class AnalysisPluginSupport : public satdump::Plugin
{
public:
    std::string getID()
    {
        return "analysis_support";
    }

    void init()
    {
	    satdump::eventBus->register_handler<RegisterModulesEvent>(registerPluginsHandler);
    }

    static void registerPluginsHandler(const RegisterModulesEvent &evt)
    {
        REGISTER_MODULE_EXTERNAL(evt.modules_registry, analysis::AnalysisWork);
    }
};

PLUGIN_LOADER(AnalysisPluginSupport)
