# CSC3060 Project 4: Cache Simulator

Hey there! This is my solo submission for Project 4. I built out the cache simulator starting from a basic L1 setup, eventually wiring up the L2 cache and testing a ton of different replacement and prefetching strategies to get the AMAT down.


## How to Build
Getting this thing to compile is pretty simple. Just open your terminal in this directory and run:
```bash
make
```
If things get weird or you just want a clean slate before recompiling, run:
```bash
make clean
```
That'll wipe out all the compiled binaries and `.o` files so you can start over fresh.

---

## Running the Simulator
I didn't mess with the default grading targets at all, so you can test my code exactly how the assignment PDF asks you to.

### Task 1: Single-Level L1
```bash
make task1
```
This runs the basic single L1 cache (32KB, 8-way, LRU) on the `trace_sanity.txt` file. Everything checks out here—it hits the expected 79.57 AMAT perfectly.

### Task 2: Adding L2 to the Mix
```bash
make task2
```
This tests the multi-level hierarchy. It was pretty cool to finally see the L1 misses correctly spilling over into the L2 cache. It definitely works, pulling the AMAT down to about 45.21 cycles.

### Task 3: The Big Optimization
```bash
make task3
```
Honestly, finding the right config for this part took a lot of trial and error. I ran the provided python script to analyze my personalized trace and noticed a massive amount of stride-1 (55.8%) and stride-64 accesses. Also, set 26 was getting absolutely hammered by conflicts. 

To deal with this, I hardcoded my best layout straight into the `Makefile`. My final tuned variables look like this:
- `TASK3_ASSOC = 8`
- `TASK3_BLOCK = 64`
- `TASK3_L1_POLICY = BIP`
- `TASK3_L1_PREFETCH = Stride`
- `TASK3_L2_POLICY = SRRIP`
- `TASK3_L2_PREFETCH = NextLine`

**Why this setup?** Basically, throwing BIP on the L1 cache does an awesome job of filtering out scan-heavy garbage that would normally pollute the cache. Meanwhile, the Stride prefetcher catches those repetitive jumps I saw in the trace analysis. For the L2 cache, SRRIP acts as a really solid safety net to keep the actual useful blocks around long-term, backed up by a simple NextLine prefetcher.

With this exact configuration, my final AMAT on `my_trace.txt` is **1.65 cycles**. I was pretty hyped to get it that low!
