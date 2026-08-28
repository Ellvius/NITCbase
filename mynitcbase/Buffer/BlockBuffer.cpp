#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// Helper function to compare the attributes with attribute Tyep
int compareAttrs(Attribute attr1, Attribute attr2, int attrType){

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

BlockBuffer::BlockBuffer(int blockNum) {
  // initialise this.blockNum with the argument
  this->blockNum = blockNum;
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {
  
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS){
    return ret;
  }

  // populate the numEntries, numAttrs and numSlots fields in *head
  memcpy(&head->pblock,     bufferPtr + 4,  4);
  memcpy(&head->lblock,     bufferPtr + 8,  4);
  memcpy(&head->rblock,     bufferPtr + 12, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs,   bufferPtr + 20, 4);
  memcpy(&head->numSlots,   bufferPtr + 24, 4);

  return SUCCESS;
}


int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){

  // check if the block is already in the buffer
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  // load the block from disk into the buffer
  if (bufferNum == E_BLOCKNOTINBUFFER){
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND){
      return E_OUTOFBOUND;
    }

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // return a pointer to the buffer
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}


// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
  struct HeadInfo head;
  
  // get the header using this.getHeader() function
  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if(ret != SUCCESS){
    return ret;
  }

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


/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap){
  unsigned char *bufferPtr;

  // load the block into the buffer
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS){
    return ret;
  }

  // get block header
  struct HeadInfo head;
  this->getHeader(&head);

  // copy slotmap to output buffer
  int slotCount = head.numSlots;
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}