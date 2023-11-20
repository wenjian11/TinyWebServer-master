#include "config.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "root";
    string databasename = "qgydb";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化，指定了相关变量
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志，根据同步还是异步，创建日志相关信息
    server.log_write();

    //数据库，创建了连接池，并读取了数据库中的数据到map中
    server.sql_pool();

    //线程池，创建线程池的时候，已经为每个线程指定了任务，从任务队列里面取任务出来
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}