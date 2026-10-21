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



int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op){
  // get the attribute catalog entry of the attribute to get rootblock
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if(ret != SUCCESS)
    return ret;
  
  // if index does not exists => rootBlock is -1, do linearSearch
  // otherwise do BplusSearch
  RecId recId;
  if (attrCatEntry.rootBlock == -1)
    recId = linearSearch(relId, attrName, attrVal, op);
  else 
    recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);

  // if no record satisfying the given condition is found
  if (recId.block == -1 && recId.slot == -1)
    return E_NOTFOUND;

  // copy the record to the record buffer
  RecBuffer searchBlock(recId.block);
  searchBlock.getRecord(record, recId.slot);

  return SUCCESS;
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



int BlockAccess::deleteRelation(char relName[ATTR_SIZE]){
  // deletion of relation & attribute catalogy not permitted
  if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

  // get the relation catalog entry of the relation to be deleted
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr; 
  strcpy(relNameAttr.sVal, relName);

  RecId searchRes = linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

  if(searchRes.block == -1 && searchRes.slot == -1)
    return E_RELNOTEXIST;

  Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
  RecBuffer relCatBlock(searchRes.block);
  relCatBlock.getRecord(relCatEntryRecord, searchRes.slot);

  // get the first block & number of attributes from relation catalog record of the relation
  int blockNum = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
  int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

  // iterate through the linked list of blocks and
  // Release all the blocks assigned to the relation
  while(blockNum != -1){
    RecBuffer curBlock(blockNum);

    HeadInfo header;
    curBlock.getHeader(&header);

    int nextBlock = header.rblock;
    curBlock.releaseBlock();

    blockNum = nextBlock;
  }

  // Delete attribute catalog entries of the relation

  // reset the searchIndex of the attribute catalog
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  int numberOfAttributesDeleted = 0;

  // delete the attribute catalog entries till no more entries are found
  while (true){
    RecId attrCatRecId = linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

    if(attrCatRecId.block == -1 && attrCatRecId.slot == -1)
      break;

    numberOfAttributesDeleted++;

    // get the record
    RecBuffer attrCatBlock(attrCatRecId.block);

    HeadInfo curHeader;
    attrCatBlock.getHeader(&curHeader);

    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatEntryRecord, attrCatRecId.slot);

    // get rootblock, later used to delete indexes
    int rootBlock = attrCatEntryRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

    // update slotmap in the block header
    unsigned char slotMap[curHeader.numSlots];
    attrCatBlock.getSlotMap(slotMap);

    slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
    attrCatBlock.setSlotMap(slotMap);

    // update number of entries in the block header
    curHeader.numEntries--;
    attrCatBlock.setHeader(&curHeader);

    // if number of entries becomes 0
    // fix the linked list and release the block
    if (curHeader.numEntries == 0){
      // set the right block of the left block 
      // to the right block of current block
      RecBuffer leftBlock(curHeader.lblock);

      HeadInfo leftHeader;
      leftBlock.getHeader(&leftHeader);

      leftHeader.rblock = curHeader.rblock;
      leftBlock.setHeader(&leftHeader);

      // if current block is the last block
      // update the last block in the relation catalog
      if ( curHeader.rblock == -1){
        relCatEntryRecord[RELCAT_LAST_BLOCK_INDEX].nVal = curHeader.lblock;
      }
      else {
        // set the left block of the right block
        // to the left block of the current block
        RecBuffer rightBlock(curHeader.rblock);

        HeadInfo rightHeader;
        rightBlock.getHeader(&rightHeader);

        rightHeader.lblock = curHeader.lblock;
        rightBlock.setHeader(&rightHeader);
      }

      // since attribute catalog will never be empty - because of relcat & attrcat entries
      // we can safely release the current block
      attrCatBlock.releaseBlock();
    }

    // index deletion code to be written here
  }

  // Delete the relation catalog entry of the record
  HeadInfo relCatHeader;
  relCatBlock.getHeader(&relCatHeader);
  
  // update slotmap in relation catalog header
  unsigned char slotMap[relCatHeader.numSlots];
  relCatBlock.getSlotMap(slotMap);
  
  slotMap[searchRes.slot] = SLOT_UNOCCUPIED;
  relCatBlock.setSlotMap(slotMap);
  
  // decrement number of entries in relation catalog header
  relCatHeader.numEntries--;
  relCatBlock.setHeader(&relCatHeader);

  // Update the relation cache entries of relation & attribute catalogs
  // decrease number of records in relation catalog by 1
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);

  relCatEntry.numRecs--;
  RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

  // decrease number of records in attribute catalog by numberOfAttributesDeleted
  RelCatEntry attrCatEntry;
  RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);

  attrCatEntry.numRecs = attrCatEntry.numRecs - numberOfAttributesDeleted;
  RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);

  return SUCCESS;
}



int BlockAccess::project(int relId, Attribute *record){
  // get the previous record id
  RecId prevRecId;
  RelCacheTable::getSearchIndex(relId, &prevRecId);

  int blockNum, slot;

  // check whether the search index is reset 
  // set blockNum, slot accordingly
  if (prevRecId.block == -1 && prevRecId.slot == -1){
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    blockNum = relCatEntry.firstBlk;
    slot = 0;
  }
  else{
    blockNum = prevRecId.block;
    slot = prevRecId.slot + 1;
  }

  // Iterate to find the next valid blockNum & slot
  while (blockNum != -1){
    RecBuffer curBlock(blockNum);

    HeadInfo curHeader;
    curBlock.getHeader(&curHeader);

    unsigned char curSlotMap[curHeader.numSlots];
    curBlock.getSlotMap(curSlotMap);

    // check if the slot is valid and unoccupied
    if (slot >= curHeader.numSlots){
      blockNum = curHeader.rblock;
      slot = 0;
    }
    else if (curSlotMap[slot] == SLOT_UNOCCUPIED)
      slot++;
    else
      break;
  }

  // block is not valid - no record found
  if (blockNum == -1)
    return E_NOTFOUND;

  // update the search index with new rec id & copy the record
  RecId nextRecId{blockNum, slot};
  RelCacheTable::setSearchIndex(relId, &nextRecId);

  RecBuffer recordBlock(nextRecId.block);
  recordBlock.getRecord(record, nextRecId.slot);

  return SUCCESS;
}