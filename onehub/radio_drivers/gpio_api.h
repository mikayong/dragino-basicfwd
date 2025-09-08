#ifndef __GPIO_API_H
#define __GPIO_API_H 

#include <stdbool.h> 
#include <stdint.h>

// GPIO 编号类型定义
typedef unsigned int gpio_num_t;

// GPIO 方向定义
typedef enum {
    GPIO_MODE_INPUT,     // 输入
    GPIO_MODE_OUTPUT,    // 输出
    GPIO_DIR_IN_OUT  // 双向 (某些平台支持)
} gpio_direction_t;

// GPIO 中断类型定义
typedef enum {
    GPIO_INTR_NONE,         // 无中断
    GPIO_INTR_POSEDGE,      // 上升沿触发                 
    GPIO_INTR_NEGEDGE,      // 下降沿触发
    GPIO_INTR_ANYEDGE,      // 双边沿触发
    GPIO_INTR_HIGH_LEVEL,   // 高电平触发 (某些平台支持)
    GPIO_INTR_LOW_LEVEL     // 低电平触发 (某些平台支持)
} gpio_intr_type_t;

// GPIO 上下拉电阻配置
typedef enum {
    GPIO_PULL_DEFAULT,   // 默认状态
    GPIO_PULL_UP,        // 上拉
    GPIO_PULL_DOWN,      // 下拉
    GPIO_PULL_DISABLE    // 禁用上下拉
} gpio_pull_t;

// 错误码定义
typedef enum {
    GPIO_SUCCESS = 0,
    GPIO_ERROR_INVALID_ARG,
    GPIO_ERROR_NOT_FOUND,
    GPIO_ERROR_ACCESS,
    GPIO_ERROR_IO,
    GPIO_ERROR_NOT_SUPPORTED,
    GPIO_ERROR_UNKNOWN,
    GPIO_ERROR_DEBOUNCE
} gpio_error_t;

// 回调函数类型
typedef void (*gpio_intr_callback_t)(gpio_num_t line_num);

// API 函数声明

// 初始化/释放
gpio_error_t gpio_init(const char *chipname);
void gpio_cleanup(void);

// 单线操作
gpio_error_t gpio_set_direction(gpio_num_t line_num, gpio_direction_t direction);
gpio_error_t gpio_get_direction(gpio_num_t line_num, gpio_direction_t *direction);
gpio_error_t gpio_set_level(gpio_num_t line_num, int level);
gpio_error_t gpio_get_level(gpio_num_t line_num, int *level);
gpio_error_t gpio_set_pull(gpio_num_t line_num, gpio_pull_t pull);
gpio_error_t gpio_set_debounce(gpio_num_t line_num, unsigned int debounce_ms);
gpio_error_t gpio_reset_pin(gpio_num_t pin);

// 多线批量操作
gpio_error_t gpio_bulk_set_directions(gpio_num_t *lines, unsigned int count, gpio_direction_t direction);
gpio_error_t gpio_bulk_set_levels(gpio_num_t *lines, unsigned int count, int *levels);
gpio_error_t gpio_bulk_get_levels(gpio_num_t *lines, unsigned int count, int *levels);

// 中断控制
gpio_error_t gpio_set_intr_type(gpio_num_t line_num, gpio_intr_type_t intr_type);
gpio_error_t gpio_wait_for_interrupt(gpio_num_t line_num, int timeout_ms, bool *interrupted);
gpio_error_t gpio_set_callback(gpio_num_t line_num, gpio_intr_callback_t callback);

// 辅助函数
const char *gpio_error_string(gpio_error_t err);

#endif // __GPIO_API_H
