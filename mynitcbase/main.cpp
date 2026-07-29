#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  // StaticBuffer buffer;
  // OpenRelTable cache;

  // Test writing into the disk
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, 7000);
  char message[] = "hello\n";
  memcpy(buffer + 20, message, 7);
  Disk::writeBlock(buffer, 7000);

  // Test reading the changes from the disk
  unsigned char buffer2[BLOCK_SIZE];
  char message2[7];
  Disk::readBlock(buffer2, 7000);
  memcpy(message2, buffer2 + 20, 7);
  std::cout << message2;

  unsigned char mapBuf[BLOCK_SIZE];
  char Bmap[BLOCK_SIZE];
  Disk::readBlock(mapBuf, 0);
  
  for(int i = 0; i < BLOCK_SIZE; i++){
    std::cout<< static_cast<int>(mapBuf[i])<< " ";
  }
  std::cout<<'\n';

  // return FrontendInterface::handleFrontend(argc, argv);
  return 0;
}