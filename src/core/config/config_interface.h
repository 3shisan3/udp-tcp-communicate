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

#ifndef CONFIG_INTERFACE_H
#define CONFIG_INTERFACE_H

#include <string>
#include <vector>
class ConfigInterface {
public:
    virtual ~ConfigInterface() = default;
    
    // 加载配置文件
    virtual bool loadCfgFile(const std::string &cfgPath) = 0;
    // 保存配置到本地(传空，默认使用加载的配置文件路径)
    virtual bool saveCurCfg(const std::string &cfgPath = "") = 0;

    // 通常查询接口
    template <typename T>
    T getValue(const std::string &key, const T &defaultValue)
    {
        T value;
        if (!getValueImpl(key, value))
        {
            return defaultValue;
        }
        return value;
    }

    // 通常设置接口
    template <typename T>
    bool setValue(const std::string &key, const T &value)
    {
        return setValueImpl(key, value);
    }

    // 列表操作接口
    template <typename T>
    std::vector<T> getList(const std::string &key)
    {
        std::vector<T> list;
        getListImpl(key, list);
        return list;
    }
    template <typename T>
    bool appendToList(const std::string &key, const T &value)
    {
        return appendToListImpl(key, value);
    }

    // 配置文件中使用组合元素
    struct CommInfo
    {
        std::string ID;
        std::string IP;
        int Port;
    };
    struct MsgConfig
    {
        std::string ID;
        int send_interval;
    };

protected:
    // 基础类型实现
    virtual bool getValueImpl(const std::string &key, int& value) = 0;
    virtual bool getValueImpl(const std::string &key, double& value) = 0;
    virtual bool getValueImpl(const std::string &key, bool& value) = 0;
    virtual bool getValueImpl(const std::string &key, std::string& value) = 0;
    // 根据需要添加更多类型的重载

    virtual bool setValueImpl(const std::string &key, const int& value) = 0;
    virtual bool setValueImpl(const std::string &key, const double& value) = 0;
    virtual bool setValueImpl(const std::string &key, const bool& value) = 0;
    virtual bool setValueImpl(const std::string &key, const std::string& value) = 0;
    // 根据需要添加更多类型的重载

    // 列表操作实现
    virtual bool getListImpl(const std::string &key, std::vector<CommInfo>& list) = 0;
    virtual bool getListImpl(const std::string &key, std::vector<MsgConfig>& list) = 0;
    virtual bool appendToListImpl(const std::string &key, const CommInfo& value) = 0;
    virtual bool appendToListImpl(const std::string &key, const MsgConfig& value) = 0;

    std::string m_savePath_;
};

#endif  // CONFIG_INTERFACE_H