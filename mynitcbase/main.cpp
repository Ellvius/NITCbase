#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;

  /* Initialize static buffer & Open relation table */
  StaticBuffer buffer;
  OpenRelTable cache;


  for(int relId = 0; relId < 3; relId++){ // rel-id 0 (relation catalog) & 1 (attribute catalog)
    // Get relation catalog entry from relation cache table
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    printf("Relation: %s\n", relCatEntry.relName);

    for(int attrOffset = 0; attrOffset < relCatEntry.numAttrs; attrOffset++){
      // Get attribute catalog entry for relId & attribute offset
      AttrCatEntry attrCatEntry;
      AttrCacheTable::getAttrCatEntry(relId, attrOffset, &attrCatEntry);

      const char *attrType = attrCatEntry.attrType == NUMBER ? "NUM" : "STR";

      printf("  %s: %s\n", attrCatEntry.attrName, attrType);
    }
    printf("\n");
  }

  return 0;
}