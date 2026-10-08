#include "StaticBuffer.h"
#include <cstring>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];


StaticBuffer::StaticBuffer(){
  // copy Block allocation map to buffer
  for(int i = 0; i < 4; i++){
    unsigned char buffer[BLOCK_SIZE];

    Disk::readBlock(buffer, i);
    memcpy(blockAllocMap + i*BLOCK_SIZE, buffer, BLOCK_SIZE);
  }

  for(int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
    metainfo[bufIndex].free = true;
    metainfo[bufIndex].dirty = false;
    metainfo[bufIndex].timeStamp = -1;
    metainfo[bufIndex].blockNum = -1;
  }
}



StaticBuffer::~StaticBuffer(){
  // write back block allocation map to disk
  for(int i = 0; i < 4; i++){
    unsigned char buffer[BLOCK_SIZE];

    memcpy(buffer, blockAllocMap + i*BLOCK_SIZE, BLOCK_SIZE);
    Disk::writeBlock(buffer, i);
  }
  
  // write back the modified buffers
  for(int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
    if(!metainfo[bufIndex].free && metainfo[bufIndex].dirty)
      Disk::writeBlock(blocks[bufIndex], metainfo[bufIndex].blockNum);
  }
}



int StaticBuffer::setDirtyBit(int blockNum){
  int bufferNum = getBufferNum(blockNum);

  if(bufferNum == E_BLOCKNOTINBUFFER)
    return E_BLOCKNOTINBUFFER;

  if(bufferNum == E_OUTOFBOUND)
    return E_OUTOFBOUND;

  // set dirty bit 
  metainfo[bufferNum].dirty = true;

  return SUCCESS;
}



int StaticBuffer::getBufferNum(int blockNum){
  if (blockNum < 0 || blockNum > DISK_BLOCKS)
    return E_OUTOFBOUND;

  for (int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
    if (metainfo[bufIndex].blockNum == blockNum)
      return bufIndex;
  }

  return E_BLOCKNOTINBUFFER;
}



int StaticBuffer::getFreeBuffer(int blockNum){
  if(blockNum < 0 || blockNum > DISK_BLOCKS)
    return E_OUTOFBOUND;

  // increase the timestamp of all occupied buffers
  for(int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++)
    if(!metainfo[bufIndex].free)  
      metainfo[bufIndex].timeStamp++;

  int bufferNum = -1; // store buffernum of the freed buffer

  // Iterate through the metadata of the buffer to find a free buffer
  for(int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
    if(metainfo[bufIndex].free){
      bufferNum = bufIndex;
      break;
    }
  }

  // free buffer not available
  if(bufferNum == -1){
    int maxIndex = -1, maxTimestamp = -1;
    
    // find buffer with largest timestamp
    for(int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
      if(metainfo[bufIndex].timeStamp > maxTimestamp){
        maxTimestamp = metainfo[bufIndex].timeStamp;
        maxIndex = bufIndex;
      }
    }

    // write back into disk if modified
    if(metainfo[maxIndex].dirty)
      Disk::writeBlock(blocks[maxIndex], metainfo[maxIndex].blockNum);

    bufferNum = maxIndex;
  }

  // update metainfo
  metainfo[bufferNum].free = false;
  metainfo[bufferNum].blockNum = blockNum;
  metainfo[bufferNum].dirty = false;
  metainfo[bufferNum].timeStamp = 0;

  return bufferNum;
}

