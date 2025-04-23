#include "yaml_cfg.h"

#include <fstream>

bool YamlCfg::loadCfgFile(const std::string &cfgPath)
{
    try
    {
        m_yamlNode = YAML::LoadFile(cfgPath);
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
        fout << m_yamlNode; // 写入文件
        return true;
    }
    catch (...)
    {
        return false;
    }
    return true;
}
