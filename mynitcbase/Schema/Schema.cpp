#include "Schema.h"

#include <cmath>
#include <cstring>



int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]){
  // Search whether a relation with same name already exists
  Attribute relNameAsAttribute;
  strcpy(relNameAsAttribute.sVal, relName);

  RecId targetRecId;
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  targetRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAsAttribute, EQ);
  
  if(targetRecId.block != -1 && targetRecId.slot != -1)
    return E_RELEXIST;

  // Check duplicates in given attribute list
  for(int i = 0; i < nAttrs; i++){
    for(int j = i+1; j < nAttrs; j++){
      if(strcmp(attrs[i], attrs[j]) == 0)
        return E_DUPLICATEATTR;
    }
  }

  // Insert new Relation Catalog record for the new relation
  Attribute relCatRecord[RELCAT_NO_ATTRS];

  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor(2016 / (16 * nAttrs + 1));

  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);

  if(retVal != SUCCESS)
    return retVal;

  // Iterate and insert new attribute catalog entries for the relation
  for(int i = 0; i < nAttrs; i++){
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);

    // if not successful, delete the already inserted entries from relation catalog
    // using Schema::deleteRel
    if(retVal != SUCCESS){
      Schema::deleteRel(relName);
      return E_DISKFULL;
    }
  }

  return SUCCESS;
}



int Schema::deleteRel(char *relName){
  // Deletion of Relation & Attribute Catalog is not permitted
  if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

  // Check whether the relation is open before deleting
  int relId = OpenRelTable::getRelId(relName);

  if(relId != E_RELNOTOPEN)
    return E_RELOPEN;

  return BlockAccess::deleteRelation(relName);
}

int Schema::openRel(char relName[ATTR_SIZE]){
  // returns valid relid otherwise negative error codes
  int ret = OpenRelTable::openRel(relName); 

  if(ret >= 0)
    return SUCCESS;

  return ret;
}


int Schema::closeRel(char relName[ATTR_SIZE]){
  // cannot close relation and attribute catalogs
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;
  
  // returns relid of open relation otherwise error RELNOTOPEN
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN)
    return E_RELNOTOPEN;

  return OpenRelTable::closeRel(relId);
}


int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){
  // cannot rename relation & attribute catalogs
  if (strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
      strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
      strcmp(newRelName, RELCAT_RELNAME) == 0 ||
      strcmp(newRelName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

  // relation has to be closed before renaming
  if(OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN)
    return E_RELOPEN;

  return BlockAccess::renameRelation(oldRelName, newRelName);
}


int Schema::renameAttr(char relName[ATTR_SIZE],
  char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]){
  // cannot rename relation & attribute catalogs
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

  // relation has to be closed before renaming
  if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN)
    return E_RELOPEN;

  return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
}