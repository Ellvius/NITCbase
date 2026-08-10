#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  StaticBuffer buffer;
  // OpenRelTable cache;

  // Create relation catalog object and load its header into relCatHeader
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);
  
  // Loop through all the relation catalog records
  for (int i = 0; i < relCatHeader.numEntries; i++) {
    
    // Declare relCatRecord and load the ith relation catalog record into it
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);
    
    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
    
    int currAttrBlock = ATTRCAT_BLOCK;
    
    while(currAttrBlock != -1){

      // Create current attribute catalog object and load its header into attrCatHeader
      RecBuffer attrCatBuffer(currAttrBlock);
      HeadInfo attrCatHeader;
      attrCatBuffer.getHeader(&attrCatHeader);

      // Loop through all the current attribute catalog records
      for (int j = 0; j < attrCatHeader.numEntries; j++) {
  
        // Declare attrCatRecord and load the jth attribute catalog entry into it
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        // Update the Student relation
        if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0 && 
          strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class") == 0) {
          
          strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Batch");
        }
  
        // Only print the attribute catalog entry that corresponds to the current relation
        if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
          const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
          printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
        }
      }

      currAttrBlock = attrCatHeader.rblock;
    }

    printf("\n");
  }

  // return FrontendInterface::handleFrontend(argc, argv);
  return 0;
}