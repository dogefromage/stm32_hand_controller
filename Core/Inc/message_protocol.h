#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MESSAGE_BUFFER_SIZE(n) ((uint32_t)(n) + 6)

typedef enum message_protocol_status_t {
    MESSAGE_PROTOCOL_OK,
    MESSAGE_PROTOCOL_NO_PACKET,
    MESSAGE_PROTOCOL_NOT_ENOUGH_SPACE,
    MESSAGE_PROTOCOL_ERR_EXTRACTION_BUFFER_TOO_SMALL,
    MESSAGE_PROTOCOL_RING_FULL,
} message_protocol_status_t;

// package message into frame for transmission
// must have enough space available for message frame
message_protocol_status_t encapsulate_message(uint8_t* dest, uint32_t dest_size, void* payload,
                                              uint32_t payload_size);

// if message of given size is available and valid, place inside dest
message_protocol_status_t extract_message(void* payload_dest, uint32_t payload_size);

// try to add received bytes to the buffer if enough space
message_protocol_status_t add_received_bytes(uint8_t* buff, uint32_t size);
