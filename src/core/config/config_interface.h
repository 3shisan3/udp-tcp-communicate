/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        config_interface.h
Version:     1.0
Author:      cjx
start date:
Description: 配置文件抽象类
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef CONFIG_INTERFACE_H_
#define CONFIG_INTERFACE_H_

#include <string>

#include "utils/yaml.hpp"
class ConfigInterface
{
public:
    // 加载配置文件
    virtual bool loadCfgFile(const std::string &cfgPath) = 0;
    // 保存配置到本地(传空，默认使用加载的配置文件路径)
    virtual bool saveCurCfg(const std::string &cfgPath = "") = 0;

    // 通常查询接口
    template <typename T>
    T getValue(const std::string &key, const T &defaultValue)
    {
        if (!m_yamlNode.IsMap() || !m_yamlNode[key])
        {
            return defaultValue;
        }
        try
        {
            return m_yamlNode[key].as<T>();
        }
        catch (const YAML::BadConversion &)
        {
            return defaultValue;
        }
    }

    // 通常设置接口
    template <typename T>
    bool setValue(const std::string &key, const T &value)
    {
        try
        {
            m_yamlNode[key] = value;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

protected:
    YAML::Node m_yamlNode;
    std::string m_savePath_;
};


#endif  // CONFIG_INTERFACE_H_