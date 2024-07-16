#include <cstdlib>
#include "common.hh"

static const char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_url_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char *base64_gen_encode(const unsigned char *src, size_t len,
                               size_t *out_len, const char *table, int add_pad)
{
    char *out, *pos;
    const unsigned char *end, *in;
    size_t olen;
    int line_len;

    if (len >= SIZE_MAX / 4)
        return NULL;
    olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    if (add_pad)
        olen += olen / 72; /* line feeds */
    olen++;                /* nul termination */
    if (olen < len)
        return NULL; /* integer overflow */
    out = (char*)malloc(olen);
    if (out == NULL)
        return NULL;

    end = src + len;
    in = src;
    pos = out;
    line_len = 0;
    while (end - in >= 3)
    {
        *pos++ = table[(in[0] >> 2) & 0x3f];
        *pos++ = table[(((in[0] & 0x03) << 4) | (in[1] >> 4)) & 0x3f];
        *pos++ = table[(((in[1] & 0x0f) << 2) | (in[2] >> 6)) & 0x3f];
        *pos++ = table[in[2] & 0x3f];
        in += 3;
        line_len += 4;
        if (add_pad && line_len >= 72)
        {
            *pos++ = '\n';
            line_len = 0;
        }
    }

    if (end - in)
    {
        *pos++ = table[(in[0] >> 2) & 0x3f];
        if (end - in == 1)
        {
            *pos++ = table[((in[0] & 0x03) << 4) & 0x3f];
            if (add_pad)
                *pos++ = '=';
        }
        else
        {
            *pos++ = table[(((in[0] & 0x03) << 4) |
                            (in[1] >> 4)) &
                           0x3f];
            *pos++ = table[((in[1] & 0x0f) << 2) & 0x3f];
        }
        if (add_pad)
            *pos++ = '=';
        line_len += 4;
    }

    if (add_pad && line_len)
        *pos++ = '\n';

    *pos = '\0';
    if (out_len)
        *out_len = pos - out;
    return out;
}

char *base64_encode(const void *src, size_t len, size_t *out_len)
{
    return base64_gen_encode((const unsigned char *)src, len, out_len, base64_table, 1);
}


bool GetBitInU8Buf(const uint8_t *buf, size_t offset, size_t bitIdx)
{
	uint8_t b = buf[offset + (bitIdx >> 3)];
	uint32_t bitpos = bitIdx & 0b111;
	return b & (1 << bitpos);
}

void WriteI8(int8_t value, uint8_t *buffer, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	*(buffer + offset) = *ptr1;
}

void WriteI16(int16_t value, uint8_t *buffer, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(buffer + offset) = *ptr1;
	*(buffer + offset + 1) = *ptr2;
}

void WriteI32(int32_t value, uint8_t *buffer, uint32_t offset)
{
	buffer[0 + offset] = (value & 0xFF) >> 0;
	buffer[1 + offset] = (value & 0xFFFF) >> 8;
	buffer[2 + offset] = (value & 0xFFFFFF) >> 16;
	buffer[3 + offset] = (value & 0xFFFFFFFF) >> 24;
}

void WriteI64(int64_t value, uint8_t *buffer, uint32_t offset)
{
	buffer[0 + offset] = (value & 0xFF) >> 0;
	buffer[1 + offset] = (value & 0xFFFF) >> 8;
	buffer[2 + offset] = (value & 0xFFFFFF) >> 16;
	buffer[3 + offset] = (value & 0xFFFFFFFF) >> 24;
	buffer[4 + offset] = (value & 0xFFFFFFFFFF) >> 32;
	buffer[5 + offset] = (value & 0xFFFFFFFFFFFF) >> 40;
	buffer[6 + offset] = (value & 0xFFFFFFFFFFFFFF) >> 48;
	buffer[7 + offset] = (value & 0xFFFFFFFFFFFFFFFF) >> 56;
}

int16_t ParseI16(const uint8_t *const buffer, uint32_t offset)
{
	int16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr1 = *(buffer + offset);
	*ptr2 = *(buffer + offset + 1);
	return step;
}

int32_t ParseI32(const uint8_t *const buffer, uint32_t offset)
{
	int32_t value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(buffer + offset + 0);
	*ptr1 = *(buffer + offset + 1);
	*ptr2 = *(buffer + offset + 2);
	*ptr3 = *(buffer + offset + 3);
	return value;
}

