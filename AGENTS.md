# AGENT.md
#
#
# My preference is to use AGENTS.md so other agents (Codex, etc) can use it as well.
# CLAUDE.md will consume the AGENTS.md  
#

## Project Identity

This is a **kernel-bypass, zero-copy RoCEv2 congestion telemetry simulator**. It demonstrates production-accurate networking architecture (as much as we can) by mocking only the physical NIC layer while keeping every software layer
real. The codebase spans kernel space and userspace, and must be treated accordingly.

## Important Repository Layout

```
rocev2-telemetry/
├── kernel_driver/
│   ├── mock_nic.c             ← Linux kernel module (.ko)
│   └── Makefile               ← kbuild Makefile (not cmake)
├── userspace/
│   └── rocev2_header.h        ← shared struct: RoCEv2Header + congestion_flag
├── demo/
│   ├── integrated_demo.cpp    ← SimulatorThread + TelemetryThread + HTTP server
│   └── CMakeLists.txt
└── scripts/
    └── run_demo.sh            ← end-to-end: insmod → build → run → cleanup
```

## Architecture — The Full Stack

### Layer 0: Physical RAM (kernel_driver/mock_nic.c)

The kernel module calls "kmalloc(GFP_KERNEL)" to claim a physically contiguous, non-swappable 4 KB page. This is the DMA ring buffer. In real world a real NIC would DMA-write packet data to the mem. address in the descriptors here over PCIe. We have no NIC, so the simulator thread plays that role.

The module registers "/dev/mock_nic" as a char device. Its mmap() handler calls
"remap_pfn_range()" which directly programs the CPU's MMU page tables, wiring a userspace virtual address (UVA) to the physical frame (PFN) of the kmalloc'd buffer.

Real drivers use "dma_alloc_coherent()" We use "kmalloc()".

### Layer 1: Zero-Copy Bridge (remap_pfn_range)

After mmap(), both the SimulatorThread and TelemetryThread hold UVAs that the MMU translates directly to the same physical DRAM page — no kernel involvement, no copies, no syscalls on the data path.

The struct file only matters at setup and teardown. Once the PTEs exist, individual
loads and stores through the user pointer go through MMU to DRAM and never consult the fd, the struct file, or any kernel code. The file object is relevant at mmap() and at munmap()/close(). Closing the fd does not invalidate the pointer. Because the mapping is independent of the open file once installed, a process can close() the fd immediately after mmap() and keep using the pointer. The mapping lives in the address space (the VMA), not in the file.

After mmap() returns, both the SimulatorThread and the TelemetryThread hold UVAs that the MMU translates to the same physical DRAM frame.

### Layer 2: SimulatorThread (mock DMA engine, producer)

Runs in userspace. Writes "RoCEv2Header" structs into the shared page via its UVA. Increments the "sequence_number" last. This is the doorbell pattern: the sequence number is the signal that ecn_flag field are safe to read.

### Layer 3: TelemetryThread (busy poll engine, consumer)

The telemetry thread acts as the consumer and busy polls on "hdr->sequence_number" with a "volatile" pointer. Volatile is crucial here, because without it the compiler would read o, store it into a register and the loop would never detect the updates. The value is update by a different thread.

The thread never sleeps and always check for new packets.
The thread maintains a sliding window of recent packets in a RecentPacket deque
(currently 40 packets). The deque is always a bounded view of the most recent traffic.

Every 100 ms the thread evaluates: rate = window_congested / window_total.
If rate > 20%, it fires a critical alert.

### Layer 3.a: Why 100 ms and 20%?

Two scales are in play and it is worth naming them before any numbers appear,
because conflating them is what made earlier versions of this section inconsistent.

- **Real-world physics (400 Gbps, µs RTTs)** establishes *why* these constants
  were chosen. This is what you justify in an interview.
- **Demo parameterization (3k–12k pps, 10 ms ticks)** is *what the running code
  does* with those constants at rates a human can observe.

Both follow the same logical structure: the evaluation window must be far longer
than the time the congestion-control loop takes to react, and the threshold must
sit above what healthy control-loop oscillation produces. The argument is about
ratios. The scaling is only in the absolute magnitudes.

