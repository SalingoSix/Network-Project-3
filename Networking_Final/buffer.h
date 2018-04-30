#ifndef _BUFFER_HG_
#define _BUFFER_HG_

#include <vector>
#include <string>

class buffer {
public:
	buffer(size_t size);

	void writeInt32BE(size_t index, int value);
	void writeInt32BE(int value);

	int readInt32BE(size_t index);
	int readInt32BE(void);

	void writeShortBE(size_t index, short value);
	void writeShortBE(short value);

	short readShortBE(size_t index);
	short readShortBE(void);

	void writeString(size_t index, std::string value);
	void writeString(std::string value);

	std::string readString(size_t index, int strLength);
	std::string readString(int strLength);
	void displayIndices();
	void resetIndicesManually();

private:
	std::vector<uint8_t> mBuffer;
	int mReadIndex;
	int mWriteIndex;
	void resetIndices();
};


#endif