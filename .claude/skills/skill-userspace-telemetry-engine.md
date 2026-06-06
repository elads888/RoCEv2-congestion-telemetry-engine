# Skill: Userspace Telemetry Engine (RoCEv2 context)

## What this skill covers

This skill encodes the conventions specific to the C++ userspace engine
under "src/" — the process that maps the kernel module's shared page and
runs the congestion-telemetry logic. Read this before making any edit to
anything in "src/".

This is the consumer side of the contract that "kernel_driver/mock_nic.c" produces.

## The engine's contract with the kernel module

The engine depends on exactly the three promises "mock_nic.c" makes
("/dev/mock_nic" exists; one "mmap()" yields a 4 KB physically-contiguous
page; "rmmod" tears it down). In return, the engine must:

1. "open("/dev/mock_nic", O_RDWR)" as root, then
   "mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)".
2. Treat the returned pointer as "volatile" for its entire lifetime.
3. Map exactly 4096 bytes. Never assume the page is zeroed, larger than
   4 KB, or stable across "rmmod".

If the engine maps more than 4096 bytes or drops "volatile", it will read
stale or garbage data **without any error surfacing** - "mmap" succeeds,
the demo "runs," and the telemetry is wrong.

## The shared-memory pointer must be volatile

"volatile" is **mandatory**, not optional.

Without it, the compiler leaves the load of "sequence_number" out of the
busy poll loop and the loop spins forever on a cached value - the
telemetry thread freezes while the simulator keeps writing.

## RoCEv2Header layout — the doorbell invariant

"sequence_number" MUST be the **last** field of "RoCEv2Header".

The writer fills every other field of the packet first, then writes
"sequence_number" last - that final store is the doorbell. The reader
polls only "sequence_number" - when it observes a new value it treats the
rest of the header as fully published.

Rules for any edit touching the header or the write path:
- Never reorder the writes so "sequence_number" is published before the
  payload. Do not "optimize" that store.

## Thread model

Three threads, all in **userspace** (the simulator is *not* a kernel
context):

- "SimulatorThread" - writes mock RoCEv2 packets into the shared page and
  rings the doorbell via "sequence_number". Its "congestion_flag" must be
  driven by actual buffer/queue state, not by an independent
  free-running counter.
- "TelemetryThread" - busy polls the "sequence_number". On
  each new packet it pushes a "RecentPacket" onto a deque capped at **40
  entries** and recomputes the ECN-marked ratio over that window.
- HTTP server thread — serves the telemetry.