---

#### Part 1 — Real-world physics

##### Step 1 — Inter-packet time at line rate

At 400 Gbps with MTU-sized packets (1500 B = 12,000 bits):

```
packet_rate       = 400 × 10⁹ b/s ÷ 12,000 b/pkt  =  3.33 × 10⁷ pkt/s   (33.3 Mpps)
inter_packet_time = 1 ÷ 3.33 × 10⁷ pkt/s          ≈  30 ns / pkt
```

##### Step 2 — Bandwidth-delay product (BDP)

Intra-datacenter RTT = 10 µs. Packets in flight at any instant:

```
BDP_bytes   = 400 × 10⁹ b/s  ×  10 × 10⁻⁶ s  ÷  8 b/B  =  500,000 B
BDP_packets = 500,000 B  ÷  1,500 B/pkt                  ≈  333 packets in flight
```

The BDP is the **minimum congestion granularity**: when a packet is ECN-marked,
the sender does not know yet. Roughly one BDP worth of additional packets leaves
before the rate cut takes effect, so even a single brief congestion event
generates ~333 marked packets before the control loop can react.

##### Step 3 — DCQCN recovery time

This is where the common error of **2 × RTT** appears. The correct value is
**1.5 × RTT = 15 µs**. Walk the reaction chain step by step:

```
t = 0 µs      Sender transmits a packet. Switch ECN-marks it in transit.

t = RTT/2     Marked packet reaches the RECEIVER — one-way propagation is RTT/2,
  = 5 µs      not RTT. The RECEIVER (not the switch) generates the CNP.

t = RTT       CNP travels back to the sender — another one-way hop of RTT/2.
  = 10 µs     Sender receives the CNP and begins cutting its rate.

t ≈ 1.5×RTT   Queue drains and the sender rate stabilises — roughly one more RTT/2.
  ≈ 15 µs
```

**Why 2 × RTT is wrong.** The mistake is treating the CNP's return path as a full
RTT (10 µs) instead of a one-way hop (RTT/2 = 5 µs). Propagation from receiver
back to sender is RTT/2 by definition — RTT is the round trip, so each leg is
half. The corrected chain is three legs of RTT/2 each:

```
correct:     RTT/2 [to receiver]  +  RTT/2 [CNP to sender]  +  RTT/2 [settling]
           = 1.5 × RTT  =  15 µs

incorrect:   RTT [assumed round trip per leg]  ×  2  =  2 × RTT  =  20 µs   ← stale
```

The `config.h` comment used 20 µs (the stale figure). Corrected version is at the
end of this document.

##### Step 4 — Why 100 ms?

```
100 × 10⁻³ s  ÷  15 × 10⁻⁶ s  ≈  6,667 DCQCN recovery cycles per window
```

A 100 ms window spans ~6,667 complete control-loop reactions. A transient that
DCQCN clears in one or two cycles contributes a vanishing fraction of the window's
total packets to the marked count. Only congestion the loop **fails to drain** —
a queue that stays above the watermark across thousands of cycles — accumulates
enough marked packets to cross 20%.

100 ms is also fast on the human/orchestration timescale: the alert fires long
before the failure becomes an application-visible outage.

##### Step 5 — Why 20%?

A healthy DCQCN network is never at 0% marking — marking is the *mechanism* the
control loop uses to hold the queue near the watermark. Under load the system
oscillates: queue crosses watermark → marking starts → sender cuts rate → queue
drains below watermark → marking stops → rate climbs → repeat. Each marking event
is short-lived and self-correcting. Over 100 ms, the marked fraction stays in the
low single digits.

Contrast with a genuine failure: incast, misconfiguration, or a hot link holds the
queue *above* the watermark continuously. Marking never stops. The marked fraction
trends toward 100%.

20% sits in the gap between these two regimes — high enough that normal oscillation
never reaches it, low enough to catch a real event early. The gap is wide; 20% is
not a fine-tuned constant.

---

#### Part 2 — Demo parameterization

The demo runs the same queue-fills → marks → telemetry-observes logic at packet
rates slow enough to watch and control from a slider. Every constant below is
justified by the mechanics it produces.

---

##### The buffer and rate constants

