CC := gcc
CXX := g++
CFLAGS := -Wall -g -O3 -DDEBUG -std=gnu99
CXXFLAGS := -Wall -g -O3 -DDEBUG -std=c++17
CPPFLAGS :=  -Iinc -MMD -MP
LDFLAGS := 
LDLIBS := 

.phony: all clean

all: bin/champsim

clean: 
	$(RM) inc/champsim_constants.h
	$(RM) src/core_inst.cc
	$(RM) inc/cache_modules.inc
	$(RM) inc/ooo_cpu_modules.inc
	 find . -name \*.o -delete
	 find . -name \*.d -delete
	 $(RM) -r obj

	 find replacement/lru -name \*.o -delete
	 find replacement/lru -name \*.d -delete
	 find prefetcher/no -name \*.o -delete
	 find prefetcher/no -name \*.d -delete
	 find prefetcher/fdip -name \*.o -delete
	 find prefetcher/fdip -name \*.d -delete
	 find branch/bimodal -name \*.o -delete
	 find branch/bimodal -name \*.d -delete
	 find btb/basic_btb -name \*.o -delete
	 find btb/basic_btb -name \*.d -delete

bin/champsim: $(patsubst %.cc,%.o,$(wildcard src/*.cc)) obj/repl_rreplacementDlru.a obj/pref_pprefetcherDno.a obj/pref_pprefetcherDfdip.a obj/bpred_bbranchDbimodal.a obj/btb_bbtbDbasic_btb.a
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

replacement/lru/%.o: CFLAGS += -Ireplacement/lru
replacement/lru/%.o: CXXFLAGS += -Ireplacement/lru
replacement/lru/%.o: CXXFLAGS +=  -Dinitialize_replacement=repl_rreplacementDlru_initialize -Dfind_victim=repl_rreplacementDlru_victim -Dupdate_replacement_state=repl_rreplacementDlru_update -Dreplacement_final_stats=repl_rreplacementDlru_final_stats
obj/repl_rreplacementDlru.a: $(patsubst %.cc,%.o,$(wildcard replacement/lru/*.cc)) $(patsubst %.c,%.o,$(wildcard replacement/lru/*.c))
	@mkdir -p $(dir $@)
	ar -rcs $@ $^

prefetcher/no/%.o: CFLAGS += -Iprefetcher/no
prefetcher/no/%.o: CXXFLAGS += -Iprefetcher/no
prefetcher/no/%.o: CXXFLAGS +=  -Dprefetcher_initialize=pref_pprefetcherDno_initialize -Dprefetcher_cache_operate=pref_pprefetcherDno_cache_operate -Dprefetcher_cache_fill=pref_pprefetcherDno_cache_fill -Dprefetcher_cycle_operate=pref_pprefetcherDno_cycle_operate -Dprefetcher_final_stats=pref_pprefetcherDno_final_stats -Dl1d_prefetcher_initialize=pref_pprefetcherDno_initialize -Dl2c_prefetcher_initialize=pref_pprefetcherDno_initialize -Dllc_prefetcher_initialize=pref_pprefetcherDno_initialize -Dl1d_prefetcher_operate=pref_pprefetcherDno_cache_operate -Dl2c_prefetcher_operate=pref_pprefetcherDno_cache_operate -Dllc_prefetcher_operate=pref_pprefetcherDno_cache_operate -Dl1d_prefetcher_cache_fill=pref_pprefetcherDno_cache_fill -Dl2c_prefetcher_cache_fill=pref_pprefetcherDno_cache_fill -Dllc_prefetcher_cache_fill=pref_pprefetcherDno_cache_fill -Dl1d_prefetcher_final_stats=pref_pprefetcherDno_final_stats -Dl2c_prefetcher_final_stats=pref_pprefetcherDno_final_stats -Dllc_prefetcher_final_stats=pref_pprefetcherDno_final_stats
obj/pref_pprefetcherDno.a: $(patsubst %.cc,%.o,$(wildcard prefetcher/no/*.cc)) $(patsubst %.c,%.o,$(wildcard prefetcher/no/*.c))
	@mkdir -p $(dir $@)
	ar -rcs $@ $^

prefetcher/fdip/%.o: CFLAGS += -Iprefetcher/fdip
prefetcher/fdip/%.o: CXXFLAGS += -Iprefetcher/fdip
prefetcher/fdip/%.o: CXXFLAGS +=  -Dprefetcher_initialize=pref_pprefetcherDfdip_initialize -Dprefetcher_branch_operate=pref_pprefetcherDfdip_branch_operate -Dprefetcher_cache_operate=pref_pprefetcherDfdip_cache_operate -Dprefetcher_cycle_operate=pref_pprefetcherDfdip_cycle_operate -Dprefetcher_cache_fill=pref_pprefetcherDfdip_cache_fill -Dprefetcher_final_stats=pref_pprefetcherDfdip_final_stats -Dl1i_prefetcher_initialize=pref_pprefetcherDfdip_initialize -Dl1i_prefetcher_branch_operate=pref_pprefetcherDfdip_branch_operate -Dl1i_prefetcher_cache_operate=pref_pprefetcherDfdip_cache_operate -Dl1i_prefetcher_cycle_operate=pref_pprefetcherDfdip_cycle_operate -Dl1i_prefetcher_cache_fill=pref_pprefetcherDfdip_cache_fill -Dl1i_prefetcher_final_stats=pref_pprefetcherDfdip_final_stats
obj/pref_pprefetcherDfdip.a: $(patsubst %.cc,%.o,$(wildcard prefetcher/fdip/*.cc)) $(patsubst %.c,%.o,$(wildcard prefetcher/fdip/*.c))
	@mkdir -p $(dir $@)
	ar -rcs $@ $^

branch/bimodal/%.o: CFLAGS += -Ibranch/bimodal
branch/bimodal/%.o: CXXFLAGS += -Ibranch/bimodal
branch/bimodal/%.o: CXXFLAGS +=  -Dinitialize_branch_predictor=bpred_bbranchDbimodal_initialize -Dlast_branch_result=bpred_bbranchDbimodal_last_result -Dpredict_branch=bpred_bbranchDbimodal_predict
obj/bpred_bbranchDbimodal.a: $(patsubst %.cc,%.o,$(wildcard branch/bimodal/*.cc)) $(patsubst %.c,%.o,$(wildcard branch/bimodal/*.c))
	@mkdir -p $(dir $@)
	ar -rcs $@ $^

btb/basic_btb/%.o: CFLAGS += -Ibtb/basic_btb
btb/basic_btb/%.o: CXXFLAGS += -Ibtb/basic_btb
btb/basic_btb/%.o: CXXFLAGS +=  -Dinitialize_btb=btb_bbtbDbasic_btb_initialize -Dupdate_btb=btb_bbtbDbasic_btb_update -Dbtb_prediction=btb_bbtbDbasic_btb_predict
obj/btb_bbtbDbasic_btb.a: $(patsubst %.cc,%.o,$(wildcard btb/basic_btb/*.cc)) $(patsubst %.c,%.o,$(wildcard btb/basic_btb/*.c))
	@mkdir -p $(dir $@)
	ar -rcs $@ $^

-include $(wildcard src/*.d)
-include $(wildcard replacement/lru/*.d)
-include $(wildcard prefetcher/no/*.d)
-include $(wildcard prefetcher/fdip/*.d)
-include $(wildcard branch/bimodal/*.d)
-include $(wildcard btb/basic_btb/*.d)

