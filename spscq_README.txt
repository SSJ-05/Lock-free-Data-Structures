Single Producer Single Consumer Queue (SPSC)

This project implements a minimal lock-free Single Producer / Single Consumer queue using atomic pointers and a linked-list structure.

The implementation focuses on:
	-	Clear memory ordering
	-	Predictable behavior
	-	Minimal synchronization overhead
	-	Separation of structural correctness from advanced memory reclamation

It is intended for educational and experimental purposes in low-latency systems.


Design Overview
1. The queue maintains two atomic pointers:

std::atomic<Node*> head_;
std::atomic<Node*> tail_;

2. Each node contains:

struct Node {
    std::shared_ptr<T> data_;
    Node* next_;
};

3. A dummy node is used at initialization so that head_ and tail_ always exist and the queue logic remains simple.



Why Single Producer / Single Consumer?

This design assumes:
	-	Exactly one producer thread
	-	Exactly one consumer thread

Under SPSC constraints:
	-	No CAS is required.
	-	No ABA problem arises.
	-	Only one thread modifies tail_.
	-	Only one thread modifies head_.

This significantly simplifies correctness and reduces synchronization cost compared to MPMC designs.


Why a Linked List?
The queue is implemented as a linked list with a moving dummy node.

Rationale:
	-	Constant-time push and pop.
	-	No fixed capacity constraint.
	-	No index arithmetic or wrap-around logic.
	-	Easy reasoning about ownership transfer.

For ultra-low-latency production systems, a bounded ring buffer is often preferable due to:
	-	Contiguous memory
	-	Better cache locality
	-	No per-node allocation

**This implementation favors conceptual clarity over absolute performance.



Why std::shared_ptr<T> ?
Each node stores:
std::shared_ptr<T> data_;

Reasons:
	-	Simplifies object lifetime management.
	-	Ensures returned values remain valid even after node deletion.
	-	Avoids copying large T objects.

Tradeoffs:
	-	Atomic reference counting
	-	Additional control block allocation
	-	Extra cache traffic

In performance-critical systems, this would likely be replaced by:
	-	Value storage with noexcept move types
	-	Custom allocators
	-	Arena-based memory management



Memory Ordering Strategy
The design uses minimal memory ordering:

Producer (push)
tail_.store(p, std::memory_order_release);

This ensures:
	-	All prior writes (data assignment and next_ linkage)
	-	Become visible to the consumer before the tail update.

Consumer (pop_head)
tail_.load(std::memory_order_acquire);

This pairs with the release store and guarantees visibility of:
	-	Node linkage
	-	Node data

Other loads/stores use memory_order_relaxed because:
	-	Only one thread modifies head_
	-	Only one thread modifies tail_
	-	There is no concurrent modification of the same atomic by multiple threads
This keeps synchronization overhead minimal while preserving correctness.


Dummy Node Pattern
The queue always maintains one empty node at the tail.

Push:
Writes data into the current tail node
Appends a new empty node
Advances tail_

Pop:
Reads from head_
Advances head_
Deletes the old node

This avoids special-case handling for empty states and simplifies logic.



Destruction
Destructor behavior:

while (pop());
delete head_.load();

Assumes:
	-	No concurrent producer/consumer activity during destruction.
	-	Queue is externally synchronized before destruction.

Characteristics
	-	Lock-free under SPSC constraints
	-	No CAS required
	-	No ABA exposure
	-	Minimal memory barriers
	-	Unbounded growth (heap allocations per push)

**Not Production-Ready
