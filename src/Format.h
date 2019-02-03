#pragma once

#include "common/String.h"
#include <vector>

class VideoBuffer;

namespace format
{
	const static char hex[] = "0123456789ABCDEF";

	ByteString URLEncode(ByteString value);
	ByteString UnixtimeToDate(time_t unixtime, ByteString dateFomat = ByteString("%d %b %Y"));
	ByteString UnixtimeToDateMini(time_t unixtime);
	String CleanString(String dirtyString, bool ascii, bool color, bool newlines, bool numeric = false);
	std::vector<char> VideoBufferToBMP(const VideoBuffer & vidBuf);
	std::vector<char> VideoBufferToPPM(const VideoBuffer & vidBuf);
	unsigned long CalculateCRC(unsigned char * data, int length);
}
