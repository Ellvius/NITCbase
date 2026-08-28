#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op){
  // get previous search index from relation cache
  RecId prevRecId;
  int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);

  if(ret != SUCCESS){
    return RecId{-1, -1};
  }

  int block, slot;    // current iterating block & slot number

  // set the starting block & slot depending on previous hit
  if (prevRecId.block == -1 && prevRecId.slot == -1){
    // no hits from previous search
    // set search to start from first block
    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if(ret != SUCCESS){
      return RecId{-1, -1};
    }
    
    block = relCatEntry.firstBlk;
    slot = 0;
  }
  else{
    // hit from previous search
    // search start from the next record
    block = prevRecId.block;
    slot = prevRecId.slot + 1;
  }

  // search the next record in the relation which satisfies the given condition
  while (block != -1){
    
    // create buffer and get header & slotmap for the block
    RecBuffer curBlock(block);

    HeadInfo head;
    curBlock.getHeader(&head);

    unsigned char slotMap[head.numSlots];
    curBlock.getSlotMap(slotMap);

    // continue to next block if no more slots in current block
    if(slot >= head.numSlots){
      block = head.rblock;
      slot = 0;
      continue; 
    }

    // skip unoccupied slots
    if(slotMap[slot] == SLOT_UNOCCUPIED){
      slot++;
      continue;
    }

    // get the record 
    Attribute record[head.numAttrs];
    curBlock.getRecord(record, slot);

    // get attribute cache entry for record offset
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    // find the difference between the attributes
    int cmpVal; 
    cmpVal = compareAttrs(record[attrCatEntry.offset], attrVal, attrCatEntry.attrType);

    // check whether the given condition satisfies
    if (
        (op == NE && cmpVal != 0) || // if op is "not equal to"
        (op == LT && cmpVal < 0) ||  // if op is "less than"
        (op == LE && cmpVal <= 0) || // if op is "less than or equal to"
        (op == EQ && cmpVal == 0) || // if op is "equal to"
        (op == GT && cmpVal > 0) ||  // if op is "greater than"
        (op == GE && cmpVal >= 0)    // if op is "greater than or equal to"
    ){
      // search hit - update search index in relation cache
      RecId recId{block, slot};
      RelCacheTable::setSearchIndex(relId, &recId);

      return RecId{block, slot};
    }

    slot++;
  }

  // no record found satisfying the condition
  return RecId{-1, -1};
}