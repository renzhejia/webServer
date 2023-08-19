//#ifndef CONNECTION_POOL_H
//#define CONNECTION_POOL_H
//
//#include <stdio.h>
//#include <list>
//#include <mysql/mysql.h>
//#include <error.h>
//#include <string.h>
//#include <iostream>
//#include <string>
//#include "locker.h"
//using namespace std;
//
//class connection_pool
//{
//public:
//    MYSQL *GetConnection();              // 获取数据库连接
//    bool ReleaceConnection(MYSQL *conn); // 释放mysql连接
//    int GetFreeConn();                   // 获取当前空闲连接数
//    void DestoryPool();                  // 销毁所有连接
//    // 局部静态变量单例模式,私有化构造函数和析构函数，想要创建连接池对象只能通过
//    // 该函数进行创建
//    static connection_pool *GetInstance();
//
//    // 初始化
//    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
//
//    connection_pool();
//    ~connection_pool();
//
//private:
//    unsigned int MaxConn;  // 最大连接数
//    unsigned int CurConn;  // 当前已使用的连接数
//    unsigned int FreeConn; // 当前空闲的连接数
//
//private:
//    locker lock;
//    list<MYSQL *> connList; // 连接池
//    sem reserve;
//
//private:
//    string Url;          // 主机地址
//    string Port;         // 数据库端口号
//    string User;         // 登陆数据库用户名
//    string PassWord;     // 登陆数据库密码
//    string DatabaseName; // 使用数据库名
//};
//
//class connectionRAII
//{
//public:
//    // 这里需要注意的是，在获取连接时，通过有参构造对传入的参数进行修改。
//    // 其中数据库连接本身是指针类型，所以参数需要通过双指针才能对其进行修改。
//    // 双指针对MYSQL *con修改
//    connectionRAII(MYSQL **SQL, connection_pool *connPool);
//    ~connectionRAII();
//
//private:
//    MYSQL *conRAII;
//    connection_pool *poolRAII;
//};
//
//#endif // !CONNECTION_POOL_H
