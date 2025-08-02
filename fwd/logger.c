/*
 * @Author: skerlan
 * @Date: 2025-07-30 10:00:00
 * @Description:  创建日志线程，将日志推送到inproc://logger端口

 * 使用方法：
 * 1. 在main函数中创建日志线程
 * 2. 在需要打印日志的地方，使用push_log函数打印日志
 * 3. 在需要退出日志线程的地方，设置EXIT_SIGNAL为1
 * 4. 在main函数中等待日志线程结束
 * 5. 在main函数中销毁日志线程
 * 6. 在main函数中销毁zmq上下文

 */

#include "zhelpers.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>

// 日志级别
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_DEBUG 8

// 颜色宏
#define COLOR_RESET   "\033[0m"
#define COLOR_INFO    "\033[32m"
#define COLOR_WARN    "\033[33m"
#define COLOR_ERROR   "\033[31m"
#define COLOR_DEBUG   "\033[36m"


static int g_log_level_mask = LOG_LEVEL_INFO | LOG_LEVEL_WARN | LOG_LEVEL_ERROR | LOG_LEVEL_DEBUG;

// 日志打印函数
void print_log_by_level(const char *msg) {
    if (strncmp(msg, "[INFO]", 6) == 0) {
        if (g_log_level_mask & LOG_LEVEL_INFO)
            printf(COLOR_INFO "%s" COLOR_RESET "\n", msg);
    } else if (strncmp(msg, "[WARN]", 6) == 0) {
        if (g_log_level_mask & LOG_LEVEL_WARN)
            printf(COLOR_WARN "%s" COLOR_RESET "\n", msg);
    } else if (strncmp(msg, "[ERROR]", 7) == 0) {
        if (g_log_level_mask & LOG_LEVEL_ERROR)
            printf(COLOR_ERROR "%s" COLOR_RESET "\n", msg);
    } else if (strncmp(msg, "[DEBUG]", 7) == 0) {
        if (g_log_level_mask & LOG_LEVEL_DEBUG)
            printf(COLOR_DEBUG "%s" COLOR_RESET "\n", msg);
    } else {
        printf("%s\n", msg);
    }
}

// 日志处理线程
void* log_pull_thread(void *arg) {
    void *zmq_ctx = arg;
    void *pull_sock = zmq_socket(zmq_ctx, ZMQ_PULL);
    if (!pull_sock) {
        perror("zmq_socket");
        return NULL;
    }
    // 绑定端口
    if (zmq_bind(pull_sock, "inproc://logger") != 0) {
        perror("zmq_bind");
        zmq_close(pull_sock);
        return NULL;
    }

    while (!EXIT_SIGNAL) {
        char *msg = s_recv(pull_sock); 
        if (!msg) continue; 
        print_log_by_level(msg);
        free(msg);
    }

    zmq_close(pull_sock);
    return NULL;
}
