# LLVM Clang toolchain for macOS
# Requires:
#   - LLVM installed at /opt/homebrew/opt/llvm (for compiler)
#   - Custom-built libc++ at ~/dv/llvm-libs (for shared library with correct deployment target)
#
# Build libc++ with: ./build-libcxx.sh
#
# IMPORTANT: You must bundle the libc++ dylibs with your application:
#   ~/dv/llvm-libs/lib/libc++.1.dylib
#   ~/dv/llvm-libs/lib/libc++abi.1.dylib

set(LLVM_PREFIX "/opt/homebrew/opt/llvm")
set(LIBCXX_PREFIX "$ENV{HOME}/dv/llvm-libs")

set(CMAKE_C_COMPILER   "${LLVM_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")

# Use Apple's ar and ranlib to create archives compatible with Apple's linker
# (LLVM's ar creates archives that Apple's ld can't read for universal builds)
set(CMAKE_AR "/usr/bin/ar")
set(CMAKE_RANLIB "/usr/bin/ranlib")

# Use custom-built libc++ headers (built with correct deployment target)
# -nostdinc++ removes default C++ include paths
# -isystem adds our custom headers
set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -isystem ${LIBCXX_PREFIX}/include/c++/v1")

# Dynamically link our custom-built libc++
# -nostdlib++ prevents automatic linking of the system libc++
# -L adds our library path
# -lc++ -lc++abi links our custom libraries
# -rpath tells the runtime linker where to find the libraries:
#   - ${LIBCXX_PREFIX}/lib: for development (finds libs at build location)
#   - @loader_path: for deployment (finds libs next to the binary)
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-nostdlib++ -L${LIBCXX_PREFIX}/lib -lc++ -lc++abi -Wl,-rpath,${LIBCXX_PREFIX}/lib -Wl,-rpath,@loader_path")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-nostdlib++ -L${LIBCXX_PREFIX}/lib -lc++ -lc++abi -Wl,-rpath,${LIBCXX_PREFIX}/lib -Wl,-rpath,@loader_path")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-nostdlib++ -L${LIBCXX_PREFIX}/lib -lc++ -lc++abi -Wl,-rpath,${LIBCXX_PREFIX}/lib -Wl,-rpath,@loader_path")

# Minimum macOS version
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")

# Get the macOS SDK path
execute_process(
	COMMAND xcrun --sdk macosx --show-sdk-path
	OUTPUT_VARIABLE MACOSX_SDK_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_OSX_SYSROOT "${MACOSX_SDK_PATH}")
