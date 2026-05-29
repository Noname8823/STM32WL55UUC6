/*
 * rf_packet.h
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */

#ifndef INC_RF_PACKET_H_
#define INC_RF_PACKET_H_

#include <stdint.h>

#define RF_PACKET_MAGIC     0xA5
#define RF_PACKET_MAX_DATA  64
#define RF_BROADCAST_ID     255

typedef enum
{
    RF_PKT_INPUT = 1,
    RF_PKT_RS485 = 2,
    RF_PKT_ACK = 3
} RfPacketType_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t src_id;
    uint8_t dst_id;
    uint8_t type;
    uint8_t seq;
    uint8_t input_mask;
    uint8_t len;
    uint8_t data[RF_PACKET_MAX_DATA];
} RfPacket_t;

uint8_t RFPacket_IsValid(RfPacket_t *pkt, uint8_t size);
uint8_t RFPacket_IsForMe(RfPacket_t *pkt, uint8_t my_id);

#endif /* INC_RF_PACKET_H_ */
