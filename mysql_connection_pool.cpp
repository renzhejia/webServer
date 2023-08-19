//#include "mysql_connection_pool.h"
//
//connection_pool *connection_pool::GetInstance()
//{
//    static connection_pool connPool;
//    return &connPool;
//}
//
//// 构造函数
//connection_pool::connection_pool()
//{
//    this->CurConn = 0;
//    this->FreeConn = 0;
//}
//
//// RAII机制销毁连接池,析构函数
//connection_pool::~connection_pool()
//{
//    DestoryPool();
//}
//
//// 构造初始化
//void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn)
//{
//    // 初始化数据库信息
//    this->Url = url;
//    this->Port = Port;
//    this->User = User;
//    this->PassWord = PassWord;
//    this->DatabaseName = DataBaseName;
//
//    // 创建MaxConn条数据库连接
//    for (int i = 0; i < MaxConn; i++)
//    {
//        MYSQL *con = NULL;
//        // 分配或初始化与mysql_real_connect()相适应的MYSQL对象,果mysql是NULL指针，
//        // 该函数将分配、初始化、并返回新对象。否则，将初始化对象，并返回对象的地址。
//        con = mysql_init(con);
//
//        if (con == NULL)
//        {
//            cout << "Error: " << mysql_error(con);
//            exit(1);
//        }
//
//        // mysql_real_connect()尝试与运行在主机上的MySQL数据库引擎建立连接。
//        // c_str是将string类型转换为char类型
//        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),
//                                 DataBaseName.c_str(), Port, NULL, 0);
//
//        if (con == NULL)
//        {
//            cout << "Error: " << mysql_error(con);
//            exit(1);
//        }
//
//        // 更新连接池和空闲连接的数量
//        connList.push_back(con);
//        ++FreeConn;
//    }
//
//    // 将信号量初始化为最大连接次数
//    reserve = sem(FreeConn);
//
//    this->MaxConn = FreeConn;
//}
//
//// 获取数据库连接
//MYSQL *connection_pool::GetConnection()
//{
//    MYSQL *con = NULL;
//
//    // 取出连接，信号量原子减1，为0则等待
//    reserve.wait();
//
//    lock.lock();
//
//    con = connList.front();
//    connList.pop_front();
//
//    --FreeConn;
//    ++CurConn;
//
//    lock.unlock();
//    return con;
//}
//
//bool connection_pool::ReleaceConnection(MYSQL *conn)
//{
//    if (conn == NULL)
//    {
//        return false;
//    }
//
//    lock.lock();
//    connList.push_back(conn);
//    ++FreeConn;
//    --CurConn;
//    lock.unlock();
//
//    reserve.post();
//    return true;
//}
//
//// 销毁连接池
//void connection_pool::DestoryPool()
//{
//    lock.lock();
//    if (connList.size() > 0)
//    {
//        list<MYSQL *>::iterator it;
//        for (it = connList.begin(); it != connList.end(); it++)
//        {
//            MYSQL *con = *it;
//            mysql_close(con);
//        }
//
//        FreeConn = 0;
//        CurConn = 0;
//
//        // 清空List
//        connList.clear();
//        lock.unlock();
//        return;
//    }
//    lock.unlock();
//}
//
//int connection_pool::GetFreeConn()
//{
//    return this->FreeConn;
//}
//
//// 不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行获取和释放
//connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
//{
//    *SQL = connPool->GetConnection();
//
//    conRAII = *SQL;
//    poolRAII = connPool;
//}
//
//connectionRAII::~connectionRAII()
//{
//    poolRAII->ReleaceConnection(conRAII);
//}