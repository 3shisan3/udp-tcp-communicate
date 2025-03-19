/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        singleton.h
Version:     1.0
Author:      cjx
start date: 2023-8-28
Description: 通用单例方法
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2023-8-28      cjx        create
2             2025-3-02      cjx        兼容无默认构造函数场景

*****************************************************************/

#ifndef SINGLETON_H_
#define SINGLETON_H_

#include <memory>
#include <mutex>
#include <type_traits>

template<class SingletonClass>
class SingletonTemplate final   // 增加final标记，防止继承使用出现违背单例行为的操作
{
public:
    // 删除拷贝构造函数和拷贝赋值运算符
    SingletonTemplate(const SingletonTemplate&) = delete;
    SingletonTemplate& operator=(const SingletonTemplate&) = delete;

    /**
     * @brief 获取单例
     *
     * @return 
     */
    static SingletonClass &getSingletonInstance()
    {
        std::call_once(m_s_singleFlag, [&] {
            m_s_pSingletonInstance.reset(new SingletonClass());
        });

        return *m_s_pSingletonInstance;
    }

    // 非默认构造时
    template<typename... Args>
    static SingletonClass &getSingletonInstance(Args&&... args)
    {
        std::call_once(m_s_singleFlag, [&] {
            m_s_pSingletonInstance.reset(new SingletonClass(std::forward<Args>(args)...));
        });

        return *m_s_pSingletonInstance;
    }

protected:
    // 保护构造函数和析构函数，防止外部实例化或删除
    SingletonTemplate() = default;
    ~SingletonTemplate() = default;

private:
    /**
     * @brief 单例的静态指针
     */
    static std::unique_ptr<SingletonClass> m_s_pSingletonInstance;

    /**
     * @brief 执行一次触发标志
     */
    static std::once_flag m_s_singleFlag;
};

template<class SingletonClass>
std::unique_ptr<SingletonClass> SingletonTemplate<SingletonClass>::m_s_pSingletonInstance = nullptr;

template<class SingletonClass>
std::once_flag SingletonTemplate<SingletonClass>::m_s_singleFlag;

#endif