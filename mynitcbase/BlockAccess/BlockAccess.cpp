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



int BlockAccess::insert(int relId, Attribute *record){
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  int blockNum = relCatEntry.firstBlk;
  int numOfSlots = relCatEntry.numSlotsPerBlk;
  int numOfAttributes = relCatEntry.numAttrs;
  int prevBlockNum = -1;
  
  // newRecId stores the rec id where the record is to be inserted
  RecId newRecId = {-1, -1};

  // traverse the linked list of blocks to find a free slot
  while (blockNum != -1){
    RecBuffer currentBlock(blockNum);

    struct HeadInfo header;
    currentBlock.getHeader(&header);

    unsigned char slotMap[numOfSlots];
    currentBlock.getSlotMap(slotMap);

    // search for free slot in the slotmap of the current block
    for(int slot = 0; slot < numOfSlots; slot++){
      if(slotMap[slot] == SLOT_UNOCCUPIED){
        newRecId.block = blockNum;
        newRecId.slot = slot;
        break;
      }
    }

    // free slot found in the current blocks
    if(newRecId.block != -1 || newRecId.slot != -1)
      break;

    // search in the next block in the linked list of blocks
    prevBlockNum = blockNum;
    blockNum = header.rblock;
  }

  // if no free slot is found in existing record blocks
  // have to allocate new record blocks
  if(newRecId.block == -1 || newRecId.slot == -1){
    // cannot allocate more blocks for relation catalog
    if(relId == RELCAT_RELID)
      E_MAXRELATIONS;

    // get new record block using constructor1
    RecBuffer newRecordBlock;
    int newBlockNum = newRecordBlock.getBlockNum();

    if (newBlockNum == E_DISKFULL)
      return E_DISKFULL;

    newRecId.block = newBlockNum;
    newRecId.slot = 0;

    // set the header for the new record block
    struct HeadInfo newHeader;

    newHeader.blockType = REC;
    newHeader.pblock = -1;
    newHeader.lblock = prevBlockNum;
    newHeader.rblock = -1;
    newHeader.numEntries = 0;
    newHeader.numAttrs = numOfAttributes;
    newHeader.numSlots = numOfSlots;

    newRecordBlock.setHeader(&newHeader);

    // set slot Map
    unsigned char slotMap[numOfSlots];
    for(int i = 0; i < numOfSlots; i++){
      slotMap[i] = SLOT_UNOCCUPIED;
    }

    newRecordBlock.setSlotMap(slotMap);
    
    // update the linked list of blocks with new record block
    if(prevBlockNum != -1){
      RecBuffer prevBlock(prevBlockNum);

      HeadInfo prevBlockHeader;
      prevBlock.getHeader(&prevBlockHeader);

      prevBlockHeader.rblock = newRecId.block;
      prevBlock.setHeader(&prevBlockHeader);
    }
    else {
      // update for the very first block of the relation
      relCatEntry.firstBlk = newRecId.block;
    }

    // update the last block of the linked list
    relCatEntry.lastBlk = newRecId.block;
  }

  // insert into the slot specified by newRecId
  RecBuffer insertBlock(newRecId.block);
  insertBlock.setRecord(record, newRecId.slot);

  // update slotmap and header
  unsigned char slotMap[numOfSlots];
  insertBlock.getSlotMap(slotMap);

  slotMap[newRecId.slot] = SLOT_OCCUPIED;
  insertBlock.setSlotMap(slotMap);

  HeadInfo header;
  insertBlock.getHeader(&header);

  header.numEntries++;
  insertBlock.setHeader(&header);

  // increment the cache entry
  relCatEntry.numRecs++;

  return RelCacheTable::setRelCatEntry(relId, &relCatEntry);
}



int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
  // search the newName in relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute newRelationName; // set newRelationName with newName
  strcpy(newRelationName.sVal, newName);

  RecId newrelRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, newRelationName, EQ);
  
  if(newrelRecId.block != -1 && newrelRecId.slot != -1)
    return E_RELEXIST;


  // search the oldName in relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute oldRelationName; // set oldRelationName with oldName
  strcpy(oldRelationName.sVal, oldName);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, oldRelationName, EQ);

  if(relcatRecId.block == -1 && relcatRecId.slot == -1)
    return E_RELNOTEXIST;


  // update the relation catalog record
  RecBuffer recBuffer(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  recBuffer.getRecord(relCatRecord, relcatRecId.slot);

  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
  recBuffer.setRecord(relCatRecord, relcatRecId.slot);

  
  // update the attribute catalog records
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  while(true){
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

    if(attrcatRecId.block == -1 && attrcatRecId.slot == -1)
      break;

    RecBuffer attrcatBuffer(attrcatRecId.block);
    attrcatBuffer.getRecord(attrCatRecord, attrcatRecId.slot);

    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
    attrcatBuffer.setRecord(attrCatRecord, attrcatRecId.slot);
  }

  return SUCCESS;
}



int BlockAccess::renameAttribute(char relName[ATTR_SIZE], 
  char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){

  // search the relation in relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr; // set relNameAttr to relName
  strcpy(relNameAttr.sVal, relName);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

  if(relcatRecId.block == -1 && relcatRecId.slot == -1)
    return E_RELNOTEXIST;


  // search attribute catalog for oldName & newName
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  RecId attrToRenameRecId{-1, -1}; // to store the record id of the attribute to be renamed

  while (true){
    RecId searchRes = BlockAccess::linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

    if(searchRes.block == -1 && searchRes.slot == -1) 
      break;
    
    RecBuffer attrcatBuffer(searchRes.block);
    attrcatBuffer.getRecord(attrCatRecord, searchRes.slot);

    // save the record id of the oldName attribute
    if(strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0)
      attrToRenameRecId = searchRes;
    
    // check whether an attribute with newName exists for the same relation
    if(strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
      return E_ATTREXIST;
  }

  if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
    return E_ATTRNOTEXIST;


  // update the attrcat record with the newName
  RecBuffer attrcatBuffer(attrToRenameRecId.block);
  attrcatBuffer.getRecord(attrCatRecord, attrToRenameRecId.slot);

  strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
  attrcatBuffer.setRecord(attrCatRecord, attrToRenameRecId.slot);

  return SUCCESS;
}