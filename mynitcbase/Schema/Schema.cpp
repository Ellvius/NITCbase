#include "Schema.h"

#include <cmath>
#include <cstring>


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