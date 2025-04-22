#include "config_wrapper.h"



ConfigWrapper::FileType ConfigWrapper::identifyFileType(const std::string &cfgPath)
{
    if (cfgPath.find(".json") != std::string::npos)
    {
        return FileType::FILE_TYPE_JSON;
    }
    else if (cfgPath.find(".yaml") != std::string::npos)
    {
        return FileType::FILE_TYPE_YAML;
    }
    else
    {
        return FileType::FILE_TYPE_UNKNOWN;
    }
}