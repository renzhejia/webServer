#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"
#include "lst_timer.h"
#include "log.h"

#define MAX_FD 65536           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量
#define TIMESLOT 5             // 最小超时单位
static int sig_pipefd[2];

#define SYNLOG // 同步写日志
// #define ASYNLOG //异步写日志

// 添加文件描述符
extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);
extern int setnonblocking(int fd);

static sort_timer_lst timer_lst; // 定时器链表

// 设置信号处理函数
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之。
void cb_func(client_data *user_data)
{
    if (user_data->sockfd != -1)
    {
        epoll_ctl(http_conn::m_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
        // printf("close %d\n", user_data->sockfd);
        LOG_INFO("close %d\n", user_data->sockfd);
        close(user_data->sockfd);
        user_data->sockfd = -1;
        http_conn::m_user_count--; // 关闭一个连接，将客户总数量-1
    }
}

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
int main(int argc, char *argv[])
{
#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0); // 同步日志模型
#endif

#ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8); // 异步日志模型
#endif

    if (argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]);

    addsig(SIGPIPE);
    addsig(SIGALRM);
    addsig(SIGTERM);

    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD];

    bool timeout = false;
    alarm(TIMESLOT); // 定时,5秒后产生SIGALARM信号

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // 端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    ret = listen(listenfd, 5);

    // 创建epoll对象，和事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    // 添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;
    socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0], false);
    // 创建用户定时器数组
    client_data *users_timer = new client_data[MAX_FD];

    while (true)
    {

        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR))
        {
            // printf("epoll failure\n");
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {

            int sockfd = events[i].data.fd;

            if (sockfd == listenfd)
            {
                // 有新客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);

                if (connfd < 0)
                {
                    // printf("errno is: %d\n", errno);
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
                // 这里我刚开始的做法是直接将http_conn类作为定时器中的client_data，然后将定时器类直接整合到http_conn中
                // 但这样效果不好。这样创建一个user专门作为http——conn连接，另一个user——timer作为定时器，都用connfd作为索引
                // 比之前的写法耦合性低得多
                util_timer *timer = new util_timer;
                users[connfd].init(connfd, client_address);
                users_timer[connfd].address = client_address;
                users_timer[connfd].sockfd = connfd;
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                timer->user_data = &users_timer[connfd];
                users_timer[connfd].timer = timer;
                timer_lst.add_timer(timer);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {

                users[sockfd].close_conn();
            }
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                // 对于信号的处理干脆直接在main函数中写了
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM:
                        {
                            // 遇到定时器发出的信号，进行对该信号的处理
                            timeout = true;
                            break;
                        }
                        case SIGTERM:

                        case SIGPIPE:

                        default:
                        {
                        }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].read())
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].m_address.sin_addr));
                    Log::get_instance()->flush();
                    // 若监测到读事件，将该事件放入请求队列
                    pool->append(users + sockfd);

                    // 因为有可读事件发生，所以定时器的寿命要重新设置
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                    }
                }
                else
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }

                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                util_timer *timer = users_timer[sockfd].timer;
                if (!users[sockfd].write())
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                    users[sockfd].close_conn();
                }
                else
                {
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].m_address.sin_addr));
                    Log::get_instance()->flush();
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                    }
                }
            }
        }
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete[] users_timer;
    delete pool;
    return 0;
}