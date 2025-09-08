#include "gpio_api.h"
#include <gpiod.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// 静态全局变量
static struct gpiod_chip *chip = NULL;
static struct gpiod_line **lines = NULL;
static gpio_num_t max_lines = 0;
static gpio_intr_callback_t *callbacks = NULL;
static pthread_t intr_thread;
static volatile bool intr_thread_running = false;

// 内部使用的消费者标签
#define GPIO_API_CONSUMER "gpio-api"

// 互斥锁
static pthread_mutex_t gpio_api_mutex = PTHREAD_MUTEX_INITIALIZER;

// 去抖动结构
typedef struct {
    unsigned int last_time;
    int last_value;
} debounce_info_t;

static debounce_info_t *debounce_info = NULL;

/**
 * @brief 获取错误描述字符串
 */
const char *gpio_error_string(gpio_error_t err)
{
    static const char *strings[] = {
        "Success",
        "Invalid argument",
        "GPIO not found",
        "Access denied",
        "I/O error",
        "Operation not supported",
        "Unknown error",
        "Debounce setting failed"
    };

    if (err >= 0 && err <= GPIO_ERROR_DEBOUNCE) {
        return strings[err];
    }
    return "Invalid error code";
}

/**
 * @brief 初始化GPIO系统
 */
gpio_error_t gpio_init(const char *chipname)
{
    if (!chipname) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);

    // 打开GPIO芯片
    chip = gpiod_chip_open_by_name(chipname);
    if (!chip) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return (errno == EACCES) ? GPIO_ERROR_ACCESS : GPIO_ERROR_NOT_FOUND;
    }

    // 获取芯片支持的GPIO线数量
    max_lines = gpiod_chip_num_lines(chip);
    if (max_lines == 0) {
        gpiod_chip_close(chip);
        chip = NULL;
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_NOT_FOUND;
    }

    // 分配线对象数组
    lines = calloc(max_lines, sizeof(struct gpiod_line *));
    if (!lines) {
        gpiod_chip_close(chip);
        chip = NULL;
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_UNKNOWN;
    }

    // 分配回调数组
    callbacks = calloc(max_lines, sizeof(gpio_intr_callback_t));
    if (!callbacks) {
        free(lines);
        lines = NULL;
        gpiod_chip_close(chip);
        chip = NULL;
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_UNKNOWN;
    }

    // 分配去抖动信息数组
    debounce_info = calloc(max_lines, sizeof(debounce_info_t));
    if (!debounce_info) {
        free(callbacks);
        callbacks = NULL;
        free(lines);
        lines = NULL;
        gpiod_chip_close(chip);
        chip = NULL;
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_UNKNOWN;
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 清理GPIO资源
 */
void gpio_cleanup(void)
{
    pthread_mutex_lock(&gpio_api_mutex);

    // 停止中断线程
    if (intr_thread_running) {
        intr_thread_running = false;
        pthread_join(intr_thread, NULL);
    }

    // 释放所有已分配的线
    if (lines) {
        for (gpio_num_t i = 0; i < max_lines; i++) {
            if (lines[i]) {
                gpiod_line_release(lines[i]);
                lines[i] = NULL;
            }
        }
        free(lines);
        lines = NULL;
    }

    // 释放其他资源
    if (callbacks) {
        free(callbacks);
        callbacks = NULL;
    }

    if (debounce_info) {
        free(debounce_info);
        debounce_info = NULL;
    }

    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }

    max_lines = 0;
    pthread_mutex_unlock(&gpio_api_mutex);
}

/**
 * @brief 获取GPIO线对象 (内部使用)
 */
static struct gpiod_line *get_line(gpio_num_t line_num)
{
    if (!chip || line_num >= max_lines) {
        return NULL;
    }

    // 如果线对象尚未创建，则创建它
    if (!lines[line_num]) {
        lines[line_num] = gpiod_chip_get_line(chip, line_num);
    }

    return lines[line_num];
}

/**
 * @brief 中断处理线程 (内部使用)
 */
