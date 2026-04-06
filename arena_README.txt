Arena — Hugepage-Backed Bump Allocator

A low-latency memory arena designed for HFT systems.
Eliminates `malloc`/`free` overhead in hot paths via hugepage-backed bump allocation.

Design Philosophy
- "Hugepages or death" - No fallback to 4KB pages.
- "mlock or death" - No page faults in hot path.
- Bump allocation - O(1) allocation — one pointer increment.
- "Reset, don't free" - reset() reuses entire arena.

Std allocators maintain free lists, metadata, and locks.
In a hot path processing millions of messages per second, this overhead is unacceptable. 
Arena replaces all of this with a single pointer increment.

Features
- 2MB hugepage-backed via MAP_HUGETLB
- Memory locked via mlock — guaranteed resident in RAM, never swapped
- Pages pre-faulted at startup via touch_all_pages() 
- Cache line aligned allocation (64 bytes default)
- create_order<T> — placement new with perfect forwarding
- reset() — O(1) full arena reuse without deallocation
- No exceptions, no heap, no dynamic allocation after construction
- C++23, Linux only

Requirements
- Linux kernel with hugepage support
- C++23 compiler (GCC 13+ or Clang 16+)
- Root or `CAP_IPC_LOCK` capability for mlock



Hugepage Setup
***bash
allocate 20 hugepages (20 x 2MB = 40MB)
echo 20 | sudo tee /proc/sys/vm/nr_hugepages

verify allocation
grep Huge /proc/meminfo

check available hugepage sizes
ls /sys/kernel/mm/hugepages/

explicitly set 2MB hugepage count
echo 20 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

release hugepages after use
echo 0 | sudo tee /proc/sys/vm/nr_hugepages
***

Usage
***cpp
#include "Arena.hpp"

struct Order {
    int id;
    double price;
    int quantity;
};

// allocate 40MB hugepage-backed arena
Arena arena(20 * 2 * 1024 * 1024);

// create objects directly in arena — zero malloc
Order* o1 = arena.create_order<Order>(1, 150.0, 100);
Order* o2 = arena.create_order<Order>(2, 151.5, 200);

// arena statistics
std::cout << "used:      " << arena.used()      << " bytes\n";
std::cout << "available: " << arena.available() << " bytes\n";
std::cout << "capacity:  " << arena.capacity()  << " bytes\n";

// reset entire arena — O(1), no destructors, no free
// preferred pattern for end-of-batch reuse
arena.reset();
***

License

MIT
