/*************************************************************
 *循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
 *线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
 **************************************************************/
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <queue>
#include "locker.h"
using namespace std;
template <class T>
class block_queue
{
public:
    // 阻塞队列构造函数
    block_queue(int max_size = 100)
    {
        if (max_size <= 0)
        {
            exit(-1);
        }
        m_max_size = max_size;
        //m_array = new queue<T>(m_max_size);
    }

    void clear()
    {
        m_mutex.lock();
        while (!m_array.empty())
        {
            m_array.pop();
        }
        m_mutex.unlock();
    }

    ~block_queue()
    {
        m_mutex.lock();
        if (!m_array.empty())
        {
            delete m_array;
        }
        m_mutex.unlock();
    }

    // 判断是否队满
    bool full()
    {
        m_mutex.lock();
        if (m_array.size() >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断是否队空
    bool empty()
    {
        m_mutex.lock();
        if (!m_array.empty())
        {
            m_mutex.unlock();
            return false;
        }
        m_mutex.unlock();
        return true;
    }

    // 返回队首元素
    bool front(T &value)
    {
        m_mutex.lock();
        if (m_array.empty())
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array.front();
        m_mutex.unlock();
        return true;
    }

    // 返回队尾元素
    bool back(T &value)
    {
        m_mutex.lock();
        if (m_array.empty())
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array.back();
        m_mutex.unlock();
        return true;
    }

    int size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_array.size();

        m_mutex.unlock();
        return tmp;
    }

    int max_size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素，需要将所有使用队列的线程先唤醒
    // 当有元素push进队列,相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item)
    {
        m_mutex.lock();
        if (m_array.size() >= m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_array.push(item);
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    // pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T &item)
    {
        m_mutex.lock();
        while (m_array.size() <= 0)
        {
            if (!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }

        item = m_array.back();
        m_array.pop();
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_cond;

    // 利用STL的队列
    queue<T> m_array;
    int m_max_size;
};

#endif