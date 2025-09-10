#pragma once

#include <cstdint>
#include <cstddef>
#include <immintrin.h>  // AVX/AVX2
#include <arm_neon.h>   // ARM NEON (when available)

namespace Monitor {
namespace Packet {

/**
 * @brief SIMD-optimized packet processing utilities
 */
class SIMDPacketProcessor {
public:
    /**
     * @brief Extract multiple 32-bit fields simultaneously using SIMD
     * @param packet Source packet data
     * @param offsets Array of byte offsets for each field
     * @param results Output array for extracted values
     * @param count Number of fields to extract (must be 4, 8, or 16)
     */
    static void extractFields32(const uint8_t* packet, const uint32_t* offsets, 
                               uint32_t* results, size_t count) noexcept;
    
    /**
     * @brief Vectorized packet validation
     * @param packets Array of packet pointers
     * @param count Number of packets to validate
     * @param results Bit vector of validation results
     */
    static void validatePacketsBatch(const uint8_t** packets, size_t count, 
                                   uint64_t* results) noexcept;
    
    /**
     * @brief Fast packet header parsing using SIMD
     * @param data Packet data
     * @param header Output header structure
     */
    static bool parseHeaderFast(const uint8_t* data, PacketHeader* header) noexcept;
    
    /**
     * @brief Parallel checksum calculation
     * @param data Input data
     * @param size Data size
     * @return Calculated checksum
     */
    static uint32_t calculateChecksumSIMD(const uint8_t* data, size_t size) noexcept;
    
    /**
     * @brief Vectorized memory compare for packet matching
     * @param a First packet data
     * @param b Second packet data  
     * @param size Comparison size
     * @return true if equal
     */
    static bool comparePacketsSIMD(const uint8_t* a, const uint8_t* b, size_t size) noexcept;

private:
    // Architecture-specific implementations
    #if defined(__AVX2__)
    static void extractFields32_AVX2(const uint8_t* packet, const uint32_t* offsets,
                                    uint32_t* results, size_t count) noexcept;
    #endif
    
    #if defined(__ARM_NEON) || defined(__aarch64__)
    static void extractFields32_NEON(const uint8_t* packet, const uint32_t* offsets,
                                    uint32_t* results, size_t count) noexcept;
    #endif
    
    // Fallback scalar implementation
    static void extractFields32_Scalar(const uint8_t* packet, const uint32_t* offsets,
                                      uint32_t* results, size_t count) noexcept;
};

/**
 * @brief Cache-optimized packet batch processor
 */
class BatchPacketProcessor {
public:
    static constexpr size_t OPTIMAL_BATCH_SIZE = 64;  // Cache-friendly batch size
    
    /**
     * @brief Process packets in optimally-sized batches
     * @param packets Input packet array
     * @param count Number of packets
     * @param processor Processing function
     */
    template<typename Processor>
    static void processBatches(const Packet** packets, size_t count, Processor processor) {
        const size_t fullBatches = count / OPTIMAL_BATCH_SIZE;
        const size_t remainder = count % OPTIMAL_BATCH_SIZE;
        
        // Process full batches
        for (size_t i = 0; i < fullBatches; ++i) {
            processor(packets + i * OPTIMAL_BATCH_SIZE, OPTIMAL_BATCH_SIZE);
        }
        
        // Process remaining packets
        if (remainder > 0) {
            processor(packets + fullBatches * OPTIMAL_BATCH_SIZE, remainder);
        }
    }
    
    /**
     * @brief Parallel packet processing using multiple threads
     * @param packets Input packets
     * @param count Number of packets
     * @param numThreads Number of processing threads
     * @param processor Processing function
     */
    template<typename Processor>
    static void processParallel(const Packet** packets, size_t count, 
                               size_t numThreads, Processor processor);
};

/**
 * @brief Template specializations for common packet types
 */
template<>
struct PacketProcessor<SignalTestPacket> {
    static void extractFieldsSIMD(const SignalTestPacket* packet, float* results) {
        // Optimized extraction for signal test packets
        #if defined(__AVX__)
        __m128 values = _mm_loadu_ps(&packet->sine_wave);
        _mm_storeu_ps(results, values);
        #else
        results[0] = packet->sine_wave;
        results[1] = packet->cosine_wave;  
        results[2] = packet->ramp;
        results[3] = static_cast<float>(packet->counter);
        #endif
    }
};

} // namespace Packet
} // namespace Monitor