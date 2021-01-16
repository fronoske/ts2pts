#include "crc.h"

static unsigned int crc_table[256];

void init_crc(void)
{
    int i, j;
    unsigned int c;
    unsigned int *pct = crc_table;
    unsigned int poly = 0x04C11DB7UL;

    for(i=0; i<256; i++){
        c=i<<24;
        for(j=0; j<8; j++)
            c = (c<<1) ^ (poly & (((int)c)>>31) );

        pct[i] = ((c>>24)&0x000000ff) |
                 ((c>> 8)&0x0000ff00) |
                 ((c<< 8)&0x00ff0000) |
                 ((c<<24)&0xff000000);
    }
    return;
}

unsigned int get_crc(const unsigned char *buffer, const size_t length)
{
    unsigned int crc = ~0;
    const unsigned int  *pct = crc_table;
    const unsigned char *end = buffer+length;

    while(buffer<end){
        crc = pct[((unsigned char)crc) ^ *buffer] ^ (crc>>8);
        buffer++;
    }
    return crc;
}
