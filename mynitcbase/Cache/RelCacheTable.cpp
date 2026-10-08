#include "RelCacheTable.h"

#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];



int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf){
  if(relId < 0 || relId >= MAX_OPEN)
    return E_OUTOFBOUND;

  //if there's no entry in the rel-id
  if(relCache[relId] == nullptr)
    return E_RELNOTOPEN;

  // copy the value to the relCatBuf argument
  *relCatBuf = relCache[relId]->relCatEntry;

  return SUCCESS;
}



int RelCacheTable::setRelCatEntry(int relId, RelCatEntry *relCatBuf){
  if (relId < 0 || relId >= MAX_OPEN)
    return E_OUTOFBOUND;

  // if there's no entry in the rel-id
  if (relCache[relId] == nullptr)
    return E_RELNOTOPEN;

  // copy the relCatBuf to the corresponding relCache entry
  relCache[relId]->relCatEntry = *relCatBuf;
  relCache[relId]->dirty = true;

  return SUCCESS;
}



/* will return the searchIndex for the relation corresponding to `relId
NOTE: this function expects the caller to allocate memory for `*searchIndex`
*/
int RelCacheTable::getSearchIndex(int relId, RecId *searchIndex){
  // check relId bounds
  if(relId < 0 || relId >= MAX_OPEN){
    return E_OUTOFBOUND;
  }

  if(relCache[relId] == nullptr){
    return E_RELNOTOPEN;
  }

  // copy the searchIndex field of the Relation Cache entry
  *searchIndex = relCache[relId]->searchIndex;
  return SUCCESS;
}

/*
 * Sets the search index for the specified relation in relation cache
 */
int RelCacheTable::setSearchIndex(int relId, RecId *searchIndex){
  // validate relation ID
  if(relId < 0 || relId >= MAX_OPEN){
    return E_OUTOFBOUND;
  }

  // check if relation is open
  if(relCache[relId] == nullptr){
    return E_RELNOTOPEN;
  }

  // update the relation's search index
  relCache[relId]->searchIndex = *searchIndex;

  return SUCCESS;
}

/**
 * Resets the search index for the specified relation to {-1, -1}.
 */
int RelCacheTable::resetSearchIndex(int relId){

  RecId searchIndex = {-1, -1};
  return RelCacheTable::setSearchIndex(relId, &searchIndex);
}

// Converts a relation catalog record to RelCatEntry struct
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry *relCatEntry){
  strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
  relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
  relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
  relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
  relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
  relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}

void RelCacheTable::relCatEntryToRecord(RelCatEntry *relCatEntry, union Attribute record[RELCAT_NO_ATTRS]){
  strcpy(record[RELCAT_REL_NAME_INDEX].sVal, relCatEntry->relName);
  record[RELCAT_NO_ATTRIBUTES_INDEX].nVal = relCatEntry->numAttrs;
  record[RELCAT_NO_RECORDS_INDEX].nVal = relCatEntry->numRecs;
  record[RELCAT_FIRST_BLOCK_INDEX].nVal = relCatEntry->firstBlk;
  record[RELCAT_LAST_BLOCK_INDEX].nVal = relCatEntry->lastBlk;
  record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = relCatEntry->numSlotsPerBlk;
}