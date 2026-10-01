#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable(){

  // initialize relation cache & attribute cache with nullptr
  for(int i = 0; i < MAX_OPEN; i++){
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    OpenRelTable::tableMetaInfo[i].free = true;
  }

  /************ Setting up Relation Cache entries ************/
  // (we need to populate relation cache with entries for the relation catalog
  //  and attribute catalog.)


  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry *)std::malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;


  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
  Attribute attrCatRelRecord[ATTRCAT_NO_ATTRS];
  relCatBlock.getRecord(attrCatRelRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  RelCacheEntry attrCatRelCacheEntry;
  RelCacheTable::recordToRelCatEntry(attrCatRelRecord, &attrCatRelCacheEntry.relCatEntry);
  attrCatRelCacheEntry.recId.block = ATTRCAT_BLOCK;
  attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry *)std::malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrCatRelCacheEntry;



  /************ Setting up Attribute cache entries ************/
  // (we need to populate attribute cache with entries for the relation catalog
  //  and attribute catalog.)


  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  
  // iterate through all the attributes of the relation catalog and create a linked
  // list of AttrCacheEntry (slots 0 to 5)
  AttrCacheEntry *head = nullptr;

  for (int slotNum = RELCAT_NO_ATTRS - 1; slotNum >= 0; slotNum--){

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, slotNum);

    AttrCacheEntry *attrCacheEntry = 
      (struct AttrCacheEntry *)std::malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
      attrCatRecord, 
      &attrCacheEntry->attrCatEntry
    );

    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = slotNum;

    attrCacheEntry->next = head;
    head = attrCacheEntry;
  }

  AttrCacheTable::attrCache[RELCAT_RELID] = head;


  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  head = nullptr;

  for(int slotNum = RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS - 1; 
    slotNum >= RELCAT_NO_ATTRS; 
    slotNum--){

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, slotNum);

    AttrCacheEntry* attrCacheEntry = 
      (struct AttrCacheEntry *)std::malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
      attrCatRecord,
      &attrCacheEntry->attrCatEntry
    );

    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = slotNum;

    attrCacheEntry->next = head;
    head = attrCacheEntry;
  }
  
  AttrCacheTable::attrCache[ATTRCAT_RELID] = head;


  /************ Setting up tableMetaInfo entries ************/

  OpenRelTable::tableMetaInfo[RELCAT_RELID].free = false;
  OpenRelTable::tableMetaInfo[ATTRCAT_RELID].free = false;

  strcpy(OpenRelTable::tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);
  strcpy(OpenRelTable::tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}


OpenRelTable::~OpenRelTable(){
  // close all open relations excepts relation & attribute catalogs
  for (int i = 2; i < MAX_OPEN; ++i){
    if (!tableMetaInfo[i].free){
      OpenRelTable::closeRel(i); 
    }
  }

  // Free relation cache entries for relation catalog & attribute catalog
  free(RelCacheTable::relCache[RELCAT_RELID]);
  free(RelCacheTable::relCache[ATTRCAT_RELID]);

  // Free attribute cache entries for relation catalog
  AttrCacheEntry *current = AttrCacheTable::attrCache[RELCAT_RELID];

  while(current != nullptr){
    AttrCacheEntry *next = current->next;
    free(current);
    current = next;
  }

  // Free attribute cache entries for attribute catalog
  current = AttrCacheTable::attrCache[ATTRCAT_RELID];

  while(current != nullptr){
    AttrCacheEntry* next = current->next;
    free(current);
    current = next;
  }
}


int OpenRelTable::getFreeOpenRelTableEntry(){
  // find free open relation table entry from tableMetaInfo
  for(int relid = 2; relid < MAX_OPEN; relid++){
    if(OpenRelTable::tableMetaInfo[relid].free)
      return relid;
  }

  return E_CACHEFULL;
}


int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
  // search tableMetaInfo for relid of relation relName
  for (int relid = 0; relid < MAX_OPEN; relid++){
    if (!tableMetaInfo[relid].free && strcmp(OpenRelTable::tableMetaInfo[relid].relName, relName) == 0)
      return relid;
  }

  return E_RELNOTOPEN;
}


int OpenRelTable::openRel(char relName[ATTR_SIZE]){
  // if relation is already open
  int exisitingRelId = OpenRelTable::getRelId(relName);

  if ( exisitingRelId != E_RELNOTOPEN)
    return exisitingRelId;

  
  // get free open relation table entry slot
  int relId = OpenRelTable::getFreeOpenRelTableEntry();

  if (relId == E_CACHEFULL)
    return E_CACHEFULL;


  /****** Setting up Relation Cache entry for the relation ******/

  Attribute RelNameAttribute; // attribute value for linear search
  memcpy(RelNameAttribute.sVal, relName, ATTR_SIZE);

  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, RelNameAttribute,  EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) 
    return E_RELNOTEXIST; // relation not found in relcat

  // load relation cache entry
  RecBuffer recBuffer(relcatRecId.block);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  recBuffer.getRecord(relCatRecord, relcatRecId.slot);

  RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId = relcatRecId;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[relId] = (struct RelCacheEntry *)std::malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[relId]) = relCacheEntry;


  /****** Setting up Attribute Cache entry for the relation ******/

  AttrCacheEntry *listHead = nullptr;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  // iterate over all the entries in attribute catalog having relName as the given relName
  while(true){
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char *)ATTRCAT_ATTR_RELNAME, RelNameAttribute, EQ);

    // load and add the entry to attribute cache
    if(attrcatRecId.block != -1 && attrcatRecId.slot != -1){
      RecBuffer attrRecBuffer(attrcatRecId.block);

      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrRecBuffer.getRecord(attrCatRecord, attrcatRecId.slot);

      AttrCacheEntry *attrCacheEntry =
          (struct AttrCacheEntry *)std::malloc(sizeof(AttrCacheEntry));
  
      AttrCacheTable::recordToAttrCatEntry(
          attrCatRecord,
          &attrCacheEntry->attrCatEntry);

      attrCacheEntry->recId.block = attrcatRecId.block;
      attrCacheEntry->recId.slot = attrcatRecId.slot;
  
      attrCacheEntry->next = listHead;
      listHead = attrCacheEntry;
    }
    else {
      break;
    }
  }

  AttrCacheTable::attrCache[relId] = listHead;


  /****** Setting up metadata in the Open Relation Table for the relation******/

  OpenRelTable::tableMetaInfo[relId].free = false;
  strcpy(OpenRelTable::tableMetaInfo[relId].relName , relName);

  return relId;
}

int OpenRelTable::closeRel(int relId){
  // do not close relation & attribute catalog
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    return E_NOTPERMITTED;

  if (relId < 0 || relId >= MAX_OPEN)
    return E_OUTOFBOUND;

  if (OpenRelTable::tableMetaInfo[relId].free)
    return E_RELNOTOPEN;

  // free allocated memory in relation cache
  free(RelCacheTable::relCache[relId]);

  // free allocated memory in attribute cache
  AttrCacheEntry *current = AttrCacheTable::attrCache[relId];

  while (current != nullptr){
    AttrCacheEntry *next = current->next;
    free(current);
    current = next;
  }

  // update tableMetaInfo
  OpenRelTable::tableMetaInfo[relId].free = true;

  // update relcache and attrcache entries
  RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;

  return SUCCESS;
}
