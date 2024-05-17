// Ryan Joseph Talalai
// CMPEN431 FA22

#include "YOURCODEHERE.h"

/**********************************************************************
    Function    : lg2pow2
    Description : this help funciton for you to calculate the bit number
                  this function is not allowed to modify
    Input       : pow2 - for example, pow2 is 16
    Output      : retval - in this example, retval is 4
***********************************************************************/

unsigned int lg2pow2(uint64_t pow2){
  unsigned int retval=0;
  while(pow2 != 1 && retval < 64){
    pow2 = pow2 >> 1;
    ++retval;
  }
  return retval;
}

void  setSizesOffsetsAndMaskFields(cache* acache, unsigned int size, unsigned int assoc, unsigned int blocksize){

  unsigned int localVAbits=8*sizeof(uint64_t*);
  if (localVAbits!=64){
    fprintf(stderr,"Running non-portable code on unsupported platform, terminating. Please use designated machines.\n");
    exit(-1);
  }

  unsigned int sizeIndex;

  acache->numways = assoc;
  acache->blocksize = blocksize;
  acache->numsets = size / blocksize / assoc;
  
  acache->numBitsForBlockOffset = lg2pow2(blocksize);                                           // Block bit number
  acache->numBitsForIndex = lg2pow2(acache->numsets) + acache->numBitsForBlockOffset;           // Index bit number for tag         

  for (int cur_bit_i = 0; cur_bit_i < (lg2pow2(acache->numsets)); ++cur_bit_i) {
    acache->VAImask |= ((uint64_t)1 << cur_bit_i);                                              // Virtual Address Index mask
  }

  for (int cur_bit_t = 0; cur_bit_t < (localVAbits - acache->numBitsForIndex); ++cur_bit_t) {
    acache->VATmask |= ((uint64_t)1 << cur_bit_t);                                              // Virtual Address Tag mask
  }
}


unsigned long long getindex(cache* acache, unsigned long long address){
  
  unsigned long long index_ret = address >> acache->numBitsForBlockOffset;
  index_ret &= acache->VAImask;

  return index_ret;
}


unsigned long long gettag(cache* acache, unsigned long long address){

  unsigned long long tag_ret = address >> acache->numBitsForIndex;
  tag_ret &= acache->VATmask;

  return tag_ret;
}


void writeback(cache* acache, unsigned int index, unsigned int waynum){                     // oldestway changed to waynum in function declaration to match definition in header file

  unsigned long long address = 0;
  unsigned long long wb_index = index << acache->numBitsForBlockOffset;
  unsigned long long wb_tag = acache->sets[index].blocks[waynum].tag << acache->numBitsForIndex;
  
  // Address creation  
  address = ((address | wb_index) | wb_tag);  

  for(int cur_word = 0; cur_word < (acache->blocksize / 8); ++cur_word){
    unsigned long long read_lc = acache->sets[index].blocks[waynum].datawords[cur_word];                  // Read local contents
    performaccess(acache->nextcache, address, 8, 1, read_lc, 0);                                          // Write data, size fixed to 8
    address += 8;
  }
}


void fill(cache * acache, unsigned int index, unsigned int waynum, unsigned long long address){

  unsigned long long fill_index = acache->VAImask << acache->numBitsForBlockOffset;
  unsigned long long fill_tag = acache->VATmask << acache->numBitsForIndex;
  
  // Address creation
  address &= (fill_tag | fill_index);

  for(int cur_word = 0; cur_word < (acache->blocksize / 8); ++cur_word){
    unsigned long long read_nl = performaccess(acache->nextcache, address, 8, 0, 0, 0);                    // Read data from next level
    acache->sets[index].blocks[waynum].datawords[cur_word] = read_nl;                                      // Set local contents
    acache->sets[index].blocks[waynum].tag = gettag(acache, address);                                      // Update tag
    address += 8;
  }
}
