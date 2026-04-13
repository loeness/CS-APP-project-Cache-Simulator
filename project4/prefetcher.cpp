#include "prefetcher.h"

std::vector<uint64_t> NextLinePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)miss;
    std::vector<uint64_t> prefetches;

    if (block_size == 0) return prefetches;
    uint64_t current_block = current_addr / block_size;
    prefetches.push_back((current_block + 1) * block_size);
    prefetches.push_back((current_block + 2) * block_size);

    return prefetches;
}

std::vector<uint64_t> StridePrefetcher::calculatePrefetch(uint64_t current_addr, bool miss) {
    (void)miss;
    std::vector<uint64_t> prefetches;
    if (block_size == 0) return prefetches;

    uint64_t current_block = current_addr / block_size;

    if (!has_last_block) {
        has_last_block = true;
        last_block = current_block;
        return prefetches;
    }

    int64_t stride = static_cast<int64_t>(current_block) - static_cast<int64_t>(last_block);
    if (stride == 0) {
        if (confidence > 0) confidence--;
        last_block = current_block;
        return prefetches;
    }

    auto push_unique_prefetch = [&](int64_t pf_block) {
        if (pf_block < 0) return;
        uint64_t pf_addr = static_cast<uint64_t>(pf_block) * block_size;
        for (size_t i = 0; i < prefetches.size(); ++i) {
            if (prefetches[i] == pf_addr) return;
        }
        prefetches.push_back(pf_addr);
    };

    static uint32_t conf_stride1 = 0;
    static uint32_t conf_stride64 = 0;
    static int64_t dir_stride1 = 1;
    static int64_t dir_stride64 = 64;

    int64_t abs_stride = (stride < 0) ? -stride : stride;
    if (abs_stride == 1) {
        dir_stride1 = stride;
        if (conf_stride1 < 8) conf_stride1++;
    } else if (conf_stride1 > 0) {
        conf_stride1--;
    }

    if (abs_stride == 64) {
        dir_stride64 = stride;
        if (conf_stride64 < 8) conf_stride64++;
    } else if (conf_stride64 > 0) {
        conf_stride64--;
    }

    if (conf_stride1 >= 2) {
        push_unique_prefetch(static_cast<int64_t>(current_block) + dir_stride1);
        push_unique_prefetch(static_cast<int64_t>(current_block) + dir_stride1 * 2);
    }

    if (conf_stride64 >= 2) {
        push_unique_prefetch(static_cast<int64_t>(current_block) + dir_stride64);
        push_unique_prefetch(static_cast<int64_t>(current_block) + dir_stride64 * 2);
    }

    if (stride == last_stride) {
        if (confidence < 8) confidence++;
    } else {
        confidence = 1;
        last_stride = stride;
    }

    if (confidence >= 3 && abs_stride != 1 && abs_stride != 64) {
        push_unique_prefetch(static_cast<int64_t>(current_block) + stride);
    }

    last_block = current_block;
    return prefetches;
}

Prefetcher* createPrefetcher(std::string name, uint32_t block_size) {
    if (name == "NextLine") return new NextLinePrefetcher(block_size);
    if (name == "Stride") return new StridePrefetcher(block_size);
    return new NoPrefetcher();
}
