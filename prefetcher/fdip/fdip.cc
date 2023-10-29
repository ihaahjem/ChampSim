// This file is very inspired by the FDIP from Roman(jogli5er): https://github.com/TPF-for-WSCA/ChampSim/tree/BTB_FTQ_AUGMENTATION/prefetcher/fdip

#include <iostream>

#include "cache.h"
#include "ooo_cpu.h"

// #define MAX_PQ_ENTRIES 64 // Same length as IFETCH_BUFFER. TODO: Find ideal length and arguments for it

// std::deque<uint64_t> PTQ;

void O3_CPU::prefetcher_initialize() { 
  #define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
  std::cout << L1I->NAME << " FDIP" << std::endl; }


void O3_CPU::prefetcher_branch_operate(uint64_t instr_ip, uint8_t branch_type, uint64_t branch_target) {
  
  // Get block address of instruction from branch predictor
  // uint64_t block_address = ((instr_ip >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);

  // // Add block to the prefetch queue if it is not already there
  // std::deque<uint64_t>::iterator it = std::find(PTQ.begin(), PTQ.end(), block_address);
  // if (it == PTQ.end()) {
  //     PTQ.push_back(block_address);
  // }
  // if(PTQ.size() >= MAX_PQ_ENTRIES){
  //   PTQ.pop_front();
  // }

}

uint32_t O3_CPU::prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit, uint32_t metadata_in)
{
  // Not sure if anything should be done in this function for my implementation
}

uint32_t O3_CPU::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void O3_CPU::prefetcher_cycle_operate() {
  // Perform prefetching of what is in the prefetch queue every cycle.
  if (PTQ.size()) {
    // Check if it is recently prefetched
    std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), PTQ.front());
    if(it == recently_prefetched.end()){

      #define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
      // Check if it is already in L1I
      uint64_t num_tag_bits = 64 - lg2(L1I->NUM_SET) - LOG2_BLOCK_SIZE;
      uint64_t tag = PTQ.front() >> (lg2(L1I->NUM_SET) + LOG2_BLOCK_SIZE);
      uint64_t index = (PTQ.front() << num_tag_bits) >> (num_tag_bits + LOG2_BLOCK_SIZE);
      uint64_t num_ways = L1I->NUM_WAY;

      // Loop through the way and compare the tag
      bool in_cache = false;
      for(auto it = L1I->block.begin() + index; it != (L1I->block.begin() + index + L1I->NUM_WAY); ++it){
        if(it->tag == tag){
          in_cache = true;
        }
      }

      if(!in_cache){
          prefetch_code_line(PTQ.front());
          recently_prefetched.push_back(PTQ.front());
          PTQ.pop_front();
      }

      if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
        recently_prefetched.pop_front();
      }
    }
 

  }
}

void O3_CPU::prefetcher_final_stats() {}

