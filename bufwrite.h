#ifndef BUF_WRITE_H
#define BUF_WRITE_H

#include <memory.h>

#define PUT_STR(p,v,s) \
    memcpy(p,v,s)

#define PUT_BYTE(p,v) \
    *(p) = (unsigned char)((v)&0xff)

#define PUT_WORD(p,v) \
    do{\
        *((p)  ) = (unsigned char)(((v)>>8)&0xff);\
        *((p)+1) = (unsigned char)( (v)    &0xff);\
    }while(0)

#define PUT_DWORD(p,v) \
    do{\
        *((p)  ) = (unsigned char)(((v)>>24)&0xff);\
        *((p)+1) = (unsigned char)(((v)>>16)&0xff);\
        *((p)+2) = (unsigned char)(((v)>> 8)&0xff);\
        *((p)+3) = (unsigned char)( (v)     &0xff);\
    }while(0)

#endif // BUF_WRITE_H
