#include "config_wrapper.h"

#include <fstream>
#include <sstream>

#include "config/json/json_cfg.h"
#include "config/yaml/yaml_cfg.h"
#include "utils/json.hpp"
#include "utils/yaml.hpp"

ConfigWrapper::FileType ConfigWrapper::identifyFileType(const std::string &cfgPath)
{
    LOG_TRACE("Identifying file type for: {}", cfgPath);

    // 优先通过文件名来判断
    if (cfgPath.find(".json") != std::string::npos)
    {
        LOG_DEBUG("Identified JSON file by extension: {}", cfgPath);
        return FileType::FILE_TYPE_JSON;
    }
    else if (cfgPath.find(".yaml") != std::string::npos)
    {
        LOG_DEBUG("Identified YAML file by extension: {}", cfgPath);
        return FileType::FILE_TYPE_YAML;
    }

    LOG_DEBUG("File type not identified by extension, checking content: {}", cfgPath);
    
    // 解析文件内容判度胺
    std::ifstream file(cfgPath);
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open file for type identification: {}", cfgPath);
        return FileType::FILE_TYPE_NONSUPPORT;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    try
    {
        auto json = nlohmann::json::parse(content);
        LOG_DEBUG("Identified JSON file by content parsing: {}", cfgPath);
        return FileType::FILE_TYPE_JSON;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        // Not JSON, continue checking
        LOG_TRACE("JSON parse failed (expected for non-JSON files): {}", e.what());
    }
    try
    {
        YAML::Node yaml = YAML::Load(content);
        LOG_DEBUG("Identified YAML file by content parsing: {}", cfgPath);
        return FileType::FILE_TYPE_YAML;
    }
    catch (const YAML::ParserException &e)
    {
        // Not YAML, continue checking
        LOG_TRACE("YAML parse failed (expected for non-YAML files): {}", e.what());
    }

    LOG_WARNING("Unsupported file type: {}", cfgPath);
    return FileType::FILE_TYPE_NONSUPPORT;
}

int ConfigWrapper::loadCfgFile(const std::string &cfgPath)
{
    LOG_INFO("Loading configuration file: {}", cfgPath);

    FileType fileType = identifyFileType(cfgPath);
    switch (fileType)
    {
    case FileType::FILE_TYPE_JSON:
    {
        LOG_DEBUG("Loading JSON configuration: {}", cfgPath);
        // m_cfgPointer_ = std::make_unique<JsonCfg>();
        // if (m_cfgPointer_->loadCfgFile(cfgPath) != 0)
        // {
        //     return -1;
        // }
    }
        break;
    case FileType::FILE_TYPE_YAML:
    {
        LOG_DEBUG("Loading YAML configuration: {}", cfgPath);
        m_cfgPointer_ = std::make_unique<YamlCfg>();
        if (!m_cfgPointer_->loadCfgFile(cfgPath))
        {
            LOG_ERROR("Failed to load YAML configuration: {}", cfgPath);
            return -1;
        }
        LOG_INFO("Successfully loaded YAML configuration: {}", cfgPath);
    }
        break;
    default:
        // 处理未知文件类型
        LOG_ERROR("Unsupported configuration file type: {}", cfgPath);
        return -1;
    }

    return 0;
}