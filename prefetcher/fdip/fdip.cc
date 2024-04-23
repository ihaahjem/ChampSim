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

  if (ptq_prefetch_entry < PTQ.size()) { // There are entries in the PTQ that are not prefetched
    bool prefetched = false;
    auto& [block_address, added_during_fetch_stall, assumed_prefetched] = PTQ.at(ptq_prefetch_entry);
    // Check if it is recently prefetched
    std::deque<uint64_t>::iterator it = std::find(recently_prefetched.begin(), recently_prefetched.end(), block_address);
    if(it == recently_prefetched.end()){
      // Check if cache block already in L1I
      uint32_t set = l1i->get_set(block_address);
      uint32_t way = l1i->get_way(block_address,set);
      if(way < l1i->NUM_WAY){
        // TODO: Figure out why this never happens. I would expect it to be found in L1I very often.

        // If found in L1I, mark prefetched as true so that we go to the next ptq_prefetch_entry
        prefetched = true;
        times_found_in_l1i++;
      }else{
        // Prefetch
        prefetched = l1i->prefetch_line(block_address,true, 0, added_during_fetch_stall, conditional_bm, prf_wp.fetch_stall_prf_number, assumed_prefetched); //Three last inputs only added to collect stats
      
        // If the prefetch was issued successfully (VAPQ not full), add to recently prefetched queue
        if(prefetched){
          recently_prefetched.push_back(block_address);
          if(recently_prefetched.size() >= MAX_RECENTLY_PREFETCHED_ENTRIES){
            recently_prefetched.pop_front();
          }
          // STATS
          prf_wp.collect_prefetch_stats(added_during_fetch_stall, conditional_bm);
          if(assumed_prefetched){
            prefetches_assumed_prefetched++;
          }
        }
        times_not_found_in_l1i++;
      }
    }
    // If prefetched was successful (Either found in recently prefetched, or successful prefetch),
    // increment ptq_prefetch_entry
    if(prefetched || it != recently_prefetched.end()){
        ptq_prefetch_entry++;
    }
  }
}

void O3_CPU::prefetcher_final_stats() {}



