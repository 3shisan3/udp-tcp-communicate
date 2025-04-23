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

#ifndef YAML_CFG_H_
#define YAML_CFG_H_

#include "config_interface.h"

#include "utils/yaml.hpp"

class YamlCfg : public ConfigInterface
{
public:
    YamlCfg() = default;
    ~YamlCfg() = default;

    bool loadCfgFile(const std::string &cfgPath) override;
    bool saveCurCfg(const std::string &cfgPath) override;

protected:
    bool getValueImpl(const std::string &key, int& value) override;
    bool getValueImpl(const std::string &key, double& value) override;
    bool getValueImpl(const std::string &key, bool& value) override;
    bool getValueImpl(const std::string &key, std::string& value) override;
    
    bool setValueImpl(const std::string &key, const int& value) override;
    bool setValueImpl(const std::string &key, const double& value) override;
    bool setValueImpl(const std::string &key, const bool& value) override;
    bool setValueImpl(const std::string &key, const std::string& value) override;

    bool getListImpl(const std::string &key, std::vector<CommInfo>& list) override;
    bool getListImpl(const std::string &key, std::vector<MsgConfig>& list) override;
    bool appendToListImpl(const std::string &key, const CommInfo& value) override;
    bool appendToListImpl(const std::string &key, const MsgConfig& value) override;

private:
    YAML::Node m_yamlNode_;
};

#endif // YAML_CFG_H_