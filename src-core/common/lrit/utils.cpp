#include "utils.h"

namespace lrit
{
    std::string getHRITTimestamp(std::tm *timeReadable)
    {
        return std::to_string(timeReadable->tm_year + 1900) +                                                                               // Year yyyy
               (timeReadable->tm_mon + 1 > 9 ? std::to_string(timeReadable->tm_mon + 1) : "0" + std::to_string(timeReadable->tm_mon + 1)) + // Month MM
               (timeReadable->tm_mday > 9 ? std::to_string(timeReadable->tm_mday) : "0" + std::to_string(timeReadable->tm_mday)) + "T" +    // Day dd
               (timeReadable->tm_hour > 9 ? std::to_string(timeReadable->tm_hour) : "0" + std::to_string(timeReadable->tm_hour)) +          // Hour HH
               (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min)) +             // Minutes mm
               (timeReadable->tm_sec > 9 ? std::to_string(timeReadable->tm_sec) : "0" + std::to_string(timeReadable->tm_sec)) + "Z";        // Seconds ss
    }

    std::string getHRITImageFilename(std::tm *timeReadable, std::string sat_name, int channel)
    {
        return getHRITImageFilename(timeReadable, sat_name, std::to_string(channel));
    }

    std::string getHRITImageFilename(std::tm *timeReadable, std::string sat_name, std::string channel)
    {
        return sat_name + "_" + channel + "_" + // Satellite name and channel
               getHRITTimestamp(timeReadable);
    }
}