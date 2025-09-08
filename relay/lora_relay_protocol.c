/**
 * @file lora_relay_protocol.c
 * @brief LoraRelay协议栈功能实现
 *
 * 该文件实现了LoraRelay协议栈的三种协议包（上发、下发和事件）的打包、
 * 解包和解析功能，严格遵循协议格式定义。
 */

#include "lora_relay_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* 检查系统字节序 */
static bool is_big_endian(void)
{
	union {
		uint32_t i;
		char c[4];
	} bint = {0x01020304};

	return bint.c[0] == 1;
}

/* 将大端字节序转换为主机字节序（32位） */
static uint32_t be_to_host32(uint32_t value)
{
	if (is_big_endian())
		return value;
	else {
		/* 小端系统，进行字节交换 */
		return ((value & 0x000000FF) << 24) |
		       ((value & 0x0000FF00) << 8)  |
		       ((value & 0x00FF0000) >> 8)  |
		       ((value & 0xFF000000) >> 24);
	}
}

/* 将主机字节序转换为大端字节序（32位） */
static uint32_t host_to_be32(uint32_t value)
{
	/* 与be_to_host32相同，因为我们需要确保数据以大端格式发送 */
	return be_to_host32(value);
}

/**
 * @brief 生成随机的12位上发ID
 *
 * 生成一个范围在0到4095（2^12-1）之间的随机无符号整数
 *
 * @return 生成的12位随机上发ID
 */
uint16_t generate_uplink_id(void)
{
	static bool initialized = false;
	
	/* 初始化随机数生成器（只在第一次调用时执行） */
	if (!initialized) {
		/* 使用当前时间作为随机数种子 */
		srand((unsigned int)time(NULL));
		initialized = true;
	}
	
	/* 生成0-4095之间的随机数（12位） */
	return (uint16_t)(rand() & 0x0FFF);
}

/**
 * @brief 生成随机的12位下发ID
 *
 * 生成一个范围在0到4095（2^12-1）之间的随机无符号整数
 *
 * @return 生成的12位随机下发ID
 */
uint16_t generate_dwlink_id(void)
{
	/* 直接复用generate_uplink_id函数的实现，因为两者功能相同 */
	return generate_uplink_id();
}

/**
 * @brief 初始化上发协议包结构体
 *
 * 为uplink_packet_t结构体的所有字段设置默认值
 *
 * @param[in,out] packet 指向要初始化的uplink_packet_t结构体的指针
 */
void init_uplink_packet(uplink_packet_t *packet)
{
	/* 检查输入参数是否有效 */
	if (!packet)
		return;
	
	/* 初始化所有字段为0 */
	memset(packet, 0, sizeof(uplink_packet_t));
	
	/* 设置默认值 */
	packet->meta_type = LORAWAN_TYPE;
	packet->payload_type = UPLINK_TYPE;
	packet->hop_count = 0;
	packet->uplink_id = 0;
	packet->data_rate = 0;
	packet->rssi = 0;
	packet->snr = 0;
	packet->channel = 0;
	packet->payload_len = 0;
}

/**
 * @brief 创建上发协议包结构体
 *
 * 为uplink_packet_t结构体分配内存并设置默认值
 *
 * @return 成功返回指向新创建的uplink_packet_t结构体的指针，失败返回NULL
 */
uplink_packet_t *create_uplink_packet(void)
{
	uplink_packet_t *packet = malloc(sizeof(uplink_packet_t));
	if (packet)
		init_uplink_packet(packet);
	return packet;
}

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
					     int8_t rssi, int8_t snr, uint8_t channel)
{
	uplink_packet_t *packet = create_uplink_packet();
	if (packet) {
		packet->uplink_id = generate_uplink_id();
		packet->data_rate = data_rate;
		packet->rssi = rssi;
		packet->snr = snr;
		packet->channel = channel;
	}
	return packet;
}

/**
 * @brief 初始化下发协议包结构体
 *
 * 为downlink_packet_t结构体的所有字段设置默认值
 *
 * @param[in,out] packet 指向要初始化的downlink_packet_t结构体的指针
 */
