#include "rf_packet.h"
#include <string.h>

static uint8_t RFPacket_IsValidType(uint8_t type)
{
    switch (type)
    {
        case RF_PKT_INPUT:
        case RF_PKT_RS485:
        case RF_PKT_ACK:
            return 1;

        default:
            return 0;
    }
}

uint8_t RFPacket_IsValid(RfPacket_t *pkt, uint8_t size)
{
    uint16_t expected_size;

    if (pkt == 0)
    {
        return 0;
    }

    if (size < RF_PACKET_HEADER_SIZE)
    {
        return 0;
    }

    if (pkt->magic != RF_PACKET_MAGIC)
    {
        return 0;
    }

    if (pkt->len > RF_PACKET_MAX_DATA)
    {
        return 0;
    }

    expected_size = (uint16_t)RF_PACKET_HEADER_SIZE + pkt->len;

    if ((uint16_t)size != expected_size)
    {
        return 0;
    }

    if (!RFPacket_IsValidType(pkt->type))
    {
        return 0;
    }

    return 1;
}

uint8_t RFPacket_IsForMe(RfPacket_t *pkt, uint8_t my_id)
{
    if (pkt == 0)
    {
        return 0;
    }

    if ((pkt->dst_id == my_id) || (pkt->dst_id == RF_BROADCAST_ID))
    {
        return 1;
    }

    return 0;
}

uint8_t RFPacket_BuildInput(RfPacket_t *pkt,
                            uint8_t src_id,
                            uint8_t dst_id,
                            uint8_t seq,
                            uint16_t input_mask)
{
    if (pkt == 0)
    {
        return 0;
    }

    memset(pkt, 0, sizeof(RfPacket_t));

    pkt->magic = RF_PACKET_MAGIC;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->type = RF_PKT_INPUT;
    pkt->seq = seq;
    pkt->input_mask = input_mask;
    pkt->len = 0;

    return RF_PACKET_HEADER_SIZE;
}

uint8_t RFPacket_BuildRS485(RfPacket_t *pkt,
                            uint8_t src_id,
                            uint8_t dst_id,
                            uint8_t seq,
                            uint8_t *data,
                            uint8_t len)
{
    if (pkt == 0)
    {
        return 0;
    }

    if (len > RF_PACKET_MAX_DATA)
    {
        return 0;
    }

    if ((len > 0) && (data == 0))
    {
        return 0;
    }

    memset(pkt, 0, sizeof(RfPacket_t));

    pkt->magic = RF_PACKET_MAGIC;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->type = RF_PKT_RS485;
    pkt->seq = seq;
    pkt->input_mask = 0;
    pkt->len = len;

    if (len > 0)
    {
        memcpy(pkt->data, data, len);
    }

    return (uint8_t)(RF_PACKET_HEADER_SIZE + len);
}

uint8_t RFPacket_BuildAck(RfPacket_t *pkt,
                          uint8_t src_id,
                          uint8_t dst_id,
                          uint8_t seq)
{
    if (pkt == 0)
    {
        return 0;
    }

    memset(pkt, 0, sizeof(RfPacket_t));

    pkt->magic = RF_PACKET_MAGIC;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->type = RF_PKT_ACK;
    pkt->seq = seq;
    pkt->input_mask = 0;
    pkt->len = 0;

    return RF_PACKET_HEADER_SIZE;
}
