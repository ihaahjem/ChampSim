#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

uint8_t warmup_complete[NUM_CPUS] = {}, simulation_complete[NUM_CPUS] = {}, all_warmup_complete = 0, all_simulation_complete = 0,
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS, knob_cloudsuite = 0, knob_low_bandwidth = 0;

uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

auto start_time = time(NULL);

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

std::vector<tracereader*> traces;

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
  }
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " TRANSLATION ACCESS: " << setw(10) << cache->roi_access[cpu][4] << "  HIT: " << setw(10) << cache->roi_hit[cpu][4] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][4] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << endl;


    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency)) / TOTAL_MISS << " cycles" << endl;
            
    
    // if(cache->NAME == "cpu0_L1I"){
      cout << "  REQUESTED DURING SPECULATION: " << setw(10) << cache->requested_wp << "  ISSUED: " << setw(10) << cache->issued_wp << " NOT ACTUALLY ISSUED: " << setw(10) << cache->not_issued_wp << "  USEFUL: " << setw(10) << cache->num_prefetched_useful_wrong_path << "  USELESS: " << setw(10) << cache->num_prefetched_useless_wrong_path << "  ACCURACY: " << setw(10) << (0.0+cache->num_prefetched_useful_wrong_path)/(cache->num_prefetched_useful_wrong_path + cache->num_prefetched_useless_wrong_path) << endl;
    //   cout << "  USEFUL DURING FETCH_STALL CONDITIONAL BM: " << setw(10) << cache->num_prefetched_useful_wrong_path_conditional << "  USELESS: " << setw(10) << cache->num_prefetched_useless_wrong_path_conditional << endl;

    //   // Misses counters during fetch stall
    //   cout << "Misses 0-5: "   << cache->misses_0_5 << endl;
    //   cout << "Misses 6-11: "  << cache->misses_6_11 << endl;
    //   cout << "Misses 12-17: " << cache->misses_12_17 << endl;
    //   cout << "Misses 18-23: " << cache->misses_18_23 << endl;
    //   cout << "Misses 24-29: " << cache->misses_24_29 << endl;
    //   cout << "Misses 30-35: " << cache->misses_30_35 << endl;
    //   cout << "Misses above: " << cache->misses_above << endl;

    //   cout << "Useful 0-5: "   << cache->useful_0_5 << endl;
    //   cout << "Useful 6-11: "  << cache->useful_6_11 << endl;
    //   cout << "Useful 12-17: " << cache->useful_12_17 << endl;
    //   cout << "Useful 18-23: " << cache->useful_18_23 << endl;
    //   cout << "Useful 24-29: " << cache->useful_24_29 << endl;
    //   cout << "Useful 30-35: " << cache->useful_30_35 << endl;
    //   cout << "Useful above: " << cache->useful_above << endl;

    //   cout << "useless 0-5: "   << cache->useless_0_5 << endl;
    //   cout << "useless 6-11: "  << cache->useless_6_11 << endl;
    //   cout << "useless 12-17: " << cache->useless_12_17 << endl;
    //   cout << "useless 18-23: " << cache->useless_18_23 << endl;
    //   cout << "useless 24-29: " << cache->useless_24_29 << endl;
    //   cout << "useless 30-35: " << cache->useless_30_35 << endl;
    //   cout << "useless above: " << cache->useless_above << endl;

    //   cout << "accuracy 0-5: "   << (0.0 + cache->useful_0_5) / (cache->useful_0_5 + cache->useless_0_5) << endl;
    //   cout << "accuracy 6-11: "  << (0.0 + cache->useful_6_11) / (cache->useful_6_11 + cache->useless_6_11)  << endl;
    //   cout << "accuracy 12-17: " << (0.0 + cache->useful_12_17) / (cache->useful_12_17 + cache->useless_12_17) << endl;
    //   cout << "accuracy 18-23: " << (0.0 + cache->useful_18_23) / (cache->useful_18_23 + cache->useless_18_23) << endl;
    //   cout << "accuracy 24-29: " << (0.0 + cache->useful_24_29) / (cache->useful_24_29 + cache->useless_24_29) << endl;
    //   cout << "accuracy 30-35: " << (0.0 + cache->useful_30_35) / (cache->useful_30_35 + cache->useless_30_35)  << endl;
    //   cout << "accuracy above: " << (0.0 + cache->useful_above) / (cache->useful_above + cache->useless_above)  << endl;
    // }



    // cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< endl;
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][3] << endl;
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
    cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions);
    cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
         << endl;

    /*
    cout << "Branch types" << endl;
    cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_JUMP: "
    << ooo_cpu[i]->total_branch_types[1] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT: " <<
    ooo_cpu[i]->total_branch_types[2] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_CONDITIONAL: "
    << ooo_cpu[i]->total_branch_types[3] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_CALL: "
    << ooo_cpu[i]->total_branch_types[4] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT_CALL:
    " << ooo_cpu[i]->total_branch_types[5] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_RETURN: " <<
    ooo_cpu[i]->total_branch_types[6] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_OTHER: " <<
    ooo_cpu[i]->total_branch_types[7] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
    */

    cout << "Branch type MPKI" << endl;
    cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
  }
}

