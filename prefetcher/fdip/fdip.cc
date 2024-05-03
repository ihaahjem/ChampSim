// This file is very inspired by the FDIP from Roman(jogli5er): https://github.com/TPF-for-WSCA/ChampSim/tree/BTB_FTQ_AUGMENTATION/prefetcher/fdip

#include <iostream>

#include "cache.h"
#include "ooo_cpu.h"


void O3_CPU::prefetcher_initialize() { 
  l1i = static_cast<CACHE*>(L1I_bus.lower_level);
  std::cout << l1i->NAME << " FDIP" << std::endl; }


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
  uint64_t b_address = 0;
  if ((l1i->get_occupancy(0, 0) < l1i->get_size(0,0) >> 1) && ptq_prefetch_entry < PTQ.size()) { 
      bool prefetched = false;
      auto& [block_address, added_during_speculation, fetch_stall] = PTQ.at(ptq_prefetch_entry);
      b_address = block_address;
      // Check if it is recently prefetched
      std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), block_address);
      if(it == recently_prefetched.end()){
        // Check if cache block already in L1I
        if(l1i->hit_test(block_address)){
          // If found in L1I, mark prefetched as true so that we go to the next ptq_prefetch_entry
          prefetched = true;

        }else{
          // Prefetch
          prefetched = l1i->prefetch_line(block_address,true, 0, added_during_speculation, conditional_bm, fetch_stall_prf_number, fetch_stall); //Three last inputs only added to collect stats

          // If the prefetch was issued successfully (VAPQ not full), add to recently prefetched queue
          if(prefetched){
            recently_prefetched.push_back(block_address);
            if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
              recently_prefetched.pop_front();
            }
            if(added_during_speculation && !fetch_stall){
              prefetched_spec_after_fetch_stall++;
            }
            collect_prefetch_stats(added_during_speculation);
          }
        }
      }
      // If prefetched was successful (Either found in recently prefetched, or successful prefetch),
      // move the counter pointing to the entry to be prefetched
      if(prefetched || it != recently_prefetched.end()){
        ptq_prefetch_entry++;
      }
    }

    // Prefetch only correct path
    // prefetcher_only_correct_path(b_address);
}

void O3_CPU::prefetcher_final_stats() {}



// PTQ correct path
void O3_CPU::prefetcher_only_correct_path(uint64_t PTQ_block_address) {
  // Perform prefetching of what is in the PTQ
  if (PTQ_only_correct.empty() || (ptq_prefetch_entry_cp < PTQ_only_correct.size() && PTQ_block_address == PTQ_only_correct.at(ptq_prefetch_entry_cp))){
    return;
  } 

  if ((l1i->get_occupancy(0, 0) < l1i->get_size(0,0) >> 1) && ptq_prefetch_entry_cp < PTQ_only_correct.size()) { 
      bool prefetched = false;
      uint64_t block_address = PTQ_only_correct.at(ptq_prefetch_entry_cp);
      // Check if it is recently prefetched
      std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), block_address);
      if(it == recently_prefetched.end()){
        // Check if cache block already in L1I
        if(l1i->hit_test(block_address)){
          // If found in L1I, mark prefetched as true so that we go to the next ptq_prefetch_entry_cp
          prefetched = true;

        }else{
          // Prefetch
          prefetched = l1i->prefetch_line(block_address,true, 0, false, false, 0, false); //Three last inputs only added to collect stats

          // If the prefetch was issued successfully (VAPQ not full), add to recently prefetched queue
          if(prefetched){
            recently_prefetched.push_back(block_address);
            if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
              recently_prefetched.pop_front();
            }
          }
        }
      }
      // If prefetched was successful (Either found in recently prefetched, or successful prefetch),
      // move the counter pointing to the entry to be prefetched
      if(prefetched || it != recently_prefetched.end()){
        ptq_prefetch_entry_cp++;
      }
    }
}

// STAT functions
void O3_CPU::collect_prefetch_stats(bool added_during_speculation) {
  if(added_during_speculation){
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

void O3_CPU::compare_wp_rp::compare_wp_rp_entries(){
  
  total_entries_to_compare +=  FTQ_when_ptq_wp.size(); 
  uint64_t num_equal_entries = 0; // How many are equal. Does not consider order
  bool correct_order = true; // Are the equal entries in same order?

  // Find number of equal entries:
  auto prev_equal_ptq = PTQ_wp.begin();
  auto prev_equal_ftq = FTQ_when_ptq_wp.begin();
  for (auto it = FTQ_when_ptq_wp.begin(); it != FTQ_when_ptq_wp.end(); ++it){
    auto it_ptq = std::find(PTQ_wp.begin(), PTQ_wp.end(), *it);
    if(it_ptq != PTQ_wp.end()){
      if(num_equal_entries > 0){
        if(!(it_ptq == prev_equal_ptq + 1 && it == prev_equal_ftq + 1 )){
          correct_order = false;
        }
      }
      num_equal_entries++;
      prev_equal_ptq = it_ptq;
      prev_equal_ftq = it;
    }
  }
  total_equal_entries += num_equal_entries;
  if(num_equal_entries > 0 && correct_order){
    num_queues_same_order++;
  }
  total_comparisons++;
}

 void O3_CPU::compare_wp_rp::compare_wp_rp_entries_print_results() {
  cout << " Total entries compared " << total_entries_to_compare << endl;
  cout << " Total queues compared " << total_comparisons << endl;
  cout << " Total equal entries " << total_equal_entries << endl;
  cout << " Number of queues with same order " << num_queues_same_order << endl;
}