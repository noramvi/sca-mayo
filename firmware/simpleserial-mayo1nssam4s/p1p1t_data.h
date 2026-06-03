#ifndef P1P1T_DATA_H
#define P1P1T_DATA_H

#include <stdint.h>

#define P1P1T_M_VEC_LIMBS 5
#define P1P1T_SUBSET_ENTRIES 77
#define P1P1T_P1_SIZE (P1P1T_SUBSET_ENTRIES * P1P1T_M_VEC_LIMBS)

#define P1P1T_O_SIZE 624 

extern const uint64_t p1p1t_P1_subset[P1P1T_P1_SIZE];
extern const uint8_t p1p1t_O[P1P1T_O_SIZE];

#endif