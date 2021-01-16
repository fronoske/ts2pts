#ifndef CRC_H
#define CRC_H

#if !defined(WIN32)
#include <unistd.h>
#endif

void init_crc(void);
unsigned int get_crc(const unsigned char *buffer, const size_t length);

#endif // CRC_H
