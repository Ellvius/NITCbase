#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// Helper function to compare the attributes with attribute Tyep
int compareAttrs(Attribute attr1, Attribute attr2, int attrType){
  StaticBuffer::compNums++;
  if(attrType == NUMBER){
    if (attr1.nVal < attr2.nVal)
      return -1;
    if (attr1.nVal > attr2.nVal)
      return 1;
    return 0;
  }

  else {
    return strcmp(attr1.sVal, attr2.sVal);
  }
}



BlockBuffer::BlockBuffer(char blockType){
  int blockTypeNum;

  switch(blockType){
    case 'R': blockTypeNum = REC;
              break;
    case 'I': blockTypeNum = IND_INTERNAL;
              break;
    case 'L': blockTypeNum = IND_LEAF;
              break;
    default:  blockTypeNum = UNUSED_BLK;
              break;
  }

  int ret = getFreeBlock(blockTypeNum);

  if(ret == E_DISKFULL)
    this->blockNum = E_DISKFULL;
}



BlockBuffer::BlockBuffer(int blockNum) {
  // initialise this.blockNum with the argument
  this->blockNum = blockNum;
}



int BlockBuffer::getBlockNum(){
  return this->blockNum;
}



int BlockBuffer::getHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS){
    return ret;
  }

  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  head->pblock = bufferHeader->pblock;
  head->lblock = bufferHeader->lblock;
  head->rblock = bufferHeader->rblock;
  head->numEntries = bufferHeader->numEntries;
  head->numAttrs = bufferHeader->numAttrs;
  head->numSlots = bufferHeader->numSlots;

  return SUCCESS;
}



int BlockBuffer::setHeader(struct HeadInfo *head){
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS)
    return ret;

  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numSlots = head->numSlots;

  return StaticBuffer::setDirtyBit(this->blockNum);
}



void BlockBuffer::releaseBlock(){
  // check if block is already released
  int blockNum = this->blockNum;
  if(blockNum < 0 || blockNum >= DISK_BLOCKS || blockNum == INVALID_BLOCKNUM)
    return;

  int bufferNum = StaticBuffer::getBufferNum(blockNum);

  // if block is present in the buffer, free it
  if(bufferNum != E_BLOCKNOTINBUFFER)
    StaticBuffer::metainfo[bufferNum].free = true;
    
  // update block allocation map
  StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;
  this->blockNum = INVALID_BLOCKNUM;
}



int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if(bufferNum == E_BLOCKNOTINBUFFER){
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
  
    if (bufferNum == E_OUTOFBOUND)
      return E_OUTOFBOUND;
  
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }
  else {
    for (int bufIndex = 0; bufIndex < BUFFER_CAPACITY; bufIndex++){
      if (!StaticBuffer::metainfo[bufIndex].free)
        StaticBuffer::metainfo[bufIndex].timeStamp++;
    }

    StaticBuffer::metainfo[bufferNum].timeStamp = 0;
  }

  // return a pointer to the buffer
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}



int BlockBuffer::getFreeBlock(int blockType){
  int freeBlock = -1;

  for(int i = 0; i < DISK_BLOCKS; i++){
    if(StaticBuffer::blockAllocMap[i] == UNUSED_BLK){
      freeBlock = i;
      break;
    }
  }

  if(freeBlock == -1)
    return E_DISKFULL;  

  this->blockNum = freeBlock;

  int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

  // initialize header
  struct HeadInfo head;

  head.pblock = -1;
  head.lblock = -1;
  head.rblock = -1;
  head.numEntries = 0;
  head.numAttrs = 0;
  head.numSlots = 0;

  this->setHeader(&head);
  this->setBlockType(blockType);

  return freeBlock;
}



int BlockBuffer::setBlockType(int blockType){
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS)
    return ret;

  // store the input block type in the first 4 bytes of the buffer.
  *((int32_t *)bufferPtr) = blockType;

  // update block allocation map
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  return StaticBuffer::setDirtyBit(this->blockNum);
}




// call parent non-default constructor with 'R' denoting record block.
RecBuffer::RecBuffer() : BlockBuffer('R') {}


// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}



int RecBuffer::getSlotMap(unsigned char *slotMap){
  unsigned char *bufferPtr;

  // load the block into the buffer
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
    return ret;

  // get block header
  struct HeadInfo head;
  this->getHeader(&head);

  // copy slotmap to output buffer
  int slotCount = head.numSlots;
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}



int RecBuffer::setSlotMap(unsigned char *slotMap){
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS)
    return ret;

  struct HeadInfo header;
  BlockBuffer::getHeader(&header);

  int numSlots = header.numSlots;

  // the slotmap starts at bufferPtr + HEADER_SIZE
  memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

  // set dirty bit
  return StaticBuffer::setDirtyBit(this->blockNum);
}



int RecBuffer::getRecord(union Attribute *rec, int slotNum){
  struct HeadInfo head;

  // get the header using this.getHeader() function
  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
    return ret;

  /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
  */
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

  // load the record into the rec data structure
  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}



int RecBuffer::setRecord(union Attribute *rec, int slotNum){
  unsigned char *bufferPtr;

  // get starting address of buffer containing the block
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
    return ret;

  struct HeadInfo head;
  this->getHeader(&head);

  int numAttrs = head.numAttrs;
  int numSlots = head.numSlots;

  // check the slotNum range
  if (slotNum < 0 || slotNum >= numSlots)
    return E_OUTOFBOUND;

  // offset the ptr to the beginning of the required record
  int recordSize = numAttrs * ATTR_SIZE;
  unsigned char *recordPtr = bufferPtr + HEADER_SIZE + numSlots + (recordSize * slotNum);

  // copy the record to buffer
  memcpy(recordPtr, rec, recordSize);
  StaticBuffer::setDirtyBit(this->blockNum);

  return SUCCESS;
}




IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType) {}

IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum) {}




IndInternal::IndInternal() : IndBuffer('I') {}

IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum) {}



int IndInternal::getEntry(void *ptr, int indexNum){
  // check indexNum range
  if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
    return E_OUTOFBOUND;

  // load block into buffer and get bufferPtr
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret != SUCCESS)
    return ret;

  // typecast the void pointer to an internal entry pointer
  struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

  // copy the entries from the indexNum`th entry to *internalEntry
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

  memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
  memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
  memcpy(&(internalEntry->rChild), entryPtr + 20, sizeof(int32_t));

  return SUCCESS;
}



int IndInternal::setEntry(void *ptr, int indexNum){
  return 0;
}



IndLeaf::IndLeaf() : IndBuffer('L') {}

IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum) {}



int IndLeaf::getEntry(void *ptr, int indexNum){
  // check indexNum range
  if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
    return E_OUTOFBOUND;

  // load block into buffer and get bufferPtr
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret != SUCCESS)
    return ret;

  // copy the indexNum'th Index entry in buffer to memory ptr
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
  memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

  return SUCCESS;
}



int IndLeaf::setEntry(void *ptr, int indexNum){
  return 0;
}