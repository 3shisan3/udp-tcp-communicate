#include "yaml_cfg.h"

#include <fstream>

bool YamlCfg::loadCfgFile(const std::string &cfgPath)
{
    try
    {
        m_yamlNode_ = YAML::LoadFile(cfgPath);
        return true;
    }
    catch (const YAML::Exception &e)
    {
        // Handle YAML parsing exceptions
        return false;
    }
}

bool YamlCfg::setSavePath(const std::string &cfgPath)
{
    m_savePath_ = cfgPath;
    return true;
}

bool YamlCfg::getValue(const std::string &key, std::string &value)
{
    try
    {
        if (m_yamlNode_[key])
        {
            value = m_yamlNode_[key].as<std::string>();
            return true;
        }
        return false;
    }
    catch (const YAML::Exception &e)
    {
        // Handle YAML exceptions
        return false;
    }
}

bool YamlCfg::setValue(const std::string &key, const std::string &value)
{
    try
    {
        m_yamlNode_[key] = value;
        if (!m_savePath_.empty())
        {
            std::ofstream fout(m_savePath_);
            fout << m_yamlNode_;
        }
        return true;
    }
    catch (const YAML::Exception &e)
    {
        // Handle YAML exceptions
        return false;
    }
}
