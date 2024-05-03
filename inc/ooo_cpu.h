#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <functional>
#include <queue>

#include "block.h"
#include "champsim.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"

using namespace std;

class CACHE;

class CacheBus : public MemoryRequestProducer
{
public:
  champsim::circular_buffer<PACKET> PROCESSED;
  CacheBus(std::size_t q_size, MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), PROCESSED(q_size) {}
  void return_data(PACKET* packet);
};

// cpu
class O3_CPU : public champsim::operable
{
public:
  uint32_t cpu = 0;

  // instruction
  uint64_t instr_unique_id = 0, completed_executions = 0, begin_sim_cycle = 0, begin_sim_instr = 0, last_sim_cycle = 0, last_sim_instr = 0,
           finish_sim_cycle = 0, finish_sim_instr = 0, instrs_to_read_this_cycle = 0, instrs_to_fetch_this_cycle = 0,
           next_print_instruction = STAT_PRINTING_PERIOD, num_retired = 0;
  uint32_t inflight_reg_executions = 0, inflight_mem_executions = 0;

  struct dib_entry_t {
    bool valid = false;
    unsigned lru = 999999;
    uint64_t address = 0;
  };

  // instruction buffer
  using dib_t = std::vector<dib_entry_t>;
  const std::size_t dib_set, dib_way, dib_window;
  dib_t DIB{dib_set * dib_way};

  // PTQ and que for recently prefetched
  #define MAX_PTQ_ENTRIES 128 // Same length as IFETCH_BUFFER.
  #define MAX_RECENTLY_PREFETCHED_ENTRIES 15 //TODO: Find ideal length
  uint64_t instrs_to_speculate_this_cycle = 0;
  uint64_t ptq_prefetch_entry = 0;
  uint64_t compare_index = 0;
  uint64_t current_block_address_ftq = 0;
  bool ptq_init = false;
  std::pair<uint64_t, uint8_t> current_btb_prediction;
  bool speculate = false;
  uint64_t num_entries_in_ftq = 0;
  uint64_t num_speculated_fetch_stall = 0;


  // STATS
  // for calculation of miss rate for WP start
  bool prev_prefetch_was_fetch_stall = false;
  uint64_t fetch_stall_prf_number = 0;
  uint64_t FS_prf_0_6 = 0, FS_prf_6_11 = 0, FS_prf_12_17 = 0, FS_prf_18_23 = 0, FS_prf_24_29 = 0, FS_prf_30_35 = 0, FS_prf_above = 0;

  // for calculation of miss rate for WP end
  uint64_t num_ftq_flush = 0;
  uint64_t num_ftq_flush_conditional = 0;
  uint64_t num_ftq_flush_call_return = 0;
  uint64_t num_ftq_flush_other = 0;
  uint64_t num_prefetched_wrong_path = 0;
  uint64_t num_entries_in_ftq_when_flush = 0;
  uint64_t num_useful_prefetch_fetch_stall = 0;
  uint64_t num_cycles_fetch_stall = 0;
  bool conditional_bm = false;
  uint64_t num_prefetched_wrong_path_contitional = 0;
  uint64_t num_ptq_flushed = 0;

  // What did we prefetcch on wrong path and how many of them were useful
  uint64_t num_instr_fetch_stall = 0;
  uint64_t num_cb_to_PTQ_fetch_stall = 0;
  uint64_t num_cb_0_5 = 0;
  uint64_t num_cb_0 = 0;
  uint64_t num_cb_1 = 0;
  uint64_t num_cb_2 = 0;
  uint64_t num_cb_3 = 0;
  uint64_t num_cb_4 = 0;
  uint64_t num_cb_6_10 = 0;
  uint64_t num_cb_11_15 = 0;
  uint64_t num_cb_16_20 = 0;
  uint64_t num_cb_21_25 = 0;
  uint64_t num_cb_26_128 = 0;

