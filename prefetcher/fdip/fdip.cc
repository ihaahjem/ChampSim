// This file is very inspired by the FDIP from Roman(jogli5er): https://github.com/TPF-for-WSCA/ChampSim/tree/BTB_FTQ_AUGMENTATION/prefetcher/fdip

#include <iostream>

#include "cache.h"
#include "ooo_cpu.h"


void O3_CPU::prefetcher_initialize() { 
  #define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
  std::cout << L1I->NAME << " FDIP" << std::endl; }


void O3_CPU::prefetcher_branch_operate(uint64_t instr_ip, uint8_t branch_type, uint64_t branch_target) {
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
  // Perform prefetching of what is in the PTQ
  if (PTQ.empty()){
    return;
  } 

  #define L1I (static_cast<CACHE*>(L1I_bus.lower_level))

  if (L1I->get_occupancy(0, 0) < L1I->MSHR_SIZE >> 1) { // Make sure the MSHRs can handle it so prefetching does not affect demands
    bool prefetched = false;
    auto& [block_address, added_during_fetch_stall] = PTQ.at(ptq_prefetch_entry);
    // Check if it is recently prefetched
    std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), block_address);
    if(it == recently_prefetched.end()){
      // Check if cache block already in L1I
      uint32_t set = L1I->get_set(block_address);
      uint32_t way = L1I->get_way(block_address,set);
      if(way == L1I->NUM_WAY){
        // Prefetch
        prefetched = L1I->prefetch_line(block_address,true, 0, added_during_fetch_stall, conditional_bm, fetch_stall_prf_number); //Three last inputs only added to collect stats
      
        // If the prefetch was issued successfully (VAPQ not full), add to recently prefetched queue
        if(prefetched){
          recently_prefetched.push_back(block_address);
          if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
            recently_prefetched.pop_front();
          }
          collect_prefetch_stats(added_during_fetch_stall);
        }
      }else{
        // If found in L1I, mark prefetched as true so that we go to the next ptq_prefetch_entry
        prefetched = true;
      }
      }
    // If prefetched was successful (Either found in recently prefetched, or successful prefetch),
    // move the counter pointing to the entry to be prefetched
    if(prefetched || it != recently_prefetched.end()){
      if(ptq_prefetch_entry < PTQ.size() - 1){
        ptq_prefetch_entry++;
      }
    }
  }
}

void O3_CPU::prefetcher_final_stats() {}



// Stat functions
void O3_CPU::collect_prefetch_stats(bool added_during_fetch_stall) {
  if(added_during_fetch_stall){
    //Increment number of wrong path instructions prefetched
    num_prefetched_wrong_path++; // How many prefetches during fetch_stall in total
    if(conditional_bm){
      num_prefetched_wrong_path_contitional++; // How many prefetches during fetch_stall were due to conditional branch
    }
    fetch_stall_prf_number++; // Used to figure out how many prefetches were performed during each fetch_stall

  }else{
    if(fetch_stall_prf_number){
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
    }
    fetch_stall_prf_number = 0;
  }
}