static void *intr_thread_func(void *arg)
{
    (void)arg;
    
    struct pollfd *pfds = malloc(max_lines * sizeof(struct pollfd));
    if (!pfds) {
        return NULL;
    }

    // 初始化pollfd数组
    unsigned int active_lines = 0;
    for (gpio_num_t i = 0; i < max_lines; i++) {
        if (callbacks[i] && lines[i]) {
            pfds[active_lines].fd = gpiod_line_event_get_fd(lines[i]);
            pfds[active_lines].events = POLLIN | POLLPRI;
            active_lines++;
        }
    }

    while (intr_thread_running && active_lines > 0) {
        int ret = poll(pfds, active_lines, 100); // 100ms超时

        if (ret > 0) {
            // 检查每个文件描述符
            for (gpio_num_t i = 0; i < max_lines; i++) {
                if (callbacks[i] && lines[i]) {
                    int fd = gpiod_line_event_get_fd(lines[i]);
                    for (unsigned int j = 0; j < active_lines; j++) {
                        if (pfds[j].fd == fd && (pfds[j].revents & (POLLIN | POLLPRI))) {
                            // 读取事件以避免后续触发
                            struct gpiod_line_event event;
                            if (gpiod_line_event_read(lines[i], &event) == 0) {
                                callbacks[i](i); // 调用回调函数
                            }
                            break;
                        }
                    }
                }
            }
        } else if (ret < 0 && errno != EINTR) {
            break; // 发生错误
        }
    }

    free(pfds);
    return NULL;
}

/**
 * @brief 启动中断线程 (内部使用)
 */
static gpio_error_t start_intr_thread(void)
{
    if (intr_thread_running) {
        return GPIO_SUCCESS;
    }

    intr_thread_running = true;
    if (pthread_create(&intr_thread, NULL, intr_thread_func, NULL) != 0) {
        intr_thread_running = false;
        return GPIO_ERROR_UNKNOWN;
    }

    return GPIO_SUCCESS;
}

/**
 * @brief 设置GPIO方向
 */
