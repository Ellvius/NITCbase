#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer(){
  // initialize all blocks as free
  for(int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++){
    metainfo[bufferIndex].free = true;
  }
}

// We are not writing back from the buffer in this stage, so we will define an empty destructor here
StaticBuffer::~StaticBuffer(){}


int StaticBuffer::getFreeBuffer(int blockNum){
  // check blockNum bounds
  if(blockNum < 0 || blockNum > DISK_BLOCKS){
    return E_OUTOFBOUND;
  }

  int allocatedBuffer;

  // Iterate through the metadata of the buffer to find a free buffer
  for(int buffIndex = 0; buffIndex < BUFFER_CAPACITY; buffIndex++){
    if(metainfo[buffIndex].free){
      allocatedBuffer = buffIndex;
      break;
    }
  }

  metainfo[allocatedBuffer].free = false;
  metainfo[allocatedBuffer].blockNum = blockNum;

  return allocatedBuffer;
}

/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum){
  if(blockNum < 0 || blockNum > DISK_BLOCKS){
    return E_OUTOFBOUND;
  }

  for(int buffIndex = 0; buffIndex < BUFFER_CAPACITY; buffIndex++){
    if(metainfo[buffIndex].blockNum == blockNum){
      return buffIndex;
    }
  }

  return E_BLOCKNOTINBUFFER;
}

