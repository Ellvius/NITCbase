#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

OpenRelTable::OpenRelTable(){

  // initialize relation cache & attribute cache with nullptr
  for(int i = 0; i < MAX_OPEN; i++){
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
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



  /**** setting up Students relation in the Relation Cache Table ****/

  int studentsRelCatSlot = -1;
  Attribute studentsRecord[RELCAT_NO_ATTRS];

  HeadInfo relCatHeader;
  relCatBlock.getHeader(&relCatHeader);

  for(int slotNum = 0; slotNum <= relCatHeader.numSlots; slotNum++){
    relCatBlock.getRecord(studentsRecord, slotNum);

    if(strcmp(studentsRecord[RELCAT_REL_NAME_INDEX].sVal, "Students") == 0){
      studentsRelCatSlot = slotNum;
      break;
    }
  }

  if(studentsRelCatSlot != -1){
    RelCacheEntry studentsRelCacheEntry;

    RelCacheTable::recordToRelCatEntry(
      studentsRecord, 
      &studentsRelCacheEntry.relCatEntry
    );

    studentsRelCacheEntry.recId.block = RELCAT_BLOCK;
    studentsRelCacheEntry.recId.slot = studentsRelCatSlot;

    RelCacheTable::relCache[2] = (struct RelCacheEntry *)std::malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[2]) = studentsRelCacheEntry;
  }

  /**** setting up Students relation in the Attribute Cache Table ****/
  HeadInfo attrCatHeader;
  attrCatBlock.getHeader(&attrCatHeader);

  AttrCacheEntry *studentsHead = nullptr;

  for (int slotNum = attrCatHeader.numSlots - 1;
       slotNum >= 0;
       slotNum--){

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, slotNum);

    if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,"Students") != 0){
      continue;
    }

    AttrCacheEntry *entry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &entry->attrCatEntry
    );

    entry->recId.block = ATTRCAT_BLOCK;
    entry->recId.slot = slotNum;

    entry->next = studentsHead;
    studentsHead = entry;
  }

  AttrCacheTable::attrCache[2] = studentsHead;
}


OpenRelTable::~OpenRelTable(){

  // Free relation cache entries for relation catalog & attribute catalog
  free(RelCacheTable::relCache[RELCAT_RELID]);
  free(RelCacheTable::relCache[ATTRCAT_RELID]);
  free(RelCacheTable::relCache[2]); // Students relation

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

  // Free attribute cache entries for Students Relation
  current = AttrCacheTable::attrCache[2];

  while (current != nullptr){
    AttrCacheEntry *next = current->next;
    free(current);
    current = next;
  }
}