**`DRAIN_RATE_PPS = 5000`** — the switch egress port capacity in packets per second.
This is the reference point: the sender must exceed 5,000 pps to fill the queue.
Below this, the queue drains regardless of what is in it.

**`DEFAULT_RATE_PPS = 3000`** — the sender's startup rate. It is deliberately *below*
drain (5,000) so the system boots healthy: the queue drains immediately, no ECN, no
alert. A congestion event requires the operator to raise the slider.

**`MAX_RATE_PPS = 12,000`** — the slider ceiling. At this rate the queue fills fast
enough to cross the ECN watermark within two ticks (derived below).

**`BUFFER_CAPACITY = 200` slots** — total queue depth. Arrivals beyond this are
tail-dropped (`dropping = true`).

**`RED_THRESHOLD_PCT = 70`** — the ECN watermark expressed as a fraction of capacity:

```
red_threshold = BUFFER_CAPACITY × RED_THRESHOLD_PCT / 100
              = 200 × 70 / 100
              = 140 slots
```

ECN marking (`ecn_active = true`) starts when `buffer_slots ≥ 140`. Tail-drop
starts at 200. The 60-slot gap between them is headroom for in-flight packets that
have already been marked but whose CNPs have not yet caused the sender to slow down
(one BDP ≈ 333 real packets; in the demo this headroom is proportional).

---

##### The tick and per-tick arithmetic

**`TICK_MS = 10`** (simulator.cpp local constant) — one simulated network timestep
of 10 ms. Time is discretized into ticks because a tight CPU loop has no intrinsic
notion of elapsed real time: you cannot compute "packets arrived per second" without
a denominator in seconds. Each tick provides that denominator.

Per-tick packet counts (`simulator.cpp`):

```c
pkts_this_tick  = max(1, rate × TICK_MS / 1000)   // sender arrivals this tick
drain_this_tick = DRAIN_RATE_PPS × TICK_MS / 1000  // switch drains this tick
                = 5000 × 10 / 1000
                = 50 packets / tick (constant)

net_fill = pkts_this_tick − drain_this_tick

buffer_slots = clamp(buffer_slots + net_fill, 0, 200)
```

At each rate setting:

```
rate =  3,000:  pkts =  30,  drain = 50,  net = −20 / tick  →  queue drains  →  no ECN
rate =  5,000:  pkts =  50,  drain = 50,  net =   0 / tick  →  queue stable
rate =  6,000:  pkts =  60,  drain = 50,  net = +10 / tick  →  queue fills
rate =  8,000:  pkts =  80,  drain = 50,  net = +30 / tick  →  queue fills faster
rate = 10,000:  pkts = 100,  drain = 50,  net = +50 / tick  →  faster still
rate = 12,000:  pkts = 120,  drain = 50,  net = +70 / tick  →  fastest
```

The default (3,000) is the only rate where the queue provably drains. Every rate
above 5,000 fills the queue; how fast depends on the net.

---

##### Time to first ECN from an empty queue

Starting from `buffer_slots = 0`, the queue reaches the ECN watermark (140 slots)
after `⌈140 / net_fill⌉` ticks:

```
rate =  6,000:  net = 10  →  ⌈140 / 10⌉ = 14 ticks  =  140 ms
rate =  8,000:  net = 30  →  ⌈140 / 30⌉ =  5 ticks  =   50 ms
rate = 10,000:  net = 50  →  ⌈140 / 50⌉ =  3 ticks  =   30 ms
rate = 12,000:  net = 70  →  ⌈140 / 70⌉ =  2 ticks  =   20 ms
```

At the slider maximum, the watermark is crossed in 20 ms — inside the first two
ticks of a 100 ms window. Eight remaining ticks all see ECN active. The congestion
rate for that window approaches 100%.

---

##### The telemetry window — what it counts

**`WINDOW_MS = 100`** — the evaluation period for the congestion rate, reset every
100 ms in `telemetry.cpp`. This spans approximately 10 simulator ticks.

The telemetry thread accumulates two counters (`telemetry.cpp`):

```cpp
uint32_t delta = cur - last_seq;          // packets simulator wrote since last poll

window_total     += delta;
window_congested += (is_ecn ? delta : 0);
```