void init_downlink_packet(downlink_packet_t *packet)
{
	/* 检查输入参数是否有效 */
	if (!packet)
		return;
	
	/* 初始化所有字段为0 */
	memset(packet, 0, sizeof(downlink_packet_t));
	
	/* 设置默认值 */
	packet->meta_type = LORAWAN_TYPE;
	packet->payload_type = DOWNLINK_TYPE;
	packet->hop_count = 0;
	packet->dwlink_id = 0;
	packet->data_rate = 0;
	packet->frequency = 0;
	packet->tx_power = 0;
	packet->delay = 0;
	packet->count_us = 0;
	packet->payload_len = 0;
}

/**
 * @brief 创建下发协议包结构体
 *
 * 为downlink_packet_t结构体分配内存并设置默认值
 *
 * @return 成功返回指向新创建的downlink_packet_t结构体的指针，失败返回NULL
 */
downlink_packet_t *create_downlink_packet(void)
{
	downlink_packet_t *packet = malloc(sizeof(downlink_packet_t));
	if (packet)
		init_downlink_packet(packet);
	return packet;
}

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
					       uint32_t frequency, uint8_t tx_power, uint8_t delay)
{
	downlink_packet_t *packet = create_downlink_packet();
	if (packet) {
		packet->dwlink_id = generate_dwlink_id();
		packet->data_rate = data_rate;
		packet->frequency = frequency;
		packet->tx_power = tx_power;
		packet->delay = delay;
	}
	return packet;
}

/**
 * @brief 构建MHDR字节
 *
 * 按照协议定义组合meta_type、payload_type和hop_count到一个字节中
 *
 * @param[in] meta_type 元数据类型（3位）
 * @param[in] payload_type 负载类型（2位）
 * @param[in] hop_count 跳数（3位）
 * @return 组合后的MHDR字节
 * @note 自动截断超出位数的数据以确保符合协议定义
 */
uint8_t build_mhdr(meta_type_t meta_type, payload_type_t payload_type, uint8_t hop_count)
{
	uint8_t mhdr = 0;
	/* 确保输入值在有效范围内，通过位与操作截断 */
	mhdr |= ((meta_type & 0x07) << 5);	/* 位7-5: meta_type */
	mhdr |= ((payload_type & 0x03) << 3);	/* 位4-3: payload_type */
	mhdr |= (hop_count & 0x07);		/* 位2-0: hop_count */
	return mhdr;
}

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
		 payload_type_t *payload_type, uint8_t *hop_count)
{
	/* 检查输出参数是否有效 */
	if (!meta_type || !payload_type || !hop_count)
		return false;
	
	/* 提取各个字段 */
	*meta_type = (mhdr_byte >> 5) & 0x07;
	*payload_type = (mhdr_byte >> 3) & 0x03;
	*hop_count = mhdr_byte & 0x07;
	
	return true;
}

/**
 * @brief 打包上发协议包
 *
 * 将uplink_packet结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向uplink_packet结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 * @note 调用者必须在使用完毕后释放返回的缓冲区
 * @warning 当payload_len超过MAX_PHY_PAYLOAD_LEN时返回NULL
 */
