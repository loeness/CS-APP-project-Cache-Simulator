#include "repl_policy.h"

// =========================================================
// TODO: Task 1 / Task 3 replacement policies
// Implement LRU first, then extend with SRRIP / BIP.
// =========================================================

void LRUPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].last_access = cycle;
}

void LRUPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    set[way].last_access = cycle;
}

int LRUPolicy::getVictim(std::vector<CacheLine>& set) {
    int victim = -1;
    uint64_t min_time = 0xFFFFFFFFFFFFFFFF; 
    for (size_t i = 0; i < set.size(); ++i)
    {
        if (set[i].valid && set[i].last_access < min_time)
        {
            min_time = set[i].last_access;
            victim = static_cast<int>(i);
        }
    }
    return victim;
}

void SRRIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    // Hit lines are predicted to be re-referenced soon.
    set[way].rrpv = 0;
}

void SRRIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    (void)cycle;
    // SRRIP inserts new lines with a long interval (not immediate reuse).
    set[way].rrpv = 2;
}

int SRRIPPolicy::getVictim(std::vector<CacheLine>& set) {
    while (true) {
        for (size_t i = 0; i < set.size(); ++i) {
            if (!set[i].valid) return static_cast<int>(i);
            if (set[i].rrpv == 3) return static_cast<int>(i);
        }

        for (size_t i = 0; i < set.size(); ++i) {
            if (set[i].valid && set[i].rrpv < 3) {
                set[i].rrpv++;
            }
        }
    }
}

void BIPPolicy::onHit(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // BIP still treats hits as MRU updates.
    set[way].last_access = cycle;
}

void BIPPolicy::onMiss(std::vector<CacheLine>& set, int way, uint64_t cycle) {
    // Mostly insert at LRU position to reduce cache pollution.
    (void)set;
    ++insertion_counter;
    bool insert_mru = (insertion_counter % throttle == 0);
    set[way].last_access = insert_mru ? cycle : 0;
}

int BIPPolicy::getVictim(std::vector<CacheLine>& set) {
    int victim = -1;
    uint64_t min_time = 0xFFFFFFFFFFFFFFFF;
    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) return static_cast<int>(i);
        if (set[i].last_access < min_time) {
            min_time = set[i].last_access;
            victim = static_cast<int>(i);
        }
    }
    return victim;
}

ReplacementPolicy* createReplacementPolicy(std::string name) {
    if (name == "SRRIP") return new SRRIPPolicy();
    if (name == "BIP") return new BIPPolicy();
    return new LRUPolicy();
}
