#include "bitmask.h"
#include <rte_config.h>
#include <rte_common.h>
#include "rte_malloc.h"
#include "debug.h"
#include <rte_cycles.h>

struct mg_bitmask * mg_bitmask_create(uint16_t size){
  uint16_t n_blocks = (size-1)/64 + 1;
  struct mg_bitmask *mask = rte_zmalloc(NULL, sizeof(struct mg_bitmask) + (size-1)/64 * 8 + 8, 0);
  mask->size = size;
  mask->n_blocks = n_blocks;
  return mask;
}
void mg_bitmask_free(struct mg_bitmask * mask){
  rte_free(mask);
}

void mg_bitmask_clear_all(struct mg_bitmask * mask){
  // TODO: check if memset() would be faster for 64bit values...
  uint16_t i;
  for(i=0; i< mask->n_blocks; i++){
    mask->mask[i] = 0;
  }
}

// This will only touch the first n bits. If other bits are set/cleared, they
// will not be affected
void mg_bitmask_set_n_one(struct mg_bitmask * mask, uint16_t n){
  // TODO: check if memset() would be faster for 64bit values...
  uint64_t * msk = mask->mask;
  while(n>=64){
    *msk = 0xffffffffffffffff;
    n -= 64;
    msk++;
  }
  if(n & 0x3f){
    *msk |= (0xffffffffffffffff >> (64-n));
  }
}

void mg_bitmask_set_all_one(struct mg_bitmask * mask){
  // TODO: check if memset() would be faster for 64bit values...
  // TODO: use mg_bitmask_set_n_one instead...
  uint16_t i;
  for(i=0; i< mask->n_blocks; i++){
    mask->mask[i] = 0xffffffffffffffff;
  }
  // FIXME XXX TODO why do we need this??? can't we just set all blocks to 1s?
  // -> NO: because some algorithms rely on trailing zeroes...
  if(mask->size & 0x3f){
    mask->mask[mask->n_blocks-1] = (0xffffffffffffffff >> (64-(mask->size & 0x3f)));
  }
}

uint8_t mg_bitmask_get_bit(struct mg_bitmask * mask, uint16_t n){
  //uint8_t i = 0;
  //uint64_t submask;
  //uint8_t r= iterate_get(mask, &i, &submask);
  //printf("res %u", r);
  // printf("CCC get bit %d\n", n);
  // printhex("mask = ", mask, 30);
  // printhex("mask = ", mask->mask, 30);
  // uint64_t r1 = mask->mask[n/64] & (1ULL<< (n&0x3f));
  // printhex("r1 = ", &r1, 8);
  //uint64_t a = rte_rdtsc();
  uint8_t result = ( (mask->mask[n/64] & (1ULL<< (n&0x3f))) != 0);
  //uint64_t b = rte_rdtsc();
  //printf("getbit: %lu\n", b-a);
  // printf("result = %d\n", (int)result);
  return result;
}

void mg_bitmask_set_bit(struct mg_bitmask * mask, uint16_t n){
  //printf(" CC set bit nr %d\n", n);
  //printhex("mask = ", mask->mask, 8*3);
  mask->mask[n/64] |= (1ULL<< (n&0x3f));
  //printhex("mask = ", mask->mask, 8*3);
}

void mg_bitmask_clear_bit(struct mg_bitmask * mask, uint16_t n){
  mask->mask[n/64] &= ~(1ULL<< (n&0x3f));
}

void mg_bitmask_and(struct mg_bitmask * mask1, struct mg_bitmask * mask2, struct mg_bitmask * result){
  uint16_t i;
  for(i=0; i< mask1->n_blocks; i++){
    result->mask[i] = mask1->mask[i] & mask2->mask[i];
  }
}

void mg_bitmask_xor(struct mg_bitmask * mask1, struct mg_bitmask * mask2, struct mg_bitmask * result){
  uint16_t i;
  for(i=0; i< mask1->n_blocks; i++){
    result->mask[i] = mask1->mask[i] ^ mask2->mask[i];
  }
}

void mg_bitmask_or(struct mg_bitmask * mask1, struct mg_bitmask * mask2, struct mg_bitmask * result){
  uint16_t i;
  for(i=0; i< mask1->n_blocks; i++){
    result->mask[i] = mask1->mask[i] | mask2->mask[i];
  }
}

// to start iterating: i initialized to 0, submask initialized (value does not matter)
// XXX: this will go horribly wrong, if called for an i>=mask.size
uint8_t mg_bitmask_iterate_get(struct mg_bitmask * mask, uint16_t* i, uint64_t* submask){
  //printf("get start i = %u\n", *i);
  //uint64_t a = rte_rdtsc();
  if(unlikely(*i%64 == 0)){
    *submask = mask->mask[*i/64];
  }
  uint8_t result = ((*submask & 1ULL) != 0ULL);
  *submask = *submask>>1;
  *i = *i +1;
  //printf("get done i = %u\n", *i);
  //uint64_t b = rte_rdtsc();
  //printf("iterat: %lu\n", b-a);
  return result;
}

// to start iterating: i initialized to 0, submask UNinitialized
// XXX: this will go horribly wrong, if called for an i>=mask.size
void mg_bitmask_iterate_set(struct mg_bitmask * mask, uint16_t* i, uint64_t** submask, uint8_t value){
  //printf("set start i = %u\n", *i);
  if(unlikely(((*i)%64) == 0)){
    *submask = &mask->mask[*i/64];
    //printf("mod\n");
    //*submask = 0ULL; // not needed, as we shift through in the end anyways
  }
  //printf("pshift\n");
  **submask = (**submask)>>1;
  //printf("ashift\n");
  if(value){
    //printf("true\n");
    **submask = **submask | 0x8000000000000000UL;
  }
  //printf("msk %lx\n", **submask);
  *i = *i + 1;
  if(unlikely(*i==mask->size)){
    //printf("end\n");
    **submask = **submask>>(*i%64);
    //printf("msk %lx\n", **submask);
  }
  //printf("set done i = %u\n", *i);
}

// FIXME: trailing zeroes are not handled correctly here....
void mg_bitmask_not(struct mg_bitmask * mask1, struct mg_bitmask * result){
  uint16_t i;
  for(i=0; i< mask1->n_blocks; i++){
    result->mask[i] = ~(mask1->mask[i]);
  }
}
