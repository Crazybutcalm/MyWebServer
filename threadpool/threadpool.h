#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<cstdio>
#include<exception>
#include<pthread.h>
#include<list>
#include "../lock/lock.h"

template<class T>
class threadpool{
public:
    threadpool(int thread_number=8, int max_requests=10000);
    ~threadpool();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int m_thread_number;//线程数量
    int m_max_requests;//最大请求数量
    pthread_t* m_threads; //描述线程池的数组
    std::list<T*> m_workqueue;//请求队列
    locker m_queuelocker; // 互斥锁
    sem m_queststat; //是否有任务需要处理
    bool m_stop;
};

//创建线程池
template<class T>
threadpool<T>::threadpool(int thread_number, int max_requests):
        m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),
        m_stop(false)
{
    if((thread_number<=0)||(max_requests<=0)){
        throw std::exception();
    }

    m_threads = new pthread_t [m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i=0;i<m_thread_number;i++){
        printf("create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this)!=0){
            delete [] m_threads;
            throw std::exception();
        }

        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<class T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}

//加入到请求队列
template<class T>
bool threadpool<T>::append(T* request){
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queststat.post();
    return true;
}

//C++中pthread_creat的第三个参数必须是静态函数,可以在这个静态函数中传递对象，调用动态方法
template<class T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool* ) arg;
    pool->run();
    return pool;
}

//调用HTTP接口函数.
template<class T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queststat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}
#endif