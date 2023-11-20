#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

// 默认为同步日志
Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

// 日志文件描述符关闭
Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}
//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size,则设置为异步，同步队列的长度超过1，则表示日志先进行存储再写入到文件，则为异步
    if (max_queue_size >= 1)
    {
        // 设置为异步
        m_is_async = true;
        // 创建队列的大小
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志flush_log_thread循环从队列中取日志，并写入到文件中
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    // 是否关闭日志功能
    m_close_log = close_log;
    // 日志缓存的大小
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    // 日志的行数
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 从右边开始找/
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // 没有/
    if (p == NULL)
    {
        // 创建日志全名
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        // p+1开始就是日志名
        strcpy(log_name, p + 1);
        // 将file_name前面路径保存起来
        strncpy(dir_name, file_name, p - file_name + 1);
        // 创建完整的日志文件的名称
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;
    
    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

// 这里每次写日志前都会判断当前日志文件的时间和当前时间是否匹配，否则创建新的日志文件
void Log::write_log(int level, const char *format, ...)
{
    // 获取时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;

    // 时间不匹配或者超过了最大行
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
       
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        // 从新开启一个新的日志文件夹
        if (m_today != my_tm.tm_mday)
        {
            // 新日志的文件夹名称
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            // 重置时间和行数
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            // 如果是行数超过了
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
 
    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();
    // 为异步时，就先放入到队列中，由线程取写入指定的文件
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        // 否则直接写入日志文件
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