`delta` is the number of descriptors published since the last poll. Because the
simulator writes `pkts_this_tick` descriptors per tick and then sleeps 10 ms, the
telemetry thread accumulates:

```
window_total ≈ Σ pkts_this_tick over ~10 ticks
             = 10 × (rate / 100)
             = rate / 10

rate =  3,000  →  ~300 packets per 100 ms window
rate =  6,000  →  ~600 packets per 100 ms window
rate = 12,000  → ~1,200 packets per 100 ms window
```

These are the real window packet counts: hundreds to low thousands. Not 30,000,
not 3.3 million.


## Agent Role Definition

You are operating as a low-level systems engineer on a kernel/userspace hybrid
codebase. Your job is to make specific targeted, well-understood changes with minimal
side effects. You are NOT a general-purpose refactoring agent. In this project,
scope and caution matter more than speed. We don't want to break anything.

This codebase has two fundamentally different risk domains:

   KERNEL SPACE (kernel_driver/mock_nic.c, kernel_driver/Makefile)
      Risk level: HIGH. Mistakes here can cause kernel-panic, leak physical
      memory at the OS level, or produce a module that silently maps the wrong
      physical page (silent data corruption). These failures are not always immediately visible, but can cause great faults in the future usage.

   USERSPACE (src/*.cpp, src/*.h)
      Risk level: REGULAR. Mistakes here produce compile errors, runtime crashes,
      or incorrect metrics output. Although all of these are recoverable without a reboot, and do not crash the system, they are unacceptable and should be carefully handled.

Treat these two domains differently at every step of your workflow with great manner.

## Authority Boundary — What You May Do Autonomously

The following actions are within your autonomous authority. You may execute
them without a human checkpoint:

   Reading any file in the project tree (subject to .claudeignore).
   Running the compiler (gcc, g++, make, cmake) to check for errors.
   Editing CMakeLists.txt and scripts/run_demo.sh.
   Running dmesg, lsmod, stat, cat /proc/* for diagnostic reads.

The following actions require a HUMAN CHECKPOINT before execution. Stop,
describe what you are about to do and why, and wait for explicit approval:

   Any edit to kernel_driver/mock_nic.c.
   Editing files in src/ (C++ sources and headers).
   Any edit to kernel_driver/Makefile.
   Running sudo insmod or sudo rmmod (in any form).
   Any change to the RoCEv2Header struct in src/rocev2_header.h.
      (Struct layout changes break the doorbell invariant)
   Any change that affects both kernel space and userspace simultaneously.
   Running run_demo.sh.

When you reach a checkpoint, output exactly this format:

  CHECKPOINT: <one sentence describing the action>
  REASON: <one sentence explaining why this action is necessary>
  RISK: <one sentence describing what could go wrong>
  WAITING FOR APPROVAL.

Do not proceed past a checkpoint until the human responds with an explicit
"approved", "go ahead", "yes", "Okay", "OK", "sure", "yeah", "ya" or equivalent.
Silence for any amount of time does not count.

## Standard Workflow for Common Task Types

### Task type: "Fix a bug in the userspace engine"

  1. Read the relevant source files first. Do not guess the bug from
     the description — take in to consideration, read the actual code, evaluate and respond.
  2. Identify the minimal change. Ask yourself: can this be fixed in one
     function without touching any interfaces? Do my changes effect any other code?
  3. Make the change in src/.
  4. Run the syntax check: g++ -std=c++17 -fsyntax-only -I./src src/main.cpp
     (adjust for the specific file). Fix any errors before proceeding.
  5. If the fix touches metrics or shared state, check that the mutex lock
     pattern is preserved, and make sure the changes don't cause potential dead locks or race conditions.
  6. Report the change and the syntax check result.

### Task type: "Add a new metric or telemetry field"

  1. Determine where the metric belongs: is it a per-packet value (goes in
     RoCEv2Header) or an aggregate value (goes in metrics.h)?
  2. If it goes in RoCEv2Header: STOP. This is a struct layout change.
     Issue a checkpoint before touching the file.
  3. If it goes in metrics.h: proceed autonomously, but verify that every
     new field is initialized in the constructor or in the reset path.
  4. Add the write path (in simulator.cpp or telemetry.cpp) and the read
     path (in http_server.cpp or metrics.cpp) in the same edit pass.
     A field that is written but not read, or vice versa, is incomplete.
  5. Run syntax check, and report result.

### Task type: "Change kernel module behavior"

  1. Read mock_nic.c in full before touching anything.
  2. Identify the exact function to change. Write out your planned change
     in a code block in your response — do NOT execute it yet.
  3. Issue a checkpoint (as described).
  4. After approval: make the edit. Do NOT run insmod yet.
  5. Run: make -C kernel_driver
     If it fails, fix the compile error and re-run. Do not attempt insmod
     with a module that produced compiler warnings under -Wall.
  6. Issue a second CHECKPOINT for the insmod step specifically.
  7. After approval: sudo insmod kernel_driver/mock_nic.ko
  8. Immediately run: dmesg | tail -15
     Verify: the PFN printed by KERN_INFO is non-zero
  9. Report the dmesg output. Done.

### Task type: "Debug a crash or unexpected behavior in the kernel"

  1. Start with dmesg — kernel messages often tell you exactly what happened.
  2. If the demo process crashed, check if the kernel module is still loaded:
     lsmod | grep mock_nic
     If it is, do NOT attempt to restart the demo immediately. Verify the
     physical page is still valid before re-mmap-ing it.
  3. Summarize your diagnosis as: OBSERVED SYMPTOM → ROOT CAUSE HYPOTHESIS
     → MINIMAL FIX. Present this before making any change.

## Escalation Criteria

Stop what you are doing and surface a question to the human when any of
these conditions are true. This is not an exhaustive list — use judgment.

  The fix to problem A requires changing interface X, which is also used
  by component Y that you have not analyzed yet.

  You are about to make a change that you cannot reverse without a reboot
  (e.g., a kernel module that panic'd on the last load attempt).

  The task description is ambiguous about which layer should change — kernel
  or userspace. Do not pick one and proceed silently.

  You have reached step 3 of a task and the codebase does not match what
  CLAUDE.md describes. Flag the discrepancy before continuing.

  A hook blocked a command. Do not attempt to work around the hook. Report
  what was blocked and why, and ask for guidance.

Format for escalation (not a checkpoint — you are not waiting for approval
on a specific action, you are surfacing a question):

  ESCALATION: <one sentence describing the ambiguity or blocker>
  CONTEXT: <one to three sentences of relevant facts>
  QUESTION: <the specific question you need answered>

## Code Conventions (Enforce These)

### Kernel module (mock_nic.c)

- All kernel code must compile cleanly with "-Wall -Werror" against the running kernel's headers.
- Never use `printk()` without a log level prefix ("KERN_INFO", "KERN_ERR", etc.).
- Any "kmalloc()" must be paired with "kfree()" in the "exit" function - no memory leaks.
- The char device must be unregistered in "module_exit" — cleanup order matters.

### Userspace C++ (integrated_demo.cpp)

- The "volatile" qualifier on "hdr" (the pointer to shared memory) is non-negotiable. Do not remove it or cast it away - it encodes a real hardware contract.
- The doorbell invariant: the simulator must write "sequence_number" last.
- Metrics under "g_metrics" are always protected by "metrics_mutex".

## What "Done" Means

A task is not complete until all of the following are true.

For userspace changes: the modified files compile cleanly under
-std=c++17 -Wall -Wextra with no errors or warnings. The change has been
described to the human with the specific files and functions modified.

For kernel module changes: the module compiles cleanly, loads without
KERN_ERR messages in dmesg, and the PFN printed on load is in a valid range.
The human has seen the dmesg output.

For any change: the doorbell invariant has been verified (sequence_number
is still the last field to write into RoCEv2Header, or the struct was not touched).
If check_doorbell_order.py exists in hooks, run it and show the output.

Do not summarize a task as complete and move on. End every task with a
structured completion report:

  COMPLETED: <task description>
  CHANGED: <files modified, with one-line description of each change>
  VERIFIED: <what was checked — compile result, dmesg output, hook output>
