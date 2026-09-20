

bool USB_ReadByte(uint8_t *byte)
{
    if (rx_tail == rx_head)
    {
        return false;  // nothing available
    }

    *byte = rx_buffer[rx_tail];

    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;

    return true;
}
