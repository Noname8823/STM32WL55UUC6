/*
 * rf_packet.c
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */


#include "rf_packet.h"

uint8_t RFPacket_IsValid(RfPacket_t *pkt, uint8_t size)
{
    if (pkt == 0)
    {
        return 0;
    }

    if (size < 7)
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

    return 1;
}

uint8_t RFPacket_IsForMe(RfPacket_t *pkt, uint8_t my_id)
{
    if (pkt->dst_id == my_id)
    {
        return 1;
    }

    if (pkt->dst_id == RF_BROADCAST_ID)
    {
        return 1;
    }

    return 0;
}
