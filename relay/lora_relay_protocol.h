/**
 * @file lora_relay_protocol.h
 * @brief LoraRelay协议栈数据结构和函数声明
 *
 * 定义了LoraRelay协议栈的三种协议包（上发、下发和事件）的数据结构
 * 以及相关的打包、解包函数声明。
 *
 * 协议采用大端字节序进行网络传输，代码中已实现自动字节序转换，
 * 可在大端和小端系统上正确运行。
 */

#ifndef LORA_RELAY_PROTOCOL_H
#define LORA_RELAY_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @defgroup LORA_RELAY_CONFIG 协议配置参数
 * @{
 */
#define MAX_PHY_PAYLOAD_LEN    245  /* PHY最大负载长度 255 - relay_payload_len(8bytes) */
#define MAX_EVENT_PAYLOAD_LEN  240  /* 事件最大负载长度 */
/** @} */

/**
 * @defgroup LORA_RELAY_TYPES 协议类型定义
 * @{
 */

/**
 * @enum meta_type
 * @brief MHDR中的meta_type字段定义
 *
 * 占3位(位7-5)，表示元数据类型
 */
enum meta_type {
    LORAWAN_TYPE = 0x07  /* LoRaWAN类型，二进制111 */
};

typedef enum meta_type meta_type_t;

/**
 * @enum payload_type
 * @brief MHDR中的payload_type字段定义
 *
 * 占2位(位4-3)，表示负载类型
 */
enum payload_type {
    UPLINK_TYPE   = 0x00,  /* 上发类型，二进制00 */
    DOWNLINK_TYPE = 0x01,  /* 下发类型，二进制01 */
    EVENT_TYPE    = 0x03   /* 事件类型，二进制11 */
};

typedef enum payload_type payload_type_t;

/**
 * @enum event_type
 * @brief 事件类型定义
 */
enum event_type {
    EVENT_JOIN    = 0x01,  /* 加入事件 */
    EVENT_RESET   = 0x02,  /* 重置事件 */
    EVENT_ERROR   = 0x03,  /* 错误事件 */
    EVENT_TIMEOUT = 0x04   /* 超时事件 */
};

typedef enum event_type event_type_t;

/** @} */

/**
 * @defgroup LORA_RELAY_PACKETS 协议包结构定义
 * @{
 */

/**
 * @struct uplink_packet
 * @brief 上发协议包结构
 *
 * 格式: MHDR(1字节) | Uplink META(5字节) | PHY payload(0-n字节)
 */
struct uplink_packet {
    meta_type_t meta_type;       /* 元数据类型 */
    payload_type_t payload_type; /* 负载类型，应设为UPLINK_TYPE */
    uint8_t hop_count;           /* 跳数(0-7) */
    
    /* Uplink META字段 */
    uint16_t uplink_id;          /* 上发ID(12位) */
    uint8_t data_rate;           /* 数据速率(0-15) */
    int8_t rssi;                 /* 接收信号强度指示 */
    int8_t snr;                  /* 信噪比(-32到31) */
    uint8_t channel;             /* 信道 */
    
    uint8_t phy_payload[MAX_PHY_PAYLOAD_LEN]; /* PHY负载数据 */
    uint16_t payload_len;                     /* PHY负载长度 */
};

typedef struct uplink_packet uplink_packet_t;

/**
 * @struct downlink_packet
 * @brief 下发协议包结构
 *
 * 格式: MHDR(1字节) | Dwlink META(7字节) | count_us(4字节) | PHY payload(0-n字节)
 */
struct downlink_packet {
    meta_type_t meta_type;       /* 元数据类型 */
    payload_type_t payload_type; /* 负载类型，应设为DOWNLINK_TYPE */
    uint8_t hop_count;           /* 跳数(0-7) */
    
    /* Dwlink META字段 */
    uint16_t dwlink_id;          /* 下发ID(12位) */
    uint8_t data_rate;           /* 数据速率(0-15) */
    uint32_t frequency;          /* 下发频率(32位无符号) */
    uint8_t tx_power;            /* 发射功率(4位) */
    uint8_t delay;               /* 延迟(4位) */
    
    uint32_t count_us;                       /* 微秒计数 */
    uint8_t phy_payload[MAX_PHY_PAYLOAD_LEN]; /* PHY负载数据 */
    uint16_t payload_len;                     /* PHY负载长度 */
};

typedef struct downlink_packet downlink_packet_t;

/**
 * @struct event_packet
 * @brief 事件协议包结构
 *
 * 格式: MHDR(1字节) | Event META(3字节) | EVENT payload(0-n字节)
 */
struct event_packet {
    meta_type_t meta_type;       /* 元数据类型 */
    payload_type_t payload_type; /* 负载类型，应设为EVENT_TYPE */
    uint8_t hop_count;           /* 跳数(0-7) */
    
    /* Event META字段 */
    uint16_t event_id;           /* 事件ID */
    event_type_t event_type;     /* 事件类型 */
    
    uint8_t event_payload[MAX_EVENT_PAYLOAD_LEN]; /* 事件负载数据 */
    uint16_t payload_len;                         /* 事件负载长度 */
};

typedef struct event_packet event_packet_t;

/** @} */

/**
 * @defgroup LORA_RELAY_FUNCTIONS 协议处理函数
 * @{
 */

/**
 * @brief 初始化上发协议包结构体
 *
 * 为uplink_packet_t结构体的所有字段设置默认值
 *
 * @param[in,out] packet 指向要初始化的uplink_packet_t结构体的指针
 */
void init_uplink_packet(uplink_packet_t *packet);

/**
 * @brief 创建上发协议包结构体
 *
 * 为uplink_packet_t结构体分配内存并设置默认值
 *
 * @return 成功返回指向新创建的uplink_packet_t结构体的指针，失败返回NULL
 */
uplink_packet_t *create_uplink_packet(void);

/**
 * @brief 创建上发协议包结构体（带参数）
 *
 * 为uplink_packet_t结构体分配内存并设置指定的值，自动生成上发ID
 *
 * @param[in] data_rate 数据速率
 * @param[in] rssi 接收信号强度指示
 * @param[in] snr 信噪比
 * @param[in] channel 信道
 * @return 成功返回指向新创建的uplink_packet_t结构体的指针，失败返回NULL
 */
uplink_packet_t *create_uplink_packet_with_params(uint8_t data_rate, 
					     int8_t rssi, int8_t snr, uint8_t channel);

/**
 * @brief 初始化下发协议包结构体
 *
 * 为downlink_packet_t结构体的所有字段设置默认值
 *
 * @param[in,out] packet 指向要初始化的downlink_packet_t结构体的指针
 */
void init_downlink_packet(downlink_packet_t *packet);

/**
 * @brief 创建下发协议包结构体
 *
 * 为downlink_packet_t结构体分配内存并设置默认值
 *
 * @return 成功返回指向新创建的downlink_packet_t结构体的指针，失败返回NULL
 */
downlink_packet_t *create_downlink_packet(void);

/**
 * @brief 创建下发协议包结构体（带参数）
 *
 * 为downlink_packet_t结构体分配内存并设置指定的值，自动生成下发ID
 *
 * @param[in] data_rate 数据速率
 * @param[in] frequency 下发频率
 * @param[in] tx_power 发射功率
 * @param[in] delay 延迟
 * @return 成功返回指向新创建的downlink_packet_t结构体的指针，失败返回NULL
 */
downlink_packet_t *create_downlink_packet_with_params(uint8_t data_rate, 
					       uint32_t frequency, uint8_t tx_power, uint8_t delay);

/**
 * @brief 生成随机的12位上发ID
 *
 * 生成一个范围在0到4095（2^12-1）之间的随机无符号整数
 *
 * @return 生成的12位随机上发ID
 */
uint16_t generate_uplink_id(void);

/**
 * @brief 生成随机的12位下发ID
 *
 * 生成一个范围在0到4095（2^12-1）之间的随机无符号整数
 *
 * @return 生成的12位随机下发ID
 */
uint16_t generate_dwlink_id(void);

/**
 * @brief 构建MHDR字节
 *
 * 按照协议定义组合meta_type、payload_type和hop_count到一个字节中
 *
 * @param[in] meta_type 元数据类型（3位）
 * @param[in] payload_type 负载类型（2位）
 * @param[in] hop_count 跳数（3位）
 * @return 组合后的MHDR字节
 */
uint8_t build_mhdr(meta_type_t meta_type, payload_type_t payload_type, uint8_t hop_count);

/**
 * @brief 解析MHDR字节
 *
 * 从MHDR字节中提取meta_type、payload_type和hop_count字段
 *
 * @param[in] mhdr_byte 待解析的MHDR字节
 * @param[out] meta_type 解析出的元数据类型
 * @param[out] payload_type 解析出的负载类型
 * @param[out] hop_count 解析出的跳数
 * @return 解析成功返回true，参数为NULL则返回false
 */
bool parse_mhdr(uint8_t mhdr_byte, meta_type_t *meta_type,
               payload_type_t *payload_type, uint8_t *hop_count);

/**
 * @brief 打包上发协议包
 *
 * 将UplinkPacket结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向UplinkPacket结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 */
uint8_t *pack_uplink_packet(const uplink_packet_t *packet, uint16_t *out_len);

/**
 * @brief 解包上发协议包
 *
 * 将接收到的字节流解析为UplinkPacket结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的UplinkPacket结构体的指针
 * @return 解包成功返回true，失败返回false
 */
bool unpack_uplink_packet(const uint8_t *data, uint16_t len, uplink_packet_t *packet);

/**
 * @brief 打包下发协议包
 *
 * 将DownlinkPacket结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向DownlinkPacket结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 */
uint8_t *pack_downlink_packet(const downlink_packet_t *packet, uint16_t *out_len);

/**
 * @brief 解包下发协议包
 *
 * 将接收到的字节流解析为DownlinkPacket结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的DownlinkPacket结构体的指针
 * @return 解包成功返回true，失败返回false
 */
bool unpack_downlink_packet(const uint8_t *data, uint16_t len, downlink_packet_t *packet);

/**
 * @brief 打包事件协议包
 *
 * 将EventPacket结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向EventPacket结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 */
uint8_t *pack_event_packet(const event_packet_t *packet, uint16_t *out_len);

/**
 * @brief 解包事件协议包
 *
 * 将接收到的字节流解析为EventPacket结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的EventPacket结构体的指针
 * @return 解包成功返回true，失败返回false
 */
bool unpack_event_packet(const uint8_t *data, uint16_t len, event_packet_t *packet);

/** @} */

#endif /* LORA_RELAY_PROTOCOL_H */

