#include "Algebra.h"

#include <cstring>
#include <iostream>


int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
  // operation not allowed for relation & attribute catalog
  if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME)== 0)
    return E_NOTPERMITTED;

  int relId = OpenRelTable::getRelId(relName);

  if(relId == E_RELNOTOPEN)
    return E_RELNOTOPEN;

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  // check whether the number of attributes match
  if(relCatEntry.numAttrs != nAttrs)
    return E_NATTRMISMATCH;

  // let recordValues[numberOfAttributes] be an array of type union Attribute
  Attribute recordValues[nAttrs];

  // Iterate through the attribute cat entries to check attribute type
  // assign values to a record buffer
  for(int i = 0; i < nAttrs; i++)  {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

    int type = attrCatEntry.attrType;

    if (type == NUMBER){
      if(isNumber(record[i]))
           recordValues[i].nVal = atof(record[i]);
      else 
        return E_ATTRTYPEMISMATCH;
    }
    else if (type == STRING){
      strcpy(recordValues[i].sVal, record[i]);
    }
  }

  // insert the record buffer into the relation
  return BlockAccess::insert(relId, recordValues);
}



int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
  // check if source relation is open
  int srcRelid = OpenRelTable::getRelId(srcRel);
  if(srcRelid == E_RELNOTOPEN)
    return E_RELNOTOPEN;

  // check whether the given attribute exists
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(srcRelid, attr, &attrCatEntry);
  if(ret != SUCCESS)
    return ret;

  // convert strVal to Attribute of appropriate type
  Attribute attrVal;
  int type = attrCatEntry.attrType;

  if (type == NUMBER){
    if(isNumber(strVal))
      attrVal.nVal = atof(strVal);
    else
      return E_ATTRTYPEMISMATCH;
  }
  else if (type == STRING)
    strcpy(attrVal.sVal, strVal);

  // generate the arguments to create and open target relation
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelid, &relCatEntry);

  int srcNAttrs = relCatEntry.numAttrs;
  char attrNames[srcNAttrs][ATTR_SIZE];
  int attrTypes[srcNAttrs];

  // iterate through all the attrcat entries and copy attrName & type
  for(int i = 0; i < srcNAttrs; i++){
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelid, i, &attrCatEntry);

    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }

  // create and open target relation with appropriate arguments
  ret = Schema::createRel(targetRel, srcNAttrs, attrNames, attrTypes);
  if(ret != SUCCESS)
    return ret;

  int targetRelId = OpenRelTable::openRel(targetRel);
  if(targetRelId < 0 || targetRelId >= MAX_OPEN){
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  
  // BlockAccess::search() can either do linearSearch or B+ tree search
  // reset both searchIndexes to start from first record
  RelCacheTable::resetSearchIndex(srcRelid);
  AttrCacheTable::resetSearchIndex(srcRelid, attr);
  
  Attribute record[srcNAttrs];

  // search till we find records satisfying the condition &
  // insert the records found into target relation
  while (true){
    int ret = BlockAccess::search(srcRelid, record, attr, attrVal, op);
    if(ret != SUCCESS)
     break;

    ret = BlockAccess::insert(targetRelId, record);

    if(ret != SUCCESS){
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // close target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}



int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]){
  // check if the source relation is open
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if(srcRelId == E_RELNOTOPEN)
    return E_RELNOTOPEN;

  // generate the arguments to create and open target relation
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

  int srcNAttrs = relCatEntry.numAttrs;
  int attrOffset[tar_nAttrs]; // stores offsets of ith attr of target relation in source relation
  int attrTypes[tar_nAttrs];

  // check is attributes of target relation are present in source relation &
  // update attrOffset and attrType, if present
  for(int i = 0; i < tar_nAttrs; i++){
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);

    if(ret == E_ATTRNOTEXIST)
      return E_ATTRNOTEXIST;

    attrOffset[i] = attrCatEntry.offset;
    attrTypes[i] = attrCatEntry.attrType;
  }

  // create and open target relation
  int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attrTypes);
  if(ret != SUCCESS)
    return ret;

  int targetRelId = OpenRelTable::openRel(targetRel);
  if(targetRelId < 0 || targetRelId >= MAX_OPEN){
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  // reset search index to copy from the first record of the source relation
  RelCacheTable::resetSearchIndex(srcRelId);

  // iterate till a record exists in source relation &
  // insert the subset of record into the target relation
  Attribute record[srcNAttrs];
  while (true){
    int ret = BlockAccess::project(srcRelId, record);
    if(ret != SUCCESS)
      break;

    // iterate through target attributes &
    // copy the appropriate attributes from source to target rcd
    Attribute projRecord[tar_nAttrs];
    for(int i = 0; i < tar_nAttrs; i++)
      projRecord[i] = record[attrOffset[i]];

    ret = BlockAccess::insert(targetRelId, projRecord);

    if (ret != SUCCESS){
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // close the target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}



int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]){
  // check is the source relation is open
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if(srcRelId == E_RELNOTOPEN)
    return E_RELNOTOPEN;

  // generate arguments to create and open target relation
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

  int numAttrs = relCatEntry.numAttrs;
  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];

  // iterate through every attribute of source relation &
  // copy attribute name and attribute type
  for(int i = 0; i < numAttrs; i++){
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }

  // create and open target relation
  int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if(ret != SUCCESS)
    return ret;

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0 || targetRelId >= MAX_OPEN){
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  // reset search index to copy from the first record of the source relation
  RelCacheTable::resetSearchIndex(srcRelId);

  // iterate till a record exists in source relation &
  // insert the record into the target relation
  Attribute record[numAttrs];
  while (true){
    int ret = BlockAccess::project(srcRelId, record);
    if(ret != SUCCESS)
      break;

    ret = BlockAccess::insert(targetRelId, record);

    if (ret != SUCCESS){
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // close the target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}



// to check if the string can be parsed as floating point integer
bool isNumber(char *str){
  int len;
  float ignore;

  // sscanf returns the number of elements read
  // returns 0 if no float matching else 1
  // only reads the first float ignoring whitespaces
  int ret = sscanf(str, "%f %n", &ignore, &len);

  // len == strlen(str) is used to check whether the string contains whitespaces
  return ret == 1 && len == strlen(str);
}