void print_dram_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  std::cout << std::endl;
  std::cout << "DRAM Statistics" << std::endl;
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    std::cout << " CHANNEL " << i << std::endl;

    auto& channel = DRAM.channels[i];
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (DRAM_CHANNELS > 1) {
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
  }
}

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
  }

  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_fill = 0;

  cache->total_miss_latency = 0;

  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

  cout << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++) {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
  cout << endl;

  // reset DRAM stats
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
}

void signal_handler(int signal)
{
  cout << "Caught signal: " << signal << endl;
  exit(1);
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

  // initialize knobs
  uint8_t show_heartbeat = 1;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
                                         {"traces", no_argument, &traces_encountered, 1},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hc", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      show_heartbeat = 0;
      break;
    case 'c':
      knob_cloudsuite = 1;
      MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
      break;
    case 0:
      break;
    default:
      abort();
    }
  }

  cout << "Warmup Instructions: " << warmup_instructions << endl;
  cout << "Simulation Instructions: " << simulation_instructions << endl;
  cout << "Number of CPUs: " << NUM_CPUS << endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  for (int i = optind; i < argc; i++) {
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    assert(0);
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu) {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>())) {

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables) {
      try {
        op->_operate();
      } catch (champsim::deadlock& dl) {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables) {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i) {
      // read from trace
      while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0) {
        ooo_cpu[i]->init_instruction(traces[i]->get());
      }

      // Fill PTQ speculatively
      while (ooo_cpu[i]->speculate && ooo_cpu[i]->fetch_stall == 1 && ooo_cpu[i]->instrs_to_speculate_this_cycle > 0 && ooo_cpu[i]->num_entries_in_ftq < ooo_cpu[i]->IFETCH_BUFFER.size()){
        ooo_cpu[i]->fill_ptq_speculatively();
        ooo_cpu[i]->num_instr_fetch_stall++;
        ooo_cpu[i]->num_entries_in_ftq++;
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
        float cumulative_ipc;
        if (warmup_complete[i])
          cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        else
          cumulative_ipc = (1.0 * ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions)) {
        warmup_complete[i] = 1;
        all_warmup_complete++;
      }
      if (all_warmup_complete == NUM_CPUS) { // this part is called only once
                                             // when all cores are warmed up
        all_warmup_complete++;
        finish_warmup();
      }

      // simulation complete
      if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0)
          && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions))) {
        simulation_complete[i] = 1;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

        cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        cout << " Number of PTQ flushes " << ooo_cpu[i]->num_ptq_flushed << endl;
        cout << " Number of FTQ flushes " << ooo_cpu[i]->num_ftq_flush << endl;
        // cout << " Number of FTQ flushes due to conditional " << ooo_cpu[i]->num_ftq_flush_conditional << endl;
        // cout << " Number of FTQ flushes due to call/return " << ooo_cpu[i]->num_ftq_flush_call_return << endl;
        // cout << " Number of FTQ flushes due to other " << ooo_cpu[i]->num_ftq_flush_other << endl;
        // cout << " AVG number of entries in FTQ when flush " << ooo_cpu[i]->num_entries_in_ftq_when_flush/ooo_cpu[i]->num_ftq_flush << endl;
        cout << " Number of block_addresses prefetched during speculation " << ooo_cpu[i]->num_prefetched_wrong_path << endl;
        // cout << " Number of block_addresses prefetched during fetch_stall conditional " << ooo_cpu[i]->num_prefetched_wrong_path_contitional << endl;
        // cout << " Number cycles spent in fetch_stall " << ooo_cpu[i]->num_cycles_fetch_stall << endl;
        // cout << " AVG number of cache blocks added to PTQ during fetch stall " << ooo_cpu[i]->num_cb_to_PTQ_fetch_stall/ooo_cpu[i]->num_ftq_flush << endl;
        
        // uint64_t tot_cb = ooo_cpu[i]->num_cb_0 + ooo_cpu[i]->num_cb_1 + ooo_cpu[i]->num_cb_2 + ooo_cpu[i]->num_cb_3 + ooo_cpu[i]->num_cb_4 + ooo_cpu[i]->num_cb_6_10 + ooo_cpu[i]->num_cb_11_15 + ooo_cpu[i]->num_cb_16_20 + ooo_cpu[i]->num_cb_21_25 + ooo_cpu[i]->num_cb_26_128;
        // cout << " Percentage cb num_0 " << (0.0 + ooo_cpu[i]->num_cb_0) / tot_cb << endl;
        // cout << " Percentage cb num_1 " << (0.0 + ooo_cpu[i]->num_cb_1) / tot_cb << endl;
        // cout << " Percentage cb num_2 " << (0.0 + ooo_cpu[i]->num_cb_2) / tot_cb << endl;
        // cout << " Percentage cb num_3 " << (0.0 + ooo_cpu[i]->num_cb_3) / tot_cb << endl;
        // cout << " Percentage cb num_4 " << (0.0 + ooo_cpu[i]->num_cb_4) / tot_cb << endl;
        // cout << " Percentage cb num_6_10 " << (0.0 + ooo_cpu[i]->num_cb_6_10) / tot_cb << endl;
        // cout << " Percentage cb num_11_15 " << (0.0 + ooo_cpu[i]->num_cb_11_15) / tot_cb << endl;
        // cout << " Percentage cb num_16_20 " << (0.0 + ooo_cpu[i]->num_cb_16_20) / tot_cb << endl;
        // cout << " Percentage cb num_21_25 " << (0.0 + ooo_cpu[i]->num_cb_21_25) / tot_cb << endl;
        // cout << " Percentage cb num_26_128 " << (0.0 + ooo_cpu[i]->num_cb_26_128) / tot_cb << endl;

        // uint64_t tot_addr = ooo_cpu[i]->num_addr_0_39 + ooo_cpu[i]->num_addr_40_79 + ooo_cpu[i]->num_addr_80_119 + ooo_cpu[i]->num_addr_120_159 + ooo_cpu[i]->num_addr_160_199 + ooo_cpu[i]->num_addr_6_11 + ooo_cpu[i]->num_addr_40_792_17 + ooo_cpu[i]->num_addr_40_798_23 + ooo_cpu[i]->num_addr_80_1194_29 + ooo_cpu[i]->num_addr_above;
        // cout << " Percentage addr num_0 " << (0.0 + ooo_cpu[i]->num_addr_0_39) / tot_addr << endl;
        // cout << " Percentage addr num_1 " << (0.0 + ooo_cpu[i]->num_addr_40_79) / tot_addr << endl;
        // cout << " Percentage addr num_2 " << (0.0 + ooo_cpu[i]->num_addr_80_119) / tot_addr << endl;
        // cout << " Percentage addr num_3 " << (0.0 + ooo_cpu[i]->num_addr_120_159) / tot_addr << endl;
        // cout << " Percentage addr num_4 " << (0.0 + ooo_cpu[i]->num_addr_160_199) / tot_addr << endl;
        // cout << " Percentage addr num_6_11 " << (0.0 + ooo_cpu[i]->num_addr_6_11) / tot_addr << endl;
        // cout << " Percentage addr num_12_17 " << (0.0 + ooo_cpu[i]->num_addr_40_792_17) / tot_addr << endl;
        // cout << " Percentage addr num_18_23 " << (0.0 + ooo_cpu[i]->num_addr_40_798_23) / tot_addr << endl;
        // cout << " Percentage addr num_24_29 " << (0.0 + ooo_cpu[i]->num_addr_80_1194_29) / tot_addr << endl;
        // cout << " Percentage addr num_above " << (0.0 + ooo_cpu[i]->num_addr_above) / tot_addr << endl;

        // uint64_t tot_cycles = ooo_cpu[i]->cycles_0 + ooo_cpu[i]->cycles_1 + ooo_cpu[i]->cycles_2 + ooo_cpu[i]->cycles_3 + ooo_cpu[i]->cycles_4 + ooo_cpu[i]->cycles_6_11 + ooo_cpu[i]->cycles_12_17 + ooo_cpu[i]->cycles_18_23 + ooo_cpu[i]->cycles_24_29 + ooo_cpu[i]->cycles_above;
        // if (tot_cycles) {
        //     cout << " cycles 0 " << (0.0 + ooo_cpu[i]->cycles_0) / tot_cycles << endl;
        //     cout << " cycles 1 " << (0.0 + ooo_cpu[i]->cycles_1) / tot_cycles << endl;
        //     cout << " cycles 2 " << (0.0 + ooo_cpu[i]->cycles_2) / tot_cycles << endl;
        //     cout << " cycles 3 " << (0.0 + ooo_cpu[i]->cycles_3) / tot_cycles << endl;
        //     cout << " cycles 4 " << (0.0 + ooo_cpu[i]->cycles_4) / tot_cycles << endl;
        //     cout << " cycles 6_11 " << (0.0 +  ooo_cpu[i]->cycles_6_11) / tot_cycles << endl;
        //     cout << " cycles 12_17 " << (0.0 + ooo_cpu[i]->cycles_12_17) / tot_cycles << endl;
        //     cout << " cycles 18_23 " << (0.0 + ooo_cpu[i]->cycles_18_23) / tot_cycles << endl;
        //     cout << " cycles 24_29 " << (0.0 + ooo_cpu[i]->cycles_24_29) / tot_cycles << endl;
        //     cout << " above 29 " << (0.0 + ooo_cpu[i]->cycles_above) / tot_cycles << endl;
        // }



        // FS_prf counters
        // cout << "FS_prf 0-5: "  <<  ooo_cpu[i]->FS_prf_0_6 << endl;
        // cout << "FS_prf 6-11: " <<  ooo_cpu[i]->FS_prf_6_11 << endl;
        // cout << "FS_prf 12-17: " << ooo_cpu[i]->FS_prf_12_17 << endl;
        // cout << "FS_prf 18-23: " << ooo_cpu[i]->FS_prf_18_23 << endl;
        // cout << "FS_prf 24-29: " << ooo_cpu[i]->FS_prf_24_29 << endl;
        // cout << "FS_prf 30-35: " << ooo_cpu[i]->FS_prf_30_35 << endl;
        // cout << "FS_prf above: " << ooo_cpu[i]->FS_prf_above << endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
          record_roi_stats(i, *it);
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  cout << endl << "ChampSim completed all CPUs" << endl;
  if (NUM_CPUS > 1) {
    cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      cout << endl
           << "CPU " << i
           << " cumulative IPC: " << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
           << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  cout << endl << "Region of Interest Statistics" << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
  print_dram_stats();
  print_branch_stats();
#endif

  return 0;
}
