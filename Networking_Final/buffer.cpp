#include "buffer.h"
#include <iostream>

//Constructs a buffer of a specified size
buffer::buffer(size_t size) : mWriteIndex(0),
mReadIndex(0)
{
	mBuffer.resize(size);
}

//Writes an integer value to the specified index within the buffer (as well as the 3 subsequent indices)
//Will expand the buffer if specified index is outside the buffer's scope
//Uses Big Endian notation
void buffer::writeInt32BE(size_t index, int value)
{
	if (index + 3 > mBuffer.capacity() - 1)
		mBuffer.resize(index + 4);

	mBuffer[index + 3] = value % 256;
	mBuffer[index + 2] = (value >> 8) % 256;
	mBuffer[index + 1] = (value >> 16) % 256;
	mBuffer[index] = value >> 24;

	return;
}

//Writes an integer value to the next free space within the buffer (as well as the 3 subsequent indices)
//Will expand the buffer if there is not enough space for 4 more entries
//Uses Big Endian notation
void buffer::writeInt32BE(int value)
{
	unsigned int temp = mWriteIndex;
	if ((temp + 3) > (mBuffer.capacity() - 1))
		mBuffer.resize(mBuffer.capacity() * 2 + 4);

	mBuffer[mWriteIndex + 3] = value % 256;
	mBuffer[mWriteIndex + 2] = (value >> 8) % 256;
	mBuffer[mWriteIndex + 1] = (value >> 16) % 256;
	mBuffer[mWriteIndex] = value >> 24;

	mWriteIndex += 4;

	return;
}

//Works the same as writeInt32BE, but only takes up 2 bytes in buffer
void buffer::writeShortBE(size_t index, short value)
{
	if (index + 1 > mBuffer.capacity() - 1)
		mBuffer.resize(index + 2);

	mBuffer[index + 1] = value % 256;
	mBuffer[index] = (value >> 8) % 256;

	return;
}

//Works same as writeInt32BE
void buffer::writeShortBE(short value)
{
	unsigned int temp = mWriteIndex;
	if ((temp + 1) > (mBuffer.capacity() - 1))
		mBuffer.resize(mBuffer.capacity() * 2 + 4);

	mBuffer[mWriteIndex + 1] = value % 256;
	mBuffer[mWriteIndex] = (value >> 8) % 256;

	mWriteIndex += 2;

	return;
}

//Inserts a string, character by character into the buffer, starting from the specified index value
//Inserts characters in the order they appear within the string
//Expands the buffer size if the string will not fit
void buffer::writeString(size_t index, std::string value)
{
	int strLength = value.length();
	if ((index + strLength) > (mBuffer.capacity() - 1))
		mBuffer.resize(index + strLength);

	for (int i = 0; i < strLength; i++)
		mBuffer[index++] = (unsigned char)value[i];

	return;
}

//Inserts a string, character by character into the buffer, starting from the earliest free space
//Inserts characters in the order they appear within the string
//Expands the buffer size if the string will not fit
void buffer::writeString(std::string value)
{
	int strLength = value.length();
	unsigned int temp = mWriteIndex;
	int bufferSize = mBuffer.capacity() - 1;
	if ((temp + strLength) > bufferSize)
		mBuffer.resize(mBuffer.capacity() * 2 + strLength);

	for (int i = 0; i < strLength; i++)
		mBuffer[mWriteIndex++] = (unsigned char)value[i];

	return;
}

//Reads 4 indices in a row, and turns them into a 32 bit integer, beginning at the specified index
//returns 0 if index is out of the buffer's scope
int buffer::readInt32BE(size_t index)
{
	if (index + 3 > mBuffer.capacity() - 1)
		return 0;

	int readValue = mBuffer[index];
	readValue = readValue * 256 + mBuffer[index + 1];
	readValue = readValue * 256 + mBuffer[index + 2];
	readValue = readValue * 256 + mBuffer[index + 3];
	return readValue;
}

//Reads the next 4 previously unread indices in the buffer
//Returns 0 if it runs out of the buffer's scope (ie. nothing left to read)
int buffer::readInt32BE(void)
{
	unsigned int temp = mReadIndex;
	if ((temp + 3) > (mBuffer.capacity() - 1))
		return 0;

	int readValue = mBuffer[mReadIndex++];
	readValue = readValue * 256 + mBuffer[mReadIndex++];
	readValue = readValue * 256 + mBuffer[mReadIndex++];
	readValue = readValue * 256 + mBuffer[mReadIndex++];
	resetIndices();
	return readValue;
}

//Reads 2 bytes from the buffer starting at the specified index
short buffer::readShortBE(size_t index)
{
	if (index + 3 > mBuffer.capacity() - 1)
		return 0;

	int readValue = mBuffer[index];
	readValue = readValue * 256 + mBuffer[index + 1];
	return readValue;
}

//Reads 2 bytes from the buffer beginning at mReadIndex
short buffer::readShortBE(void)
{
	unsigned int temp = mReadIndex;
	if ((temp + 3) > (mBuffer.capacity() - 1))
		return 0;

	int readValue = mBuffer[mReadIndex++];
	readValue = readValue * 256 + mBuffer[mReadIndex++];
	resetIndices();
	return readValue;
}

//Reads a specified number of bytes from the buffer as a single string, beginning at the specified index
std::string buffer::readString(size_t index, int strLength)
{
	if (index + strLength > mBuffer.capacity())
		return 0;

	std::string readValue = "";

	for (int i = 0; i < strLength; i++)
	{
		char c = (char)mBuffer[index++];
		readValue += c;
	}
	return readValue;
}

//Reads a specified number of bytes from the buffer as a string, starting from mReadIndex
std::string buffer::readString(int strLength)
{
	if (mWriteIndex + strLength > mBuffer.capacity())
		return 0;

	std::string readValue = "";

	for (int i = 0; i < strLength; i++)
	{
		char c = (char)mBuffer[mReadIndex++];
		readValue += c;
	}
	resetIndices();
	return readValue;
}

void buffer::resetIndices()
{
	if (mWriteIndex == mReadIndex)
		mWriteIndex = mReadIndex = 0;
}

void buffer::resetIndicesManually()
{
	mWriteIndex = mReadIndex = 0;
}

void buffer::displayIndices()
{
	std::cout << "Write Index : " << mWriteIndex << std::endl;
	std::cout << "Read Index : " << mReadIndex << std::endl;
}