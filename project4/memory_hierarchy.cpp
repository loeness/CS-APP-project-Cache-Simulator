#include "memory_hierarchy.h"
#include "prefetcher.h"
#include "repl_policy.h"
#include <cmath>
#include <iomanip>
#include <iostream>

using namespace std;

MainMemory::MainMemory(int lat) : latency(lat) {}

int MainMemory::access(uint64_t addr, char type, uint64_t cycle) {
    (void)addr;
    (void)type;
    (void)cycle;
    access_count++;
    return latency;
}

void MainMemory::printStats() {
    cout << "  [Main Memory] Total Accesses: " << access_count << endl;
}

CacheLevel::CacheLevel(string name, CacheConfig cfg, MemoryObject* next)
    : level_name(name), config(cfg), next_level(next) {
    policy = createReplacementPolicy(config.policy_name);
    prefetcher = createPrefetcher(config.prefetcher, config.block_size);

    uint64_t total_bytes = (uint64_t)config.size_kb * 1024;
    num_sets = total_bytes / (config.block_size * config.associativity);

    offset_bits = log2(config.block_size);
    index_bits = log2(num_sets);

    sets.resize(num_sets, vector<CacheLine>(config.associativity));

    cout << "Constructed " << level_name << ": "
         << config.size_kb << "KB, " << config.associativity << "-way, "
         << config.latency << "cyc, "
         << "[" << config.policy_name << " + " << prefetcher->getName() << "]" << endl;
}

CacheLevel::~CacheLevel() {
    delete policy;
    delete prefetcher;
}

uint64_t CacheLevel::get_index(uint64_t addr) {
    // TODO: Task 1
    // Compute the set index from the address.
    // Hint: remove block offset bits first, then keep only the index bits.
    uint64_t mask = ((uint64_t)1 << index_bits) - 1;
    uint64_t index = (addr >> offset_bits) & mask;
    return index;
}

uint64_t CacheLevel::get_tag(uint64_t addr) {
    // TODO: Task 1
    // Compute the tag from the address.
    // Hint: shift away both block offset bits and set index bits.
    uint64_t tag = addr >> (offset_bits + index_bits);
    return tag;
}

uint64_t CacheLevel::reconstruct_addr(uint64_t tag, uint64_t index) {
    // TODO: Task 1 / Task 2
    // Rebuild a block-aligned address from a tag and set index.
    // This helper is useful when writing back an evicted dirty line.
    uint64_t res_tag = tag << (offset_bits + index_bits);
    uint64_t res_index = index << offset_bits;
    uint64_t addr = (res_tag | res_index);
    return addr;
}

void CacheLevel::write_back_victim(const CacheLine& line, uint64_t index, uint64_t cycle) {
    // TODO: Task 1 / Task 2
    // Move dirty write-back logic into this helper.
    // Suggested steps:
    // 1. If the victim is not dirty, return immediately.
    // 2. If there is no next level, return immediately.
    // 3. Increment the write-back counter.
    // 4. Reconstruct the evicted block address from tag + index.
    // 5. Send a write access to the next level.
    if (!line.valid || !line.dirty) return;
    if (!next_level) return;

    write_backs++;
    uint64_t wb_addr = reconstruct_addr(line.tag, index);
    (void)next_level->access(wb_addr, 'w', cycle);
}

