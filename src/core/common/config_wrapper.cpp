#include "config_wrapper.h"

#include <fstream>
#include <sstream>

#include "config/json_cfg.h"
#include "config/yaml_cfg.h"
#include "utils/json.hpp"
#include "utils/yaml.hpp"

ConfigWrapper::FileType ConfigWrapper::identifyFileType(const std::string &cfgPath)
{
    // 优先通过文件名来判断
    if (cfgPath.find(".json") != std::string::npos)
    {
        return FileType::FILE_TYPE_JSON;
    }
    else if (cfgPath.find(".yaml") != std::string::npos)
    {
        return FileType::FILE_TYPE_YAML;
    }
    
    // 解析文件内容判度胺
    std::ifstream file(cfgPath);
    if (!file.is_open())
    {
        return FileType::FILE_TYPE_UNKNOWN;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    try
    {
        auto json = nlohmann::json::parse(content);
        return FileType::FILE_TYPE_JSON;
    }
    catch (const nlohmann::json::parse_error &)
    {
        // Not JSON, continue checking
    }
    try
    {
        YAML::Node yaml = YAML::Load(content);
        return FileType::FILE_TYPE_YAML;
    }
    catch (const YAML::ParserException &)
    {
        // Not YAML, continue checking
    }

    return FileType::FILE_TYPE_UNKNOWN;
}

int ConfigWrapper::loadCfgFile(const std::string &cfgPath)
{
    FileType fileType = identifyFileType(cfgPath);
    switch (fileType)
    {
    case FileType::FILE_TYPE_JSON:
    {
        // m_cfgPointer_ = std::make_unique<JsonCfg>();
        // if (m_cfgPointer_->loadCfgFile(cfgPath) != 0)
        // {
        //     return -1;
        // }
    }
        break;
    case FileType::FILE_TYPE_YAML:
    {
        m_cfgPointer_ = std::make_unique<YamlCfg>();
        if (m_cfgPointer_->loadCfgFile(cfgPath) != 0)
        {
            return -1;
        }
    }
        break;
    default:
        // 处理未知文件类型
        return -1;
    }

    return 0;
}