/********************************************************

   ksbuffer.h
    Copyright 2019.10.27 konoar

 ********************************************************/

#ifndef __KS_BUFFER_H__
#define __KS_BUFFER_H__

#define KS_BUFFER_BLOCK_MAX 16 
#define KS_BUFFER_FLAG_END  1

typedef void* ksBuffer;

int ksBufferInit    (ksBuffer *buff);
int ksBufferUninit  (ksBuffer *buff);
int ksBufferWrite   (ksBuffer *buff, const char *data, int *size, short flag);
int ksBufferRead    (ksBuffer *buff, char *data, int *size);

#endif // __KS_BUFFER_H__

