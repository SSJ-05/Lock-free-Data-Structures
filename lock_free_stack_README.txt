This project implements a lock-free stack using std::atomic<Node*> and CAS.
The design choices below are intentional and optimized for reasoning about concurrency tradeoffs in low-latency systems.

1. Why a Linked List Under the Hood?
The stack is implemented as a singly linked list.

Rationale:
    - Push/pop operations operate only on the head pointer.
    - A linked list allows O(1) insertion/removal at the head.
    - No resizing or reallocation like std::vector.
    - Avoids contiguous memory relocation, which would complicate atomic updates.
    - For a lock-free stack, a linked structure maps naturally to the CAS pattern.
This keeps the critical update to a single atomic pointer.


2. Spin Limit Before Yield
Push uses a bounded spin before yielding:
    static constexpr std::uint8_t spin_limit_ { 100 };

Behavior:
    - Attempt CAS up to spin_limit_ times.
    - Use _mm_pause() between attempts.
    - If still failing, fall back to std::this_thread::yield().

Why:
    - Short critical sections benefit from spinning.
    - Avoids immediate scheduler involvement.
    - Prevents infinite busy-wait under heavy contention or preemption.

This hybrid approach balances:
    - Latency under light contention
    - CPU waste under sustained contention


3. Intentional Memory Leak
Nodes are allocated with new and never reclaimed. This is deliberate.

The focus of this implementation is:
    - Atomic correctness
    - Memory ordering reasoning
    - CAS behavior under contention

Safe memory reclamation in lock-free structures introduces additional complexity:
    - Hazard pointers
    - Epoch-based reclamation
    - Reference counting on nodes
These mechanisms obscure the core atomic logic.

**This implementation isolates the lock-free algorithm from memory reclamation concerns for clarity and experimentation.

4. Why _mm_pause()?
This emits the PAUSE instruction on x86.

Benefits:
    - Reduces pipeline penalties in tight spin loops.
    - Lowers power consumption during spin.
    - Improves performance on hyper-threaded cores.
    - Reduces memory order violation penalties on failed CAS loops.

Without PAUSE, tight spin loops can:
    - Increase inter-core contention.
    - Harm sibling logical core performance.
    - Create excessive speculative execution pressure.

**Notes:
This implementation is:
    - Lock-free (push/pop)
    - Not memory-safe (nodes are leaked)
    - Intended for educational and experimental use

It is not production-ready without:
    - Proper memory reclamation
    - Careful ABA mitigation
    - Benchmark validation under real workload