  uint64_t num_addr_to_PTQ_fetch_stall = 0;
  uint64_t num_addr_0_39_5 = 0;
  uint64_t num_addr_0_39 = 0;
  uint64_t num_addr_40_79 = 0;
  uint64_t num_addr_80_119 = 0;
  uint64_t num_addr_120_159 = 0;
  uint64_t num_addr_160_199 = 0;
  uint64_t num_addr_6_11 = 0;
  uint64_t num_addr_40_792_17 = 0;
  uint64_t num_addr_40_798_23 = 0;
  uint64_t num_addr_80_1194_29 = 0;
  uint64_t num_addr_above = 0;

  //time to fetch after bm resolved that has not been prefetched
  uint64_t cb_until_time_start = 0;
  uint64_t cycles_fetch_first_cb_after_prf = 0;
  bool start_counting_cycles = false;
  uint64_t index_start_count = 0;
  uint64_t cycles_0_5 = 0;
  uint64_t cycles_0 = 0;
  uint64_t cycles_1 = 0;
  uint64_t cycles_2 = 0;
  uint64_t cycles_3 = 0;
  uint64_t cycles_4 = 0;
  uint64_t cycles_5 = 0;
  uint64_t cycles_6_11 = 0;
  uint64_t cycles_12_17 = 0;
  uint64_t cycles_18_23 = 0;
  uint64_t cycles_24_29 = 0;
  uint64_t cycles_above = 0;
  uint64_t percentage_tot = 0;

  uint64_t num_fetch_stall = 0;
  uint64_t blocks_speculated_after_fetch_stall = 0;
  bool spec_after_fetch_stall = false;
  uint64_t prefetched_spec_after_fetch_stall = 0;

  struct PTQ_entry{
    uint64_t block_address = 0;
    bool speculated = false;
    bool fetch_stall = false;
  };
  PTQ_entry ptq_entry;

  std::deque<PTQ_entry> PTQ;
  std::deque<uint64_t> FTQ;
  std::deque<uint64_t> recently_prefetched;

  struct compare_wp_rp {
    std::deque<uint64_t> PTQ_wp;
    std::deque<uint64_t> FTQ_when_ptq_wp;

    uint64_t total_entries_to_compare = 0;
    uint64_t total_equal_entries = 0;
    uint64_t total_comparisons = 0;
    uint64_t num_queues_same_order = 0;

    void compare_wp_rp_entries();
    void compare_wp_rp_entries_print_results();


  };
  compare_wp_rp compare_paths;

  // reorder buffer, load/store queue, register file
  champsim::circular_buffer<ooo_model_instr> IFETCH_BUFFER;
  champsim::delay_queue<ooo_model_instr> DISPATCH_BUFFER;
  champsim::delay_queue<ooo_model_instr> DECODE_BUFFER;
  champsim::circular_buffer<ooo_model_instr> ROB;
  std::vector<LSQ_ENTRY> LQ;
  std::vector<LSQ_ENTRY> SQ;

  // Constants
  const unsigned FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, LQ_WIDTH, SQ_WIDTH, RETIRE_WIDTH;
  const unsigned BRANCH_MISPREDICT_PENALTY, SCHEDULING_LATENCY, EXEC_LATENCY;

  // store array, this structure is required to properly handle store
  // instructions
  std::queue<uint64_t> STA;

  // Ready-To-Execute
  std::queue<champsim::circular_buffer<ooo_model_instr>::iterator> ready_to_execute;

  // Ready-To-Load
  std::queue<std::vector<LSQ_ENTRY>::iterator> RTL0, RTL1;

  // Ready-To-Store
  std::queue<std::vector<LSQ_ENTRY>::iterator> RTS0, RTS1;

  // branch
  uint8_t fetch_stall = 0;
  uint64_t fetch_resume_cycle = 0;
  uint64_t num_branch = 0, branch_mispredictions = 0;
  uint64_t total_rob_occupancy_at_branch_mispredict;

  uint64_t total_branch_types[8] = {};
  uint64_t branch_type_misses[8] = {};

  CacheBus ITLB_bus, DTLB_bus, L1I_bus, L1D_bus;
  CACHE* l1i;

