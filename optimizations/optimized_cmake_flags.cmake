# Optimized CMake configuration for Phase 12: Optimization & Release
# Monitor Application Performance Optimizations

# Performance-focused build configuration
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Applying performance optimizations for Release build")
    
    # Aggressive optimization flags
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        # Clang/Apple Clang optimizations
        add_compile_options(
            -O3                          # Maximum optimization
            -DNDEBUG                     # Remove debug assertions
            -flto                        # Link-time optimization
            -march=native                # Optimize for current CPU
            -mtune=native                # Tune for current CPU
            -ffast-math                  # Fast math operations
            -funroll-loops               # Unroll loops
            -fvectorize                  # Enable auto-vectorization
            -fslp-vectorize             # Enable SLP vectorization
            -finline-functions           # Inline functions aggressively
            -fomit-frame-pointer        # Omit frame pointer when possible
        )
        
        # Linker optimizations
        add_link_options(
            -flto                        # Link-time optimization
            -Wl,-dead_strip             # Remove dead code (macOS)
            -Wl,-dead_strip_dylibs     # Remove unused dylibs (macOS)
        )
        
        # Architecture-specific optimizations
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            message(STATUS "Enabling ARM64/Apple Silicon optimizations")
            add_compile_options(-mcpu=apple-a14)  # Apple Silicon optimization
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
            message(STATUS "Enabling x86_64 optimizations")
            add_compile_options(
                -msse4.2                 # SSE 4.2 instructions
                -mavx2                   # AVX2 instructions (if available)
                -mfma                    # FMA instructions
            )
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC optimizations
        add_compile_options(
            -O3 -DNDEBUG -flto -march=native -mtune=native
            -ffast-math -funroll-loops -finline-functions
            -fomit-frame-pointer -ftree-vectorize
        )
        add_link_options(-flto -Wl,--gc-sections)
        
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # MSVC optimizations
        add_compile_options(
            /O2                          # Maximum optimization
            /Ob2                         # Inline any suitable function
            /Oi                          # Enable intrinsic functions
            /Ot                          # Favor fast code
            /Oy                          # Omit frame pointers
            /GL                          # Whole program optimization
            /DNDEBUG                     # Remove debug assertions
        )
        add_link_options(/LTCG)         # Link-time code generation
    endif()
    
    # Template instantiation optimization
    add_compile_definitions(
        EXPLICIT_TEMPLATE_INSTANTIATION=1
        OPTIMIZE_FOR_RELEASE=1
    )
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - enabling debugging and sanitizers")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        add_compile_options(
            -g -O0                       # Debug info, no optimization
            -fsanitize=address           # Address sanitizer
            -fsanitize=undefined         # Undefined behavior sanitizer
            -fno-omit-frame-pointer     # Keep frame pointer for debugging
        )
        add_link_options(
            -fsanitize=address
            -fsanitize=undefined
        )
    endif()
endif()

# Memory optimization flags
add_compile_definitions(
    QT_NO_DEBUG_OUTPUT=1             # Remove Qt debug output in release
    QT_NO_INFO_OUTPUT=1              # Remove Qt info output in release  
    QT_DISABLE_DEPRECATED_BEFORE=0x060000  # Disable deprecated Qt features
)

# SIMD support detection and enabling
include(CheckCXXSourceCompiles)

# Check for AVX2 support
check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256i a = _mm256_setzero_si256();
        return 0;
    }
" HAVE_AVX2)

if(HAVE_AVX2)
    message(STATUS "AVX2 support detected - enabling SIMD optimizations")
    add_compile_definitions(HAVE_AVX2=1)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(-mavx2 -mfma)
    endif()
endif()

# Check for ARM NEON support
check_cxx_source_compiles("
    #include <arm_neon.h>
    int main() {
        float32x4_t a = vdupq_n_f32(0.0f);
        return 0;
    }
" HAVE_NEON)

if(HAVE_NEON)
    message(STATUS "ARM NEON support detected - enabling SIMD optimizations")
    add_compile_definitions(HAVE_NEON=1)
endif()

# Profile-Guided Optimization setup (advanced)
option(ENABLE_PGO "Enable Profile-Guided Optimization" OFF)
if(ENABLE_PGO)
    message(STATUS "Profile-Guided Optimization enabled")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        # PGO with Clang
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-instr-generate")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fprofile-instr-generate")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # PGO with GCC
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fprofile-generate")
    endif()
endif()

# Custom optimization function for specific files
function(optimize_file TARGET FILE)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_source_files_properties(${FILE} PROPERTIES
            COMPILE_FLAGS "-O3 -finline-functions -funroll-loops"
        )
    endif()
endfunction()

# Optimization for critical hot-path files
macro(apply_hot_path_optimizations TARGET)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Apply aggressive optimizations to performance-critical files
        optimize_file(${TARGET} "src/memory/memory_pool.cpp")
        optimize_file(${TARGET} "src/packet/core/packet.cpp") 
        optimize_file(${TARGET} "src/threading/thread_pool.cpp")
        optimize_file(${TARGET} "src/concurrent/mpsc_ring_buffer.h")
        optimize_file(${TARGET} "src/offline/sources/file_indexer.cpp")
        
        message(STATUS "Applied hot-path optimizations to ${TARGET}")
    endif()
endmacro()

# Binary size optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Strip unnecessary symbols
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_link_options(-Wl,-dead_strip -Wl,-dead_strip_dylibs)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_link_options(-Wl,--gc-sections -Wl,--strip-all)
    endif()
    
    # Template instantiation control
    add_compile_definitions(
        EXPLICIT_TEMPLATE_INSTANTIATION=1
        QT_NO_TEMPLATE_FRIEND=1
    )
endif()

message(STATUS "Performance optimization configuration complete")