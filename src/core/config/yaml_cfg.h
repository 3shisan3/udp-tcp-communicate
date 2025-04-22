/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        yaml_cfg.h
Version:     1.0
Author:      cjx
start date:
Description: yaml配置文件解析
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef YAML_CFG_H
#define YAML_CFG_H

#include "common/config_wrapper.h"
#include "utils/yaml.hpp"

class YamlCfg : public ConfigWrapper
{
public:
    YamlCfg() = default;
    ~YamlCfg() = default;

    bool loadCfgFile(const std::string &cfgPath);
    bool setSavePath(const std::string &cfgPath);

    bool getValue(const std::string &key, std::string &value) override;
    bool setValue(const std::string &key, const std::string &value) override;

private:
    std::string m_savePath_;
    YAML::Node m_yamlNode_;
};




#endif // YAML_CFG_H