int CacheLevel::access(uint64_t addr, char type, uint64_t cycle) {
    int lat = config.latency;

    // TODO: Task 1
    // 1. Derive the address fields for the current cache geometry:
    //    - block offset bits
    //    - set index bits
    //    - tag bits
    // 2. Use the address to compute index/tag and select the set.
    // 3. Search all ways for a valid tag match.
    // 4. On hit:
    //    - increment hits
    //    - call policy->onHit(...)
    //    - update dirty bit for writes
    //    - clear is_prefetched if a prefetched line is consumed
    // 5. On miss:
    //    - increment misses
    //    - find an invalid line or select a victim with policy->getVictim(...)
    //    - call write_back_victim(...) if the chosen victim is dirty
    //    - fetch the requested block from next_level and add that latency to lat
    //    - install the new cache line and call policy->onMiss(...)
    // 6. Your code should work correctly even if cache size, associativity,
    //    number of sets, or cache line size changes.
    // 7. Task 3: after demand access logic works, call the prefetcher here and
    //    install returned blocks through install_prefetch(...).
    uint64_t res_index = get_index(addr);
    uint64_t res_tag = get_tag(addr);
    auto& set = sets[res_index];
    int hit_way = -1;
    for (size_t way = 0; way < set.size(); ++way) {
        if (set[way].valid && set[way].tag == res_tag) {
            hit_way = static_cast<int>(way);
            break;
        }
    }

    bool miss = (hit_way == -1);
    if (!miss) {
        hits++;
        policy->onHit(set, hit_way, cycle);
        if (type == 'w') set[hit_way].dirty = true;
        if (set[hit_way].is_prefetched) set[hit_way].is_prefetched = false;
    } else {
        misses++;
        int victim_way = -1;
        for (size_t way = 0; way < set.size(); ++way) {
            if (!set[way].valid) {
                victim_way = static_cast<int>(way);
                break;
            }
        }

        if (victim_way == -1) {
            victim_way = policy->getVictim(set);
            if (victim_way < 0) victim_way = 0;
            write_back_victim(set[victim_way], res_index, cycle);
        }

        if (next_level) lat += next_level->access(addr, 'r', cycle);

        set[victim_way].tag = res_tag;
        set[victim_way].valid = true;
        set[victim_way].dirty = (type == 'w');
        set[victim_way].is_prefetched = false;
        policy->onMiss(set, victim_way, cycle);
    }

    std::vector<uint64_t> prefetches = prefetcher->calculatePrefetch(addr, miss);
    for (uint64_t pf_addr : prefetches) {
        prefetch_issued++;
        install_prefetch(pf_addr, cycle);
    }

    return lat;
}

void CacheLevel::install_prefetch(uint64_t addr, uint64_t cycle) {
    // TODO: Task 3
    // Implement a prefetch fill path similar to the miss path in access(), but
    // treat prefetched lines as clean and mark is_prefetched = true.
    // If you evict a dirty victim during prefetch installation, reuse
    // write_back_victim(...) instead of duplicating that logic.
    uint64_t res_index = get_index(addr);
    uint64_t res_tag = get_tag(addr);
    auto& set = sets[res_index];

    for (size_t way = 0; way < set.size(); ++way) {
        if (set[way].valid && set[way].tag == res_tag) {
            return;
        }
    }

    int victim_way = -1;
    for (size_t way = 0; way < set.size(); ++way) {
        if (!set[way].valid) {
            victim_way = static_cast<int>(way);
            break;
        }
    }

    if (victim_way == -1) {
        victim_way = policy->getVictim(set);
        if (victim_way < 0) victim_way = 0;
        write_back_victim(set[victim_way], res_index, cycle);
    }

    if (next_level) {
        (void)next_level->access(addr, 'r', cycle);

        // When prefetching into this level, also warm adjacent blocks in
        // the next cache level to increase downstream hit chance.
        if (next_level->getName() != "Main Memory") {
            uint64_t next_addr1 = addr + config.block_size;
            uint64_t next_addr2 = addr + (config.block_size * 2ULL);
            (void)next_level->access(next_addr1, 'r', cycle);
            (void)next_level->access(next_addr2, 'r', cycle);
        }
    }

    set[victim_way].tag = res_tag;
    set[victim_way].valid = true;
    set[victim_way].dirty = false;
    set[victim_way].is_prefetched = true;

    // BIP is very conservative on insertion; speculative lines may be evicted
    // before use. For BIP, give prefetched lines MRU-like placement.
    if (config.policy_name == "BIP") {
        set[victim_way].last_access = cycle;
    } else {
        policy->onMiss(set, victim_way, cycle);
    }
}

void CacheLevel::printStats() {
    uint64_t total = hits + misses;
    cout << "  [" << level_name << "] "
         << "Hit Rate: " << fixed << setprecision(2) << (total ? (double)hits / total * 100.0 : 0) << "% "
         << "(Access: " << total << ", Miss: " << misses << ", WB: " << write_backs << ")" << endl;
    cout << "      Prefetches Issued: " << prefetch_issued << endl;
}