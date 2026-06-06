#pragma once
#include <cstdint>

#define ROCEV2_HEADER_SIZE 8

// =============================================================================
// rocev2_header.h — Shared Memory Layout: RoCEv2 Packet Descriptor
//
// This struct is the contract between the SimulatorThread (writer)
// and the TelemetryThread (reader). Both sides access the exact same physical
// DRAM bytes through different virtual addresses — so they must agree on the
// byte offset of every field.
//
// In a production RoCEv2 system, the NIC's DMA engine writes a full Base Transport Header, 12 bytes 
// =============================================================================


// Use of __attribute__((packed)) for precise control over struct layout, ensuring no padding bytes are added
// Could cause data corruption - writer writes at 4 bytes offset and reader reads at 8 bytes offset
struct __attribute__((packed)) RoCEv2_Header {

    uint32_t sequence_number;
 
    uint8_t  congestion_flag;

    // Explicit padding to align struct size to 8 bytes.
    // Without this, a ring buffer of N structs would have misaligned entries.
    uint8_t  padding[3];
};

// Compile-time assertion to ensure the struct is exactly 8 bytes 
static_assert(sizeof(RoCEv2_Header) == ROCEV2_HEADER_SIZE,
    "RoCEv2_Header must be exactly 8 bytes — layout mismatch will cause silent data corruption");
