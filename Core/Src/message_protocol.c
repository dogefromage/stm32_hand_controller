#include "message_protocol.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef RING_BUFFER_SIZE
#define RING_BUFFER_SIZE 512
#endif

#ifndef EXTRACTION_BUFFER_SIZE
#define EXTRACTION_BUFFER_SIZE 128
#endif

const uint8_t magic[4] = {0xca, 0xfe, 0xba, 0xbe};

uint8_t extraction_buffer[EXTRACTION_BUFFER_SIZE];

uint8_t ring_buffer[RING_BUFFER_SIZE];
volatile int32_t read_head = 0, write_head = 0;

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}

message_protocol_status_t encapsulate_message(uint8_t* dest, uint32_t dest_size, void* payload,
                                              uint32_t payload_size) {
    // [ MAGIC(4) | payload(payload_size) | CRC(2) ]

    uint32_t message_size = MESSAGE_BUFFER_SIZE(payload_size);

    if (dest_size < message_size) {
        return MESSAGE_PROTOCOL_NOT_ENOUGH_SPACE;  // error, not enough space
    }

    uint8_t* next = dest;

    memcpy(next, magic, 4);
    next += 4;

    memcpy(next, payload, payload_size);
    next += payload_size;

    uint16_t crc = crc16_ccitt(dest, 4 + payload_size);
    memcpy(next, &crc, 2);

    return MESSAGE_PROTOCOL_OK;
}

// returns number of bytes read, either 0 or size
uint32_t peek_ring(uint8_t* dest, uint32_t size) {
    int32_t available_bytes = write_head - read_head;
    if (available_bytes < 0) {
        available_bytes += RING_BUFFER_SIZE;
    }

    if (available_bytes < size) {
        return 0;  // not enough
    }

    for (int i = 0; i < size; i++) {
        dest[i] = ring_buffer[(read_head + i) % RING_BUFFER_SIZE];
    }

    return size;
}

// returns number of bytes read, either 0 or size
uint32_t read_ring(uint8_t* dest, uint32_t size) {
    uint32_t read = peek_ring(dest, size);
    if (read > 0) {
        read_head = (read_head + read) % RING_BUFFER_SIZE;
    }
    return read;
}

uint32_t attempt_extract_at_head(void* payload_dest, uint32_t payload_size) {
    uint8_t header[4];
    peek_ring(header, 4);

    if (memcmp(header, magic, 4) != 0) {
        // nothing here, continue
        return 0;
    }

    // try to get message
    uint32_t message_size = MESSAGE_BUFFER_SIZE(payload_size);

    if (peek_ring(extraction_buffer, message_size) == 0) {
        return 0;  // ring does not contain a whole packet
    }

    // check crc
    uint16_t received_crc = *(uint16_t*)(&extraction_buffer[4 + payload_size]);
    uint16_t generated_crc = crc16_ccitt(extraction_buffer, 4 + payload_size);

    if (received_crc != generated_crc) {
        return 0;  // message is not valid
    }

    // message is valid!

    memcpy(payload_dest, &extraction_buffer[4], payload_size);

    return message_size;
}

message_protocol_status_t extract_message(void* payload_dest, uint32_t payload_size) {
    uint32_t message_size = MESSAGE_BUFFER_SIZE(payload_size);
    if (message_size >= EXTRACTION_BUFFER_SIZE) {
        return MESSAGE_PROTOCOL_ERR_EXTRACTION_BUFFER_TOO_SMALL;  // must increase extraction buffer
                                                                  // size
    }

    while (read_head != write_head) {
        uint32_t bytes_used_on_success = attempt_extract_at_head(payload_dest, payload_size);
        if (bytes_used_on_success > 0) {
            // read message successfully!
            read_head = (read_head + bytes_used_on_success) % RING_BUFFER_SIZE;
            return MESSAGE_PROTOCOL_OK;
        }
        // check next byte
        read_head = (read_head + 1) % RING_BUFFER_SIZE;
    }

    return MESSAGE_PROTOCOL_NO_PACKET;
}

message_protocol_status_t add_received_bytes(uint8_t* buff, uint32_t size) {
    for (int i = 0; i < size; i++) {
        if ((write_head + 1) % RING_BUFFER_SIZE == read_head) {
            return MESSAGE_PROTOCOL_RING_FULL;  // full
        }

        ring_buffer[write_head] = buff[i];
        write_head = (write_head + 1) % RING_BUFFER_SIZE;
    }

    return MESSAGE_PROTOCOL_OK;
}