  void operate() override;

  // functions
  void init_instruction(ooo_model_instr instr);
  // PTQ
  void fill_prefetch_queue(uint64_t ip);
  void fill_ptq_speculatively();
  void new_cache_block_fetch();
  void compare_queues();
  void clear_PTQ();

  // STAT
  void collect_prefetch_stats(bool prefetch_from_fetch_stall);
  void collect_cb_added__stats();
  void collect_addr_added__stats();
  void count_cycles_until_fetch();


  void check_dib();
  void translate_fetch();
  void fetch_instruction();
  void promote_to_decode();
  void decode_instruction();
  void dispatch_instruction();
  void schedule_instruction();
  void execute_instruction();
  void schedule_memory_instruction();
  void execute_memory_instruction();
  void do_check_dib(ooo_model_instr& instr);
  void do_translate_fetch(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end);
  void do_fetch_instruction(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end);
  void do_dib_update(const ooo_model_instr& instr);
  void do_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_memory_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void operate_lsq();
  void do_complete_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void initialize_core();
  void add_load_queue(champsim::circular_buffer<ooo_model_instr>::iterator rob_index, uint32_t data_index);
  void add_store_queue(champsim::circular_buffer<ooo_model_instr>::iterator rob_index, uint32_t data_index);
  void execute_store(std::vector<LSQ_ENTRY>::iterator sq_it);
  int execute_load(std::vector<LSQ_ENTRY>::iterator lq_it);
  int do_translate_store(std::vector<LSQ_ENTRY>::iterator sq_it);
  int do_translate_load(std::vector<LSQ_ENTRY>::iterator sq_it);
  void check_dependency(int prior, int current);
  void operate_cache();
  void complete_inflight_instruction();
  void handle_memory_return();
  void retire_rob();

  void print_deadlock() override;

  int prefetch_code_line(uint64_t pf_v_addr);

#include "ooo_cpu_modules.inc"

  const bpred_t bpred_type;
  const btb_t btb_type;
  const ipref_t ipref_type;

  O3_CPU(uint32_t cpu, double freq_scale, std::size_t dib_set, std::size_t dib_way, std::size_t dib_window, std::size_t ifetch_buffer_size,
         std::size_t decode_buffer_size, std::size_t dispatch_buffer_size, std::size_t rob_size, std::size_t lq_size, std::size_t sq_size, unsigned fetch_width,
         unsigned decode_width, unsigned dispatch_width, unsigned schedule_width, unsigned execute_width, unsigned lq_width, unsigned sq_width,
         unsigned retire_width, unsigned mispredict_penalty, unsigned decode_latency, unsigned dispatch_latency, unsigned schedule_latency,
         unsigned execute_latency, MemoryRequestConsumer* itlb, MemoryRequestConsumer* dtlb, MemoryRequestConsumer* l1i, MemoryRequestConsumer* l1d,
         bpred_t bpred_type, btb_t btb_type, ipref_t ipref_type)
      : champsim::operable(freq_scale), cpu(cpu), dib_set(dib_set), dib_way(dib_way), dib_window(dib_window), IFETCH_BUFFER(ifetch_buffer_size),
        DISPATCH_BUFFER(dispatch_buffer_size, dispatch_latency), DECODE_BUFFER(decode_buffer_size, decode_latency), ROB(rob_size), LQ(lq_size), SQ(sq_size),
        FETCH_WIDTH(fetch_width), DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width), SCHEDULER_SIZE(schedule_width), EXEC_WIDTH(execute_width),
        LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width), BRANCH_MISPREDICT_PENALTY(mispredict_penalty), SCHEDULING_LATENCY(schedule_latency),
        EXEC_LATENCY(execute_latency), ITLB_bus(rob_size, itlb), DTLB_bus(rob_size, dtlb), L1I_bus(rob_size, l1i), L1D_bus(rob_size, l1d),
        bpred_type(bpred_type), btb_type(btb_type), ipref_type(ipref_type)
  {
  }
};

#endif
