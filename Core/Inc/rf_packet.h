#ifndef RF_PACKET_H
#define RF_PACKET_H

#include <stdint.h>
#include <stddef.h>

#define RF_PACKET_MAGIC        0xA55AU
#define RF_PACKET_MAX_DATA     64U
#define RF_BROADCAST_ID        0xFFU

typedef enum
{
    RF_PKT_INPUT = 0x01,
    RF_PKT_RS485 = 0x02,
    RF_PKT_ACK   = 0x03
} RfPacketType_t;

#pragma pack(push, 1)

typedef struct
{
    uint16_t magic;
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  type;
    uint8_t  seq;
    uint16_t input_mask;
    uint8_t  len;
    uint8_t  data[RF_PACKET_MAX_DATA];
} RfPacket_t;

#pragma pack(pop)

#define RF_PACKET_HEADER_SIZE   ((uint8_t)offsetof(RfPacket_t, data))

uint8_t RFPacket_IsValid(RfPacket_t *pkt, uint8_t size);
uint8_t RFPacket_IsForMe(RfPacket_t *pkt, uint8_t my_id);

uint8_t RFPacket_BuildInput(RfPacket_t *pkt,
                            uint8_t src_id,
                            uint8_t dst_id,
                            uint8_t seq,
                            uint16_t input_mask);

uint8_t RFPacket_BuildRS485(RfPacket_t *pkt,
                            uint8_t src_id,
                            uint8_t dst_id,
                            uint8_t seq,
                            uint8_t *data,
                            uint8_t len);

uint8_t RFPacket_BuildAck(RfPacket_t *pkt,
                          uint8_t src_id,
                          uint8_t dst_id,
                          uint8_t seq);

#endif
