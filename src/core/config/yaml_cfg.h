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

#include "config_interface.h"

class YamlCfg : public ConfigInterface
{
public:
    YamlCfg() = default;
    ~YamlCfg() = default;

    bool loadCfgFile(const std::string &cfgPath) override;
    bool saveCurCfg(const std::string &cfgPath) override;
    
};




#endif // YAML_CFG_H