gpio_error_t gpio_set_direction(gpio_num_t line_num, gpio_direction_t direction)
{
    if (line_num >= max_lines) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    // 如果线已经被请求，先释放
    if (gpiod_line_is_used(line)) {
        gpiod_line_release(line);
    }

    int ret = -1;

    switch (direction) {
    case GPIO_MODE_INPUT:
        ret = gpiod_line_request_input(line, GPIO_API_CONSUMER);
        break;
    case GPIO_MODE_OUTPUT:
        ret = gpiod_line_request_output(line, GPIO_API_CONSUMER, 0);
        break;
    case GPIO_DIR_IN_OUT:
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_NOT_SUPPORTED;
    default:
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    if (ret < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return (errno == EACCES) ? GPIO_ERROR_ACCESS : GPIO_ERROR_IO;
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 批量设置GPIO方向
 */
gpio_error_t gpio_bulk_set_directions(gpio_num_t *lines, unsigned int count, gpio_direction_t direction)
{
    if (!lines || count == 0) {
        return GPIO_ERROR_INVALID_ARG;
    }

    gpio_error_t ret = GPIO_SUCCESS;
    for (unsigned int i = 0; i < count; i++) {
        gpio_error_t err = gpio_set_direction(lines[i], direction);
        if (err != GPIO_SUCCESS) {
            ret = err; // 返回最后一个错误
        }
    }

    return ret;
}

/**
 * @brief 获取GPIO方向
 */
gpio_error_t gpio_get_direction(gpio_num_t line_num, gpio_direction_t *direction)
{
    if (line_num >= max_lines || !direction) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    int dir = gpiod_line_direction(line);
    if (dir == GPIOD_LINE_DIRECTION_INPUT) {
        *direction = GPIO_MODE_INPUT;
    } else if (dir == GPIOD_LINE_DIRECTION_OUTPUT) {
        *direction = GPIO_MODE_OUTPUT;
    } else {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_UNKNOWN;
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 设置GPIO电平
 */
gpio_error_t gpio_set_level(gpio_num_t line_num, int level)
{
    if (line_num >= max_lines || (level != 0 && level != 1)) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    if (gpiod_line_set_value(line, level) < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return (errno == EACCES) ? GPIO_ERROR_ACCESS : GPIO_ERROR_IO;
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 批量设置GPIO电平
 */
gpio_error_t gpio_bulk_set_levels(gpio_num_t *lines, unsigned int count, int *levels)
{
    if (!lines || !levels || count == 0) {
        return GPIO_ERROR_INVALID_ARG;
    }

    gpio_error_t ret = GPIO_SUCCESS;
    for (unsigned int i = 0; i < count; i++) {
        gpio_error_t err = gpio_set_level(lines[i], levels[i]);
        if (err != GPIO_SUCCESS) {
            ret = err; // 返回最后一个错误
        }
    }

    return ret;
}

/**
 * @brief 获取GPIO电平
 */
gpio_error_t gpio_get_level(gpio_num_t line_num, int *level)
{
    if (line_num >= max_lines || !level) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    int val = gpiod_line_get_value(line);
    if (val < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return (errno == EACCES) ? GPIO_ERROR_ACCESS : GPIO_ERROR_IO;
    }

    *level = val;
    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 批量获取GPIO电平
 */
gpio_error_t gpio_bulk_get_levels(gpio_num_t *lines, unsigned int count, int *levels)
{
    if (!lines || !levels || count == 0) {
        return GPIO_ERROR_INVALID_ARG;
    }

    gpio_error_t ret = GPIO_SUCCESS;
    for (unsigned int i = 0; i < count; i++) {
        gpio_error_t err = gpio_get_level(lines[i], &levels[i]);
        if (err != GPIO_SUCCESS) {
            ret = err; // 返回最后一个错误
        }
    }

    return ret;
}

/**
 * @brief 设置GPIO上下拉电阻
 */
gpio_error_t gpio_set_pull(gpio_num_t line_num, gpio_pull_t pull)
{
    if (line_num >= max_lines) {
        return GPIO_ERROR_INVALID_ARG;
    }

    // 注意: libgpiod 1.5+ 才支持上下拉设置
    // 这里只是示例，实际实现取决于硬件支持
    return GPIO_ERROR_NOT_SUPPORTED;
}

/**
 * @brief 设置GPIO去抖动时间
 */
gpio_error_t gpio_set_debounce(gpio_num_t line_num, unsigned int debounce_ms)
{
    if (line_num >= max_lines) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    debounce_info[line_num].last_time = 0;
    debounce_info[line_num].last_value = -1;
    pthread_mutex_unlock(&gpio_api_mutex);

    // 实际去抖动逻辑需要在中断处理中实现
    return GPIO_SUCCESS;
}

/**
 * @brief 设置GPIO中断类型
 */
gpio_error_t gpio_set_intr_type(gpio_num_t line_num, gpio_intr_type_t intr_type)
{
    if (line_num >= max_lines) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    // 如果线已经被请求，先释放
    if (gpiod_line_is_used(line)) {
        gpiod_line_release(line);
    }

    int ret = -1;

    switch (intr_type) {
    case GPIO_INTR_NONE:
        ret = gpiod_line_request_input(line, GPIO_API_CONSUMER);
        break;
    case GPIO_INTR_POSEDGE:
        ret = gpiod_line_request_rising_edge_events(line, GPIO_API_CONSUMER);
        break;
    case GPIO_INTR_NEGEDGE:
        ret = gpiod_line_request_falling_edge_events(line, GPIO_API_CONSUMER);
        break;
    case GPIO_INTR_ANYEDGE:
        ret = gpiod_line_request_both_edges_events(line, GPIO_API_CONSUMER);
        break;
    case GPIO_INTR_HIGH_LEVEL:
    case GPIO_INTR_LOW_LEVEL:
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_NOT_SUPPORTED;
    default:
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    if (ret < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return (errno == EACCES) ? GPIO_ERROR_ACCESS : GPIO_ERROR_IO;
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 等待GPIO中断
 */
gpio_error_t gpio_wait_for_interrupt(gpio_num_t line_num, int timeout_ms, bool *interrupted)
{
    if (line_num >= max_lines || !interrupted) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    struct gpiod_line *line = get_line(line_num);
    if (!line) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_INVALID_ARG;
    }

    int fd = gpiod_line_event_get_fd(line);
    if (fd < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_IO;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN | POLLPRI,
        .revents = 0
    };

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_ERROR_IO;
    } else if (ret == 0) {
        *interrupted = false;
        pthread_mutex_unlock(&gpio_api_mutex);
        return GPIO_SUCCESS; // 超时不是错误
    }

    // 检查是否是去抖动导致的假触发
    if (debounce_info[line_num].last_time > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned int current_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        
        if (current_time - debounce_info[line_num].last_time < debounce_info[line_num].last_time) {
            *interrupted = false;
            pthread_mutex_unlock(&gpio_api_mutex);
            return GPIO_SUCCESS;
        }
    }

    *interrupted = true;
    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}

/**
 * @brief 设置GPIO中断回调
 */
gpio_error_t gpio_set_callback(gpio_num_t line_num, gpio_intr_callback_t callback)
{
    if (line_num >= max_lines) {
        return GPIO_ERROR_INVALID_ARG;
    }

    pthread_mutex_lock(&gpio_api_mutex);
    
    // 设置回调
    callbacks[line_num] = callback;

    // 如果设置了回调且中断线程未运行，则启动它
    if (callback && !intr_thread_running) {
        gpio_error_t err = start_intr_thread();
        if (err != GPIO_SUCCESS) {
            pthread_mutex_unlock(&gpio_api_mutex);
            return err;
        }
    }

    pthread_mutex_unlock(&gpio_api_mutex);
    return GPIO_SUCCESS;
}
