#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"
#include "locker.h"

class Log
{

public:
    // cpp11之后，使用局部变量懒汉不用加锁
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    // 初始化日志文件方法，可选的参数有日志文件，日志缓冲区大小，最大行数以及最长日志条队列
    bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 500000, int max_queue_size = 0);

    // 异步写日志公有方法，调用私有方法async_write_log
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return NULL;
    }

    // 将输出的内容按照标准格式整理
    void write_log(int level, const char *format, ...);

    // 强制刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();

    // 异步写日志方法
    void *async_write_log()
    {
        string single_log;

        // 从阻塞队列中取出一条日志内容，写入文件
        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            //fputs是一个函数，具有的功能是向指定的文件写入一个字符串
            //（不自动写入字符串结束标记符‘\0’）。成功写入一个字符串后，
            //文件的位置指针会自动后移，函数返回值为非负整数；
            //c_str(): 它返回当前字符串的首字符地址
            //这个的意思就是把从阻塞队列中取出来的日志内容，写到m_fp所指的文件中
            //m_fp指的文件就是日志文件
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
        return NULL;
    }

private:
    char dir_name[128];               // 文件名
    char log_name[128];               // log文件名
    int m_split_lines;                // 日志最大行数
    int m_log_buf_size;               // 日志缓冲区大小
    long long m_count;                // 日志行数记录
    int m_today;                      // 按天分文件，记录当前时间是哪一天
    FILE *m_fp;                       // 打开log的文件指针
    char *m_buf;                      // 要输出的内容
    block_queue<string> *m_log_queue; // 阻塞队列
    bool m_is_async;                  // 是同步写还是异步写
    locker m_mutex;                   // 同步类
};

// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, __VA_ARGS__);
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, __VA_ARGS__);
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, __VA_ARGS__);
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, __VA_ARGS__);
#endif // !LOG