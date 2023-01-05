#pragma once

#include <string>
#include <ctime>

namespace lrit
{
    std::string getHRITTimestamp(std::tm *timeReadable);
    std::string getHRITImageFilename(std::tm *timeReadable, std::string sat_name, int channel);
    std::string getHRITImageFilename(std::tm *timeReadable, std::string sat_name, std::string channel);
}