uint8_t *pack_uplink_packet(const uplink_packet_t *packet, uint16_t *out_len)
{
	/* 参数合法性检查 */
	if (!packet || !out_len || packet->payload_len > MAX_PHY_PAYLOAD_LEN)
		return NULL;
	
	/* 验证包类型是否正确 */
	if (packet->payload_type != UPLINK_TYPE)
		return NULL;
	
	/* 验证meta_type是否为LORAWAN_TYPE */
	if (packet->meta_type != LORAWAN_TYPE)
		return NULL;
	
	/* 计算总长度: MHDR(1) + Uplink META(5) + PHY payload(n) */
	*out_len = 1 + 5 + packet->payload_len;
	uint8_t *buffer = (uint8_t *)malloc(*out_len);
	if (!buffer)
		return NULL;	/* 内存分配失败 */
	
	uint8_t index = 0;
	
	/* 打包MHDR */
	buffer[index++] = build_mhdr(packet->meta_type, packet->payload_type, packet->hop_count);
	
	/* 打包Uplink META */
	/* 第1-2字节: uplink_id(12位，位15-4)和data_rate(4位，位3-0) */
	buffer[index++] = (packet->uplink_id >> 4) & 0xFF;	/* uplink_id高8位 */
	buffer[index++] = ((packet->uplink_id & 0x0F) << 4) | (packet->data_rate & 0x0F);
	
	/* 第3字节: rssi */
	buffer[index++] = (uint8_t)packet->rssi;
	
	/* 第4字节: snr */
	buffer[index++] = (uint8_t)packet->snr;
	
	/* 第5字节: channel */
	buffer[index++] = packet->channel;
	
	/* 打包PHY payload */
	memcpy(&buffer[index], packet->phy_payload, packet->payload_len);
	index += packet->payload_len;
	
	return buffer;
}

/**
 * @brief 解包上发协议包
 *
 * 将接收到的字节流解析为uplink_packet结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的uplink_packet结构体的指针
 * @return 解包成功返回true，失败返回false
 * @note 最小数据长度为6字节(MHDR+Uplink META)
 */
bool unpack_uplink_packet(const uint8_t *data, uint16_t len, uplink_packet_t *packet)
{
	/* 参数合法性检查 */
	if (!data || !packet || len < 6)	/* 最小长度: 1+5=6字节 */
		return false;
	
	uint8_t index = 0;
	
	/* 解析MHDR */
	if (!parse_mhdr(data[index++], &packet->meta_type, &packet->payload_type, &packet->hop_count))
		return false;
	
	/* 验证包类型是否正确（上发类型应为00） */
	if (packet->payload_type != UPLINK_TYPE)
		return false;
	
	/* 验证meta_type是否为LORAWAN_TYPE（111） */
	if (packet->meta_type != LORAWAN_TYPE)
		return false;
	
	/* 解析Uplink META */
	/* 解析uplink_id(12位，位15-4)和data_rate(4位，位3-0) */
	packet->uplink_id = (data[index] << 4) | ((data[index+1] >> 4) & 0x0F);
	packet->data_rate = data[index+1] & 0x0F;
	index += 2;
	
	/* 解析rssi */
	packet->rssi = (int8_t)data[index++];
	
	/* 解析snr并验证范围(-32到31) */
	packet->snr = (int8_t)data[index++];
	if (packet->snr < -32 || packet->snr > 31)
		return false;
	
	/* 解析channel */
	packet->channel = data[index++];
	
	/* 解析PHY payload */
	packet->payload_len = len - index;
	if (packet->payload_len > MAX_PHY_PAYLOAD_LEN)
		return false;
	memcpy(packet->phy_payload, &data[index], packet->payload_len);
	
	return true;
}

/**
 * @brief 打包下发协议包
 *
 * 将downlink_packet结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向downlink_packet结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 * @note 调用者必须在使用完毕后释放返回的缓冲区
 * @warning 当payload_len超过MAX_PHY_PAYLOAD_LEN时返回NULL
 */
