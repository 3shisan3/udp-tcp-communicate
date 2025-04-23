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

bool YamlCfg::saveCurCfg(const std::string &cfgPath)
{
    std::string savePath = cfgPath.empty() ? m_savePath_ : cfgPath;

    try
    {
        std::ofstream fout(savePath);
        fout << m_yamlNode_; // 写入文件
        return true;
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool YamlCfg::getValueImpl(const std::string &key, int& value)
{
    try
    {
        value = m_yamlNode_[key].as<int>();
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::getValueImpl(const std::string &key, double& value)
{
    try
    {
        value = m_yamlNode_[key].as<double>();
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::getValueImpl(const std::string &key, bool& value)
{
    try
    {
        value = m_yamlNode_[key].as<bool>();
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::getValueImpl(const std::string &key, std::string& value)
{
    try
    {
        value = m_yamlNode_[key].as<std::string>();
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::setValueImpl(const std::string &key, const int& value)
{
    m_yamlNode_[key] = value;
    return true;
}

bool YamlCfg::setValueImpl(const std::string &key, const double& value)
{
    m_yamlNode_[key] = value;
    return true;
}

bool YamlCfg::setValueImpl(const std::string &key, const bool& value)
{
    m_yamlNode_[key] = value;
    return true;
}

bool YamlCfg::setValueImpl(const std::string &key, const std::string& value)
{
    m_yamlNode_[key] = value;
    return true;
}

bool YamlCfg::getListImpl(const std::string &key, std::vector<CommInfo>& list)
{
    try
    {
        for (const auto& node : m_yamlNode_[key])
        {
            CommInfo info;
            if (node["ID"])
            {
                info.ID = node["ID"].as<std::string>();
            }
            info.IP = node["IP"].as<std::string>();
            info.Port = node["Port"].as<int>();
            list.push_back(info);
        }
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::getListImpl(const std::string &key, std::vector<MsgConfig>& list)
{
    try
    {
        for (const auto& node : m_yamlNode_[key])
        {
            MsgConfig config;
            config.ID = node["ID"].as<std::string>();
            config.send_interval = node["send_interval"].as<int>();
            list.push_back(config);
        }
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::appendToListImpl(const std::string &key, const CommInfo& value)
{
    try
    {
        YAML::Node node;
        if (!value.ID.empty())
        {
            node["ID"] = value.ID;
        }
        node["IP"] = value.IP;
        node["Port"] = value.Port;
        m_yamlNode_[key].push_back(node);
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

bool YamlCfg::appendToListImpl(const std::string &key, const MsgConfig& value)
{
    try
    {
        YAML::Node node;
        node["ID"] = value.ID;
        node["send_interval"] = value.send_interval;
        m_yamlNode_[key].push_back(node);
        return true;
    }
    catch (const YAML::Exception &e)
    {
        return false;
    }
}

