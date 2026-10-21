#include "BPlusTree.h"

#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op){
  // get exisiting searchIndex
  IndexId searchIndex;
  int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
  if(ret != SUCCESS)
    return RecId{-1, -1};

  // get attrcat entry of the attribute
  AttrCatEntry attrCatEntry;
  ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
    return RecId{-1, -1};

  // block and index will be used to keep track of current elements in search
  int block, index;

  // set the correct block and index according to previous searchIndex
  // if search is done for the first time
  if (searchIndex.block == -1 && searchIndex.index == -1){
    // start the search from the first entry of root.
    block = attrCatEntry.rootBlock;
    index = 0;

    // if attribute doesn't have an index
    if (block == -1)
      return RecId{-1, -1};
  }
  else{
    // a previous search hit exists in leafBlock
    block = searchIndex.block;
    index = searchIndex.index + 1;

    // load leaf block and header
    IndLeaf leaf(block);
    HeadInfo leafHead;
    leaf.getHeader(&leafHead);

    // if all entries in the current leaf has been searched
    // start from next block
    if (index >= leafHead.numEntries){
      block = leafHead.rblock;
      index = 0;

      // end of linked list of leaf blocks reached - no more entries
      if (block == -1)
        return RecId{-1, -1};
    }
  }

  // if a new search is started and the rootblock is internal index block
  // traverse through the internal blocks to find the first entry in the leaf blocks
  // satisfying the given condition
  while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL){ 
    // load internal block and its header
    IndInternal internalBlk(block);
    HeadInfo intHead;
    internalBlk.getHeader(&intHead);

    InternalEntry intEntry;

    // NE: need to search the entire linked list - move left
    // LT & LE: since values are in asc order, values if satisfying 
    //          will always exist in the left most index - move left
    // EQ, GT & GE: move to the left of the first entry that is GE
    //              all entries satisfying will start from this leaf
    if (op == NE || op == LT || op == LE){
      internalBlk.getEntry(&intEntry, 0);
      block = intEntry.lChild;
    }
    else{
      // traverse through all entries in internal block
      int i;
      for(i = 0; i < intHead.numEntries; i++){
        internalBlk.getEntry(&intEntry, i);

        int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
        if(
          (op == EQ && cmpVal >= 0) || 
          (op == GE && cmpVal >= 0) ||
          (op == GT && cmpVal > 0)
        ){
            break;
        }
      }

      // if an entry is found, go to its left
      // otherwise go to the right of the last entry - indEntry
      if (i != intHead.numEntries)
        block = intEntry.lChild;
      else
        block = intEntry.rChild;
    }
  }

  // we have reached the leaf node
  // find the leaf entry satisfying the condition by iterating linearly
  while (block != -1){
    // load leaf block and header
    IndLeaf leafBlk(block);
    HeadInfo leafHead;
    leafBlk.getHeader(&leafHead);

    Index leafEntry;

    // iterate through every index in leaf index block
    while (index < leafHead.numEntries){
      // load leaf entry and compare to find satisfying entry
      leafBlk.getEntry(&leafEntry, index);

      int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

      // satisfying leaf entry found
      if (
        (op == EQ && cmpVal == 0) ||
        (op == LE && cmpVal <= 0) ||
        (op == LT && cmpVal < 0) ||
        (op == GT && cmpVal > 0) ||
        (op == GE && cmpVal >= 0) ||
        (op == NE && cmpVal != 0)
      ){
        IndexId searchIndex = {block, index};
        AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

        return RecId{leafEntry.block, leafEntry.slot};
      }
      else if ((op == EQ || op == LE || op == LT) && cmpVal > 0){
        // stop the search for these since future entries will not satisfy condition
        // because entries are arranged in ascending order
        return RecId{-1, -1};
      }

      ++index;
    }

    // only for NE we need to iterate through every leaf entry
    if (op != NE)
      break;

    // every leafEntry in the current leafblock checked, go to the next leafblock
    block = leafHead.rblock;
    index = 0;
  }

  // no entry satisfying the condition was found
  return RecId{-1, -1};
}