/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        config_wrapper.h
Version:     1.0
Author:      cjx
start date:
Description: 配置文件管理工厂
    通过配置文件调用其他基类，进行构造，赋值给他们指向指定子类对象的指针
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef CONFIG_WRAPPER_H_
#define CONFIG_WRAPPER_H_

#include <memory>

#include "utils/singleton.h"

class ConfigWrapper
{
public:
    friend SingletonTemplate<ConfigWrapper>;

    int loadCfgFile(const std::string &cfgPath);
    
    // 返回配置文件实例指针
    ConfigWrapper &getCfgInstance()
    {
        return *m_cfgPointer_;
    }

protected:
    // 一般不会用到（会先load, 有了配置路径了），设置配置文件保存路径
    int setSavePath(const std::string &cfgPath);
    // 通常查询接口
    virtual bool getValue(const std::string &key, std::string &value) = 0;
    // 通常设置接口
    virtual bool setValue(const std::string &key, const std::string &value) = 0;

    enum class FileType
    {
        FILE_TYPE_JSON = 0,
        FILE_TYPE_YAML,
        FILE_TYPE_UNKNOWN
    };

    // 识别文件类型
    FileType identifyFileType(const std::string &cfgPath);

private:
    std::unique_ptr<ConfigWrapper> m_cfgPointer_;

};


#endif  // CONFIG_WRAPPER_H_