// STAT functions:
void O3_CPU::prefetched_wp::collect_prefetch_stats(bool added_during_fetch_stall, bool conditional_bm) {
  if(added_during_fetch_stall){
    //Increment number of wrong path instructions prefetched
    num_prefetched_wrong_path++; // How many prefetches during fetch_stall in total
    if(conditional_bm){
      num_prefetched_wrong_path_conditional++; // How many prefetches during fetch_stall were due to conditional branch
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

uint64_t O3_CPU::cb_to_ptq_wp::collect_cb_added__stats(){
    if (num_cb_to_PTQ_fetch_stall == 0) {
        num_cb_0++;
    } else if (num_cb_to_PTQ_fetch_stall == 1) {
        num_cb_1++;
    } else if (num_cb_to_PTQ_fetch_stall == 2) {
        num_cb_2++;
    } else if (num_cb_to_PTQ_fetch_stall == 3) {
        num_cb_3++;
    } else if (num_cb_to_PTQ_fetch_stall == 4) {
        num_cb_4++;
    } else if (num_cb_to_PTQ_fetch_stall < 11) {
        num_cb_6_10++;
    } else if (num_cb_to_PTQ_fetch_stall < 16) {
        num_cb_11_15++;
    } else if (num_cb_to_PTQ_fetch_stall < 21) {
        num_cb_16_20++;
    } else if (num_cb_to_PTQ_fetch_stall < 26) {
        num_cb_21_25++;
    } else {
        num_cb_26_128++;
    }
  uint64_t cb_until_time_start = num_cb_to_PTQ_fetch_stall;
  num_cb_to_PTQ_fetch_stall = 0;
  return cb_until_time_start;
}

 void O3_CPU::cb_to_ptq_wp::printStatistics_cb_added() {
        uint64_t total_cb = num_cb_0 + num_cb_1 + num_cb_2 + num_cb_3 + num_cb_4 + 
                            num_cb_6_10 + num_cb_11_15 + num_cb_16_20 + num_cb_21_25 + num_cb_26_128;
        std::cout << "Percentage cb num_0: " << static_cast<double>(num_cb_0) / total_cb << std::endl;
        std::cout << "Percentage cb num_1: " << static_cast<double>(num_cb_1) / total_cb << std::endl;
        std::cout << "Percentage cb num_2: " << static_cast<double>(num_cb_2) / total_cb << std::endl;
        std::cout << "Percentage cb num_3: " << static_cast<double>(num_cb_3) / total_cb << std::endl;
        std::cout << "Percentage cb num_4: " << static_cast<double>(num_cb_4) / total_cb << std::endl;
        std::cout << "Percentage cb num_6_10: " << static_cast<double>(num_cb_6_10) / total_cb << std::endl;
        std::cout << "Percentage cb num_11_15: " << static_cast<double>(num_cb_11_15) / total_cb << std::endl;
        std::cout << "Percentage cb num_16_20: " << static_cast<double>(num_cb_16_20) / total_cb << std::endl;
        std::cout << "Percentage cb num_21_25: " << static_cast<double>(num_cb_21_25) / total_cb << std::endl;
        std::cout << "Percentage cb num_26_128: " << static_cast<double>(num_cb_26_128) / total_cb << std::endl;
}

void O3_CPU::addr_to_ptq_wp::collect_addr_added_stats(){
    // Get stats for number of addresses accessed during fetch stall
    if (num_addr_to_PTQ_fetch_stall < 40) {
        num_addr_0_39++;
    } else if (num_addr_to_PTQ_fetch_stall < 80) {
        num_addr_40_79++;
    } else if (num_addr_to_PTQ_fetch_stall < 120) {
        num_addr_80_119++;
    } else if (num_addr_to_PTQ_fetch_stall < 160) {
        num_addr_120_159++;
    } else if (num_addr_to_PTQ_fetch_stall < 200) {
        num_addr_160_199++;
    } else {
        num_addr_above++;
    }
    num_addr_to_PTQ_fetch_stall = 0;
}

 void O3_CPU::addr_to_ptq_wp::printStatistics_addr_added() {
   uint64_t tot_addr = num_addr_0_39 + num_addr_40_79 + num_addr_80_119 + num_addr_120_159 + num_addr_160_199 + num_addr_6_11 + num_addr_40_792_17 + num_addr_40_798_23 + num_addr_80_1194_29 + num_addr_above;
        cout << " Percentage addr num_0 " << (0.0 + num_addr_0_39) / tot_addr << endl;
        cout << " Percentage addr num_1 " << (0.0 + num_addr_40_79) / tot_addr << endl;
        cout << " Percentage addr num_2 " << (0.0 + num_addr_80_119) / tot_addr << endl;
        cout << " Percentage addr num_3 " << (0.0 + num_addr_120_159) / tot_addr << endl;
        cout << " Percentage addr num_4 " << (0.0 + num_addr_160_199) / tot_addr << endl;
        cout << " Percentage addr num_6_11 " << (0.0 + num_addr_6_11) / tot_addr << endl;
        cout << " Percentage addr num_12_17 " << (0.0 + num_addr_40_792_17) / tot_addr << endl;
        cout << " Percentage addr num_18_23 " << (0.0 + num_addr_40_798_23) / tot_addr << endl;
        cout << " Percentage addr num_24_29 " << (0.0 + num_addr_80_1194_29) / tot_addr << endl;
        cout << " Percentage addr num_above " << (0.0 + num_addr_above) / tot_addr << endl;
}

void O3_CPU::CycleCounter::count_cycles_until_fetched(){
  if(!index_start_count && start_counting_cycles){
    start_counting_cycles = false;
    if (cycles_fetch_first_cb_after_prf == 0) {
        cycles_0++;
    } else if (cycles_fetch_first_cb_after_prf == 1) {
        cycles_1++;
    } else if (cycles_fetch_first_cb_after_prf == 2) {
        cycles_2++;
    } else if (cycles_fetch_first_cb_after_prf == 3) {
        cycles_3++;
    } else if (cycles_fetch_first_cb_after_prf == 4) {
        cycles_4++;
    } else if (cycles_fetch_first_cb_after_prf < 12) {
        cycles_6_11++;
    } else if (cycles_fetch_first_cb_after_prf < 18) {
        cycles_12_17++;
    } else if (cycles_fetch_first_cb_after_prf < 24) {
        cycles_18_23++;
    } else if (cycles_fetch_first_cb_after_prf < 30) {
        cycles_24_29++;
    } else {
        cycles_above++;
    }
    cycles_fetch_first_cb_after_prf=0;
  }
}

 void O3_CPU::CycleCounter::printStatistics_cycles_until_fetched() {
    uint64_t tot_cycles = cycles_0 + cycles_1 + cycles_2 + cycles_3 + cycles_4 + cycles_6_11 + cycles_12_17 + cycles_18_23 + cycles_24_29 + cycles_above;
    if (tot_cycles) {
        cout << " cycles 0 " << (0.0 + cycles_0) / tot_cycles << endl;
        cout << " cycles 1 " << (0.0 + cycles_1) / tot_cycles << endl;
        cout << " cycles 2 " << (0.0 + cycles_2) / tot_cycles << endl;
        cout << " cycles 3 " << (0.0 + cycles_3) / tot_cycles << endl;
        cout << " cycles 4 " << (0.0 + cycles_4) / tot_cycles << endl;
        cout << " cycles 6_11 " << (0.0 +  cycles_6_11) / tot_cycles << endl;
        cout << " cycles 12_17 " << (0.0 + cycles_12_17) / tot_cycles << endl;
        cout << " cycles 18_23 " << (0.0 + cycles_18_23) / tot_cycles << endl;
        cout << " cycles 24_29 " << (0.0 + cycles_24_29) / tot_cycles << endl;
        cout << " above 29 " << (0.0 + cycles_above) / tot_cycles << endl;
    }
}

void O3_CPU::compare_wp_rp::compare_wp_rp_entries(){
  
  total_entries_to_compare += FTQ_after_fetch_stall.size(); // How many are equal. Does not consider order
  uint64_t num_equal_entries = 0; // How many are equal. Does not consider order
  bool correct_order = true; // Are the equal entries in same order?

  // Find number of equal entries:
  auto prev_equal_ptq = PTQ_during_fetch_stall.begin();
  auto prev_equal_ftq = FTQ_after_fetch_stall.begin();
  for (auto it = FTQ_after_fetch_stall.begin(); it != FTQ_after_fetch_stall.end(); ++it){
    auto it_ptq = std::find(PTQ_during_fetch_stall.begin(), PTQ_during_fetch_stall.end(), *it);
    if(it_ptq != PTQ_during_fetch_stall.end()){
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