void WriteU8(uint8_t value, uint8_t *buffer, uint32_t offset)
{
	*(buffer + offset) = value;
}

void WriteU16(uint16_t value, uint8_t *buffer, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(buffer + offset) = *ptr1;
	*(buffer + offset + 1) = *ptr2;
}

void WriteU32(uint32_t value, uint8_t *buffer, uint32_t offset)
{
	buffer[0 + offset] = (value & 0xFF) >> 0;
	buffer[1 + offset] = (value & 0xFFFF) >> 8;
	buffer[2 + offset] = (value & 0xFFFFFF) >> 16;
	buffer[3 + offset] = (value & 0xFFFFFFFF) >> 24;
}

uint8_t ParseU8(const uint8_t *const buffer, uint32_t offset){
	return buffer[offset];
}

uint16_t ParseU16(const uint8_t *const buffer, uint32_t offset)
{
	uint16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr1 = *(buffer + offset);
	*ptr2 = *(buffer + offset + 1);
	return step;
}

uint32_t ParseU32(const uint8_t *const buffer, uint32_t offset)
{
	uint32_t value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(buffer + offset + 0);
	*ptr1 = *(buffer + offset + 1);
	*ptr2 = *(buffer + offset + 2);
	*ptr3 = *(buffer + offset + 3);
	return value;
}

uint64_t ParseU64(const uint8_t *const buffer, uint32_t offset)
{
	uint64_t step;
	uint8_t *ptr0 = (uint8_t *)&step;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	uint8_t *ptr4 = ptr0 + 4;
	uint8_t *ptr5 = ptr0 + 5;
	uint8_t *ptr6 = ptr0 + 6;
	uint8_t *ptr7 = ptr0 + 7;
	*ptr0 = *(buffer + offset + 0);
	*ptr1 = *(buffer + offset + 1);
	*ptr2 = *(buffer + offset + 2);
	*ptr3 = *(buffer + offset + 3);
	*ptr4 = *(buffer + offset + 4);
	*ptr5 = *(buffer + offset + 5);
	*ptr6 = *(buffer + offset + 6);
	*ptr7 = *(buffer + offset + 7);
	return step;
}

float ParseF32(const uint8_t *const buffer, uint32_t offset)
{
	float value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(buffer + offset + 0);
	*ptr1 = *(buffer + offset + 1);
	*ptr2 = *(buffer + offset + 2);
	*ptr3 = *(buffer + offset + 3);
	return value;
}

void WriteI16_BigEndian(int16_t value, uint8_t *buffer, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(buffer + offset) = *ptr2;
	*(buffer + offset + 1) = *ptr1;
}

int16_t ParseI16_BigEndian(const uint8_t *const buffer, uint32_t offset)
{
	int16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr2 = *(buffer + offset);
	*ptr1 = *(buffer + offset + 1);
	return step;
}

void WriteU16_BigEndian(uint16_t value, uint8_t *buffer, size_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(buffer + offset) = *ptr2;
	*(buffer + offset + 1) = *ptr1;
}

void WriteU32_BigEndian(uint32_t value, uint8_t *buffer, size_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	uint8_t *ptr3 = ptr1 + 2;
	uint8_t *ptr4 = ptr1 + 3;
	*(buffer + offset) = *ptr4;
	*(buffer + offset + 1) = *ptr3;
	*(buffer + offset + 2) = *ptr2;
	*(buffer + offset + 3) = *ptr1;
}

uint16_t ParseU16_BigEndian(const uint8_t *const buffer, size_t offset)
{
	uint16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr2 = *(buffer + offset);
	*ptr1 = *(buffer + offset + 1);
	return step;
}

uint32_t ParseU32_BE(const uint8_t *const buffer, size_t offset)
{
	uint32_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	uint8_t *ptr3 = ptr1 + 2;
	uint8_t *ptr4 = ptr1 + 3;
	*ptr4 = *(buffer + offset);
	*ptr3 = *(buffer + offset + 1);
	*ptr2 = *(buffer + offset + 2);
	*ptr1 = *(buffer + offset + 3);
	return step;
}