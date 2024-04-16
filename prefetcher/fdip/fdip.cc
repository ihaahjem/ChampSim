// This file is very inspired by the FDIP from Roman(jogli5er): https://github.com/TPF-for-WSCA/ChampSim/tree/BTB_FTQ_AUGMENTATION/prefetcher/fdip

#include <iostream>

#include "cache.h"
#include "ooo_cpu.h"


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
  return metadata_in;
}

uint32_t O3_CPU::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void O3_CPU::prefetcher_cycle_operate() {
  #define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
  // Perform prefetching of what is in the prefetch queue every cycle as long as there is room in VAPQ
  if (PTQ.size() && !L1I->VAPQ.full()) {
    std::pair<uint64_t, bool> element = PTQ.at(ptq_prefetch_entry);
    // // Check if it is recently prefetched
    std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), element.first);
    if(it == recently_prefetched.end()){

      // Prefetch if it is not already in L1I
      uint32_t set = L1I->get_set(element.first);
      uint32_t way = L1I->get_way(element.first,set);
      if(way == L1I->NUM_WAY){
        if(element.second){
          // Marker som fetch_stall
          L1I->prefetch_line(element.first,true, 0, true, conditional_bm, fetch_stall_prf_number);
          //Increment number of wrong path instructions prefetched
          num_prefetched_wrong_path++;
          if(conditional_bm){
            num_prefetched_wrong_path_contitional++;
          }

          // TODO: increment the number of prfetched in that ratio
          if(fetch_stall_prf_number < 6){
            FS_prf_0_6++;
          }else if (fetch_stall_prf_number < 12){
            FS_prf_6_11++;
          }else if (fetch_stall_prf_number < 18){
            FS_prf_12_17++;
          }else if (fetch_stall_prf_number < 23){
            FS_prf_18_23++;
          }else if (fetch_stall_prf_number < 30){
            FS_prf_24_29++;
          }else if (fetch_stall_prf_number < 36){
            FS_prf_30_35++;
          }else{
            FS_prf_above++;
          }  

          fetch_stall_prf_number++;

        }else{
          // Marker som ikke fetch_stall
          L1I->prefetch_line(element.first,true, 0, false, false, 0);

          fetch_stall_prf_number = 0;
        }
        recently_prefetched.push_back(element.first);
        if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
          recently_prefetched.pop_front();
        }        
      }
    }
    
    if(ptq_prefetch_entry < PTQ.size() - 1){
      ptq_prefetch_entry++;
    }

    if(index_first_spec > 0){
      index_first_spec--;
    }
  }
}

void O3_CPU::prefetcher_final_stats() {}

