#include "common.hh"

bool GetBitInU8Buf(const uint8_t *buf, size_t offset, size_t bitIdx)
{
	uint8_t b = buf[offset + (bitIdx >> 3)];
	uint32_t bitpos = bitIdx & 0b111;
	return b & (1 << bitpos);
}

int16_t ParseInt16(const uint8_t *const message, uint32_t offset)
{
	int16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr1 = *(message + offset);
	*ptr2 = *(message + offset + 1);
	return step;
}

void WriteInt16(int16_t value, uint8_t *message, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(message + offset) = *ptr1;
	*(message + offset + 1) = *ptr2;
}

uint16_t ParseUInt16(const uint8_t *const message, uint32_t offset)
{
	uint16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr1 = *(message + offset);
	*ptr2 = *(message + offset + 1);
	return step;
}

uint16_t ParseU16_BE(const uint8_t *const message, size_t offset)
{
	uint16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr2 = *(message + offset);
	*ptr1 = *(message + offset + 1);
	return step;
}

void WriteU16_BE(uint16_t value, uint8_t *message, size_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(message + offset) = *ptr2;
	*(message + offset + 1) = *ptr1;
}

uint32_t ParseU32_BE(const uint8_t *const message, size_t offset)
{
	uint32_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	uint8_t *ptr3 = ptr1 + 2;
	uint8_t *ptr4 = ptr1 + 3;
	*ptr4 = *(message + offset);
	*ptr3 = *(message + offset + 1);
	*ptr2 = *(message + offset + 2);
	*ptr1 = *(message + offset + 3);
	return step;
}

void WriteU32_BE(uint32_t value, uint8_t *message, size_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	uint8_t *ptr3 = ptr1 + 2;
	uint8_t *ptr4 = ptr1 + 3;
	*(message + offset) = *ptr4;
	*(message + offset + 1) = *ptr3;
	*(message + offset + 2) = *ptr2;
	*(message + offset + 3) = *ptr1;
}

uint64_t ParseUInt64(const uint8_t *const message, uint32_t offset)
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
	*ptr0 = *(message + offset + 0);
	*ptr1 = *(message + offset + 1);
	*ptr2 = *(message + offset + 2);
	*ptr3 = *(message + offset + 3);
	*ptr4 = *(message + offset + 4);
	*ptr5 = *(message + offset + 5);
	*ptr6 = *(message + offset + 6);
	*ptr7 = *(message + offset + 7);
	return step;
}

void WriteUInt16(uint16_t value, uint8_t *message, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(message + offset) = *ptr1;
	*(message + offset + 1) = *ptr2;
}

void WriteInt8(int8_t value, uint8_t *message, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	*(message + offset) = *ptr1;
}

uint32_t ParseUInt32(const uint8_t *const message, uint32_t offset)
{
	uint32_t value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(message + offset + 0);
	*ptr1 = *(message + offset + 1);
	*ptr2 = *(message + offset + 2);
	*ptr3 = *(message + offset + 3);
	return value;
}

int32_t ParseInt32(const uint8_t *const message, uint32_t offset)
{
	int32_t value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(message + offset + 0);
	*ptr1 = *(message + offset + 1);
	*ptr2 = *(message + offset + 2);
	*ptr3 = *(message + offset + 3);
	return value;
}

float ParseFloat32(const uint8_t *const message, uint32_t offset)
{
	float value;
	uint8_t *ptr0 = (uint8_t *)&value;
	uint8_t *ptr1 = ptr0 + 1;
	uint8_t *ptr2 = ptr0 + 2;
	uint8_t *ptr3 = ptr0 + 3;
	*ptr0 = *(message + offset + 0);
	*ptr1 = *(message + offset + 1);
	*ptr2 = *(message + offset + 2);
	*ptr3 = *(message + offset + 3);
	return value;
}

void WriteUInt32(uint32_t value, uint8_t *message, uint32_t offset)
{
	message[0 + offset] = (value & 0xFF) >> 0;
	message[1 + offset] = (value & 0xFFFF) >> 8;
	message[2 + offset] = (value & 0xFFFFFF) >> 16;
	message[3 + offset] = (value & 0xFFFFFFFF) >> 24;
}

void WriteInt64(int64_t value, uint8_t *message, uint32_t offset)
{
	message[0 + offset] = (value & 0xFF) >> 0;
	message[1 + offset] = (value & 0xFFFF) >> 8;
	message[2 + offset] = (value & 0xFFFFFF) >> 16;
	message[3 + offset] = (value & 0xFFFFFFFF) >> 24;
	message[4 + offset] = (value & 0xFFFFFFFFFF) >> 32;
	message[5 + offset] = (value & 0xFFFFFFFFFFFF) >> 40;
	message[6 + offset] = (value & 0xFFFFFFFFFFFFFF) >> 48;
	message[7 + offset] = (value & 0xFFFFFFFFFFFFFFFF) >> 56;
}



void WriteInt16BigEndian(int16_t value, uint8_t *message, uint32_t offset)
{
	uint8_t *ptr1 = (uint8_t *)&value;
	uint8_t *ptr2 = ptr1 + 1;
	*(message + offset) = *ptr2;
	*(message + offset + 1) = *ptr1;
}



int16_t ParseInt16BigEndian(const uint8_t *const message, uint32_t offset)
{
	int16_t step;
	uint8_t *ptr1 = (uint8_t *)&step;
	uint8_t *ptr2 = ptr1 + 1;
	*ptr2 = *(message + offset);
	*ptr1 = *(message + offset + 1);
	return step;
}