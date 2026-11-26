# ============================================================================
# CMakeLists.txt for the BankingSystem project
#
# Build steps (from the BankingSystem folder):
#     cmake -S . -B build
#     cmake --build build
#     ./build/BankingSystem        (on Windows: build\Debug\BankingSystem.exe)
# ============================================================================

# Minimum CMake version that understands every command used below.
cmake_minimum_required(VERSION 3.16)

# Give the project a name. This also sets PROJECT_NAME and related variables.
# LANGUAGES CXX tells CMake we need only the C++ compiler.
project(BankingSystem
    VERSION 1.0.0
    DESCRIPTION "Multi-threaded banking system with ATM, loans and fraud detection"
    LANGUAGES CXX)

# ---- Language standard ------------------------------------------------------

# We use std::invoke_result_t, std::make_unique and structured bindings, all of
# which need C++17 at minimum.
set(CMAKE_CXX_STANDARD 17)

# Refuse to build with a compiler that cannot meet the standard, instead of
# silently falling back to an older dialect.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use strict ISO C++ rather than GNU extensions such as typeof.
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build type -------------------------------------------------------------

# If the user did not choose a build type on the command line, default to
# Release, which enables optimisation. Debug builds are still available with
# -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Show which build type is active in the configuration summary.
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ---- Options ----------------------------------------------------------------

# Options appear in the CMake GUI and can be set with -D on the command line.
option(BANKING_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BANKING_ENABLE_SANITIZERS "Build with AddressSanitizer and UBSan" OFF)

# ---- Threads ----------------------------------------------------------------

# Locate the platform's threading library (pthread on Linux and macOS,
# native threads on Windows). ThreadPool depends on it.
find_package(Threads REQUIRED)

# ---- Source files -----------------------------------------------------------

# Listing sources explicitly, rather than globbing, means CMake only rebuilds
# when a file in this list changes and new files are never picked up by accident.
set(BANKING_SOURCES
    main.cpp
    Account.cpp
    Customer.cpp
    Loan.cpp
    ATM.cpp
    FraudEngine.cpp
    ThreadPool.cpp
    Logger.cpp
    TransactionDatabase.cpp
)

# Headers are listed too. They do not get compiled directly, but listing them
# lets IDEs such as Visual Studio and CLion show them in the project tree.
set(BANKING_HEADERS
    Account.h
    Customer.h
    Loan.h
    ATM.h
    FraudEngine.h
    ThreadPool.h
    Logger.h
    TransactionDatabase.h
)

# ---- Executable -------------------------------------------------------------

# Create the executable target named BankingSystem from the sources above.
add_executable(BankingSystem ${BANKING_SOURCES} ${BANKING_HEADERS})

# Make the header folder visible to every source file. Here the headers sit in
# the same folder as the sources, so "." is the correct include directory.
target_include_directories(BankingSystem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link against the threading library detected earlier.
target_link_libraries(BankingSystem PRIVATE Threads::Threads)

# ---- Compiler warnings ------------------------------------------------------

# Compilers differ in their flags, so choose the set that matches each one.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(BankingSystem PRIVATE
        -Wall          # Common, useful warnings.
        -Wextra        # Even more warnings, for example unused parameters.
        -Wpedantic     # Reject code that is not strict ISO C++.
        -Wshadow       # Warn when a local variable hides a member or outer name.
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(BankingSystem PRIVATE /W4 /permissive-)
endif()

# Optionally make warnings fatal, which is useful on a continuous-integration server.
if(BANKING_WARNINGS_AS_ERRORS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(BankingSystem PRIVATE -Werror)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(BankingSystem PRIVATE /WX)
    endif()
endif()

# ---- Sanitizers -------------------------------------------------------------

# Sanitizers instrument the program to catch memory errors and undefined
# behaviour at run time. They are slow, so they are off by default.
if(BANKING_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(BankingSystem PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(BankingSystem PRIVATE -fsanitize=address,undefined)
    message(STATUS "Sanitizers enabled")
endif()

# ---- Installation -----------------------------------------------------------

# "cmake --install build" copies the executable to the chosen prefix,
# which defaults to /usr/local on Linux and macOS.
install(TARGETS BankingSystem
    RUNTIME DESTINATION bin
)

# ---- Summary ----------------------------------------------------------------

# Print a short report so the user can confirm what was configured.
message(STATUS "Project:       ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "C++ compiler:  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "Source files:  ${BANKING_SOURCES}")