uint8_t *pack_downlink_packet(const downlink_packet_t *packet, uint16_t *out_len)
{
	/* 参数合法性检查 */
	if (!packet || !out_len || packet->payload_len > MAX_PHY_PAYLOAD_LEN)
		return NULL;
	
	/* 验证包类型是否正确 */
	if (packet->payload_type != DOWNLINK_TYPE)
		return NULL;
	
	/* 验证meta_type是否为LORAWAN_TYPE */
	if (packet->meta_type != LORAWAN_TYPE)
		return NULL;
	
	/* 计算总长度: MHDR(1) + Dwlink META(7) + count_us(4) + PHY payload(n) */
	*out_len = 1 + 7 + 4 + packet->payload_len;
	uint8_t *buffer = (uint8_t *)malloc(*out_len);
	if (!buffer)
		return NULL;	/* 内存分配失败 */
	
	uint8_t index = 0;
	
	/* 打包MHDR */
	buffer[index++] = build_mhdr(packet->meta_type, packet->payload_type, packet->hop_count);
	
	/* 打包Dwlink META */
	/* 第1-2字节: dwlink_id(12位，位15-4)和data_rate(4位，位3-0) */
	buffer[index++] = (packet->dwlink_id >> 4) & 0xFF;	/* dwlink_id高8位 */
	buffer[index++] = ((packet->dwlink_id & 0x0F) << 4) | (packet->data_rate & 0x0F);
	
	/* 第3-6字节: frequency(32位，大端模式) */
	uint32_t freq_be = host_to_be32(packet->frequency);
	buffer[index++] = (freq_be >> 24) & 0xFF;
	buffer[index++] = (freq_be >> 16) & 0xFF;
	buffer[index++] = (freq_be >> 8) & 0xFF;
	buffer[index++] = freq_be & 0xFF;
	
	/* 第7字节: txpow(4位)和delay(4位) */
	buffer[index++] = ((packet->tx_power & 0x0F) << 4) | (packet->delay & 0x0F);
	
	/* 打包count_us(4字节，大端模式) */
	uint32_t count_us_be = host_to_be32(packet->count_us);
	buffer[index++] = (count_us_be >> 24) & 0xFF;
	buffer[index++] = (count_us_be >> 16) & 0xFF;
	buffer[index++] = (count_us_be >> 8) & 0xFF;
	buffer[index++] = count_us_be & 0xFF;
	
	/* 打包PHY payload */
	memcpy(&buffer[index], packet->phy_payload, packet->payload_len);
	index += packet->payload_len;
	
	return buffer;
}

/**
 * @brief 解包下发协议包
 *
 * 将接收到的字节流解析为downlink_packet结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的downlink_packet结构体的指针
 * @return 解包成功返回true，失败返回false
 * @note 最小数据长度为12字节(MHDR+Dwlink META+count_us)
 */
bool unpack_downlink_packet(const uint8_t *data, uint16_t len, downlink_packet_t *packet)
{
	/* 参数合法性检查 */
	if (!data || !packet || len < 12)	/* 最小长度: 1+7+4=12字节 */
		return false;
	
	uint8_t index = 0;
	
	/* 解析MHDR */
	if (!parse_mhdr(data[index++], &packet->meta_type, &packet->payload_type, &packet->hop_count))
		return false;
	
	/* 验证包类型是否正确（下发类型应为01） */
	if (packet->payload_type != DOWNLINK_TYPE)
		return false;
	
	/* 验证meta_type是否为LORAWAN_TYPE（111） */
	if (packet->meta_type != LORAWAN_TYPE)
		return false;
	
	/* 解析Dwlink META */
	/* 解析dwlink_id(12位，位15-4)和data_rate(4位，位3-0) */
	packet->dwlink_id = (data[index] << 4) | ((data[index+1] >> 4) & 0x0F);
	packet->data_rate = data[index+1] & 0x0F;
	index += 2;
	
	/* 解析frequency(32位，大端模式转主机字节序) */
	uint32_t freq_be = 0;
	freq_be = (uint32_t)data[index++] << 24;
	freq_be |= (uint32_t)data[index++] << 16;
	freq_be |= (uint32_t)data[index++] << 8;
	freq_be |= (uint32_t)data[index++];
	packet->frequency = be_to_host32(freq_be);
	
	/* 解析txpow和delay */
	packet->tx_power = (data[index] >> 4) & 0x0F;
	packet->delay = data[index++] & 0x0F;
	
	/* 解析count_us(32位，大端模式转主机字节序) */
	uint32_t count_us_be = 0;
	count_us_be = (uint32_t)data[index++] << 24;
	count_us_be |= (uint32_t)data[index++] << 16;
	count_us_be |= (uint32_t)data[index++] << 8;
	count_us_be |= (uint32_t)data[index++];
	packet->count_us = be_to_host32(count_us_be);
	
	/* 解析PHY payload */
	packet->payload_len = len - index;
	if (packet->payload_len > MAX_PHY_PAYLOAD_LEN)
		return false;
	memcpy(packet->phy_payload, &data[index], packet->payload_len);
	
	return true;
}

