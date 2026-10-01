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