/**
 * @brief 打包事件协议包
 *
 * 将event_packet结构体转换为可传输的字节流格式
 *
 * @param[in] packet 指向event_packet结构体的指针
 * @param[out] out_len 输出打包后的字节流长度
 * @return 成功返回动态分配的字节流缓冲区，失败返回NULL
 * @note 调用者必须在使用完毕后释放返回的缓冲区
 * @warning 当payload_len超过MAX_EVENT_PAYLOAD_LEN时返回NULL
 */
uint8_t *pack_event_packet(const event_packet_t *packet, uint16_t *out_len)
{
	/* 参数合法性检查 */
	if (!packet || !out_len || packet->payload_len > MAX_EVENT_PAYLOAD_LEN)
		return NULL;
	
	/* 验证包类型是否正确 */
	if (packet->payload_type != EVENT_TYPE)
		return NULL;
	
	/* 验证meta_type是否为LORAWAN_TYPE */
	if (packet->meta_type != LORAWAN_TYPE)
		return NULL;
	
	/* 计算总长度: MHDR(1) + Event META(3) + EVENT payload(n) */
	*out_len = 1 + 3 + packet->payload_len;
	uint8_t *buffer = (uint8_t *)malloc(*out_len);
	if (!buffer)
		return NULL;	/* 内存分配失败 */
	
	uint8_t index = 0;
	
	/* 打包MHDR */
	buffer[index++] = build_mhdr(packet->meta_type, packet->payload_type, packet->hop_count);
	
	/* 打包Event META */
	/* 第1-2字节: eventID(大端模式) */
	buffer[index++] = (packet->event_id >> 8) & 0xFF;
	buffer[index++] = packet->event_id & 0xFF;
	
	/* 第3字节: event type */
	buffer[index++] = (uint8_t)packet->event_type;
	
	/* 打包EVENT payload */
	memcpy(&buffer[index], packet->event_payload, packet->payload_len);
	index += packet->payload_len;
	
	return buffer;
}

/**
 * @brief 解包事件协议包
 *
 * 将接收到的字节流解析为event_packet结构体
 *
 * @param[in] data 指向接收数据的指针
 * @param[in] len 接收数据的长度
 * @param[out] packet 指向要填充的event_packet结构体的指针
 * @return 解包成功返回true，失败返回false
 * @note 最小数据长度为4字节(MHDR+Event META)
 */
bool unpack_event_packet(const uint8_t *data, uint16_t len, event_packet_t *packet)
{
	/* 参数合法性检查 */
	if (!data || !packet || len < 4)	/* 最小长度: 1+3=4字节 */
		return false;
	
	uint8_t index = 0;
	
	/* 解析MHDR */
	if (!parse_mhdr(data[index++], &packet->meta_type, &packet->payload_type, &packet->hop_count))
		return false;
	
	/* 验证包类型是否正确（事件类型应为11） */
	if (packet->payload_type != EVENT_TYPE)
		return false;
	
	/* 验证meta_type是否为LORAWAN_TYPE（111） */
	if (packet->meta_type != LORAWAN_TYPE)
		return false;
	
	/* 解析Event META */
	/* 解析eventID(大端模式转主机字节序) */
	packet->event_id = (uint16_t)data[index++] << 8;
	packet->event_id |= (uint16_t)data[index++];
	
	/* 解析event type */
	packet->event_type = (event_type_t)data[index++];
	
	/* 解析EVENT payload */
	packet->payload_len = len - index;
	if (packet->payload_len > MAX_EVENT_PAYLOAD_LEN)
		return false;
	memcpy(packet->event_payload, &data[index], packet->payload_len);
	
	return true;
}
