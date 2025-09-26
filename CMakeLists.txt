cmake_minimum_required(VERSION 3.16)
project(sentio_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Performance optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling maximum performance optimizations for Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ffast-math -funroll-loops -flto -DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    add_compile_definitions(NDEBUG)
    
    # Enable OpenMP for parallel processing if available
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found - enabling parallel processing")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fopenmp")
    endif()
    
    # Additional performance flags for ML inference
    # Note: Keeping RTTI and exceptions enabled for LibTorch compatibility
    message(STATUS "Applied optimization flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find LibTorch for ML strategy support (Transformer, GRU)
# Use Python PyTorch installation which has macOS-compatible libraries
set(CMAKE_PREFIX_PATH "/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch" ${CMAKE_PREFIX_PATH})

find_package(Torch QUIET)
if(Torch_FOUND)
    message(STATUS "LibTorch found - ML strategy support enabled")
    message(STATUS "LibTorch version: ${Torch_VERSION}")
    
       # Use macOS-compatible libraries from Python PyTorch installation
       set(TORCH_LIBRARIES 
           "/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch/lib/libtorch.dylib"
           "/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch/lib/libc10.dylib"
           "/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch/lib/libtorch_cpu.dylib"
       )
       message(STATUS "LibTorch libraries: ${TORCH_LIBRARIES}")
       
       # Set global include directories for torch headers
       include_directories("/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch/include")
       include_directories("/Users/yeogirlyun/Library/Python/3.13/lib/python/site-packages/torch/include/torch/csrc/api/include")
    
    set(TORCH_AVAILABLE TRUE)
else()
    message(STATUS "LibTorch not found - ML strategy support disabled")
    message(STATUS "Make sure PyTorch 2.6.0 is installed in Python environment")
    set(TORCH_AVAILABLE FALSE)
endif()

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found - using header-only fallback")
endif()

## Dropping SQLite linkage; file-based trade books will be used

add_library(sentio_common
    src/common/types.cpp
    src/common/utils.cpp
    src/common/json_utils.cpp
    src/common/trade_event.cpp
    src/common/binary_data.cpp
)

# Strategy library with conditional GRU support
set(STRATEGY_SOURCES
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/sigor_config.cpp
    src/strategy/sigor_strategy.cpp
    src/strategy/momentum_scalper.cpp
    src/strategy/ml_strategy_base.cpp
)

# Add feature engines (always available) - 🔧 CONSOLIDATED: Single unified 91-feature engine
list(APPEND STRATEGY_SOURCES src/features/unified_feature_engine.cpp)

# Add ML strategies if LibTorch is available
if(TORCH_AVAILABLE)
    list(APPEND STRATEGY_SOURCES src/strategy/cpp_ppo_strategy.cpp)
    list(APPEND STRATEGY_SOURCES src/strategy/transformer_strategy.cpp)
    list(APPEND STRATEGY_SOURCES src/strategy/transformer_model.cpp)
    message(STATUS "Including ML strategies (CppPPO + Transformer) in build")
    add_compile_definitions(TORCH_AVAILABLE)
endif()

# ML library and training components
set(ML_SOURCES)
set(TRAINING_SOURCES)

if(TORCH_AVAILABLE)
    list(APPEND TRAINING_SOURCES src/training/cpp_ppo_trainer.cpp)
    list(APPEND TRAINING_SOURCES src/training/quality_enforced_loss.cpp)
endif()

add_library(sentio_strategy ${STRATEGY_SOURCES})
target_link_libraries(sentio_strategy PRIVATE sentio_common)

# Link OpenMP if available for performance optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND OpenMP_CXX_FOUND)
    target_link_libraries(sentio_strategy PRIVATE OpenMP::OpenMP_CXX)
endif()

# Create ML library if LibTorch is available
if(TORCH_AVAILABLE AND ML_SOURCES)
    add_library(sentio_ml ${ML_SOURCES})
    target_link_libraries(sentio_ml PRIVATE sentio_common ${TORCH_LIBRARIES})
    target_include_directories(sentio_ml PRIVATE ${TORCH_INCLUDE_DIRS})
endif()

# Create training library if LibTorch is available and we have training sources
if(TORCH_AVAILABLE AND TRAINING_SOURCES)
    add_library(sentio_training ${TRAINING_SOURCES})
    target_link_libraries(sentio_training PRIVATE sentio_common ${TORCH_LIBRARIES})
    target_include_directories(sentio_training PRIVATE ${TORCH_INCLUDE_DIRS})
    
    # Link with ML library if it exists
    if(TARGET sentio_ml)
        target_link_libraries(sentio_training PRIVATE sentio_ml)
    endif()
    
    # Add nlohmann/json if available
    if(nlohmann_json_FOUND)
        target_link_libraries(sentio_training PRIVATE nlohmann_json::nlohmann_json)
    endif()
endif()

# Link LibTorch if available
if(TORCH_AVAILABLE)
    target_link_libraries(sentio_strategy PRIVATE ${TORCH_LIBRARIES})
    target_compile_definitions(sentio_strategy PRIVATE TORCH_AVAILABLE)
    target_include_directories(sentio_strategy PRIVATE ${TORCH_INCLUDE_DIRS})
    
    # Link ML library to strategy library
    if(TARGET sentio_ml)
        target_link_libraries(sentio_strategy PRIVATE sentio_ml)
    endif()
endif()

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(sentio_strategy PRIVATE nlohmann_json::nlohmann_json)
endif()

add_library(sentio_backend
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
    src/backend/audit_component.cpp
    src/backend/leverage_manager.cpp
    src/backend/adaptive_portfolio_manager.cpp
    src/backend/adaptive_trading_mechanism.cpp
    src/backend/position_state_machine.cpp
)
target_link_libraries(sentio_backend PRIVATE sentio_common)

add_executable(sentio_cli 
    src/cli/sentio_cli_main.cpp
    src/cli/command_interface.cpp
    src/cli/strattest_command.cpp
    src/cli/trade_command.cpp
    src/cli/audit_command.cpp
)
# Link only direct dependencies - transitive dependencies will be resolved automatically
target_link_libraries(sentio_cli PRIVATE sentio_backend sentio_strategy)

# LibTorch and compile definitions are already linked through sentio_strategy
if(TORCH_AVAILABLE)
    target_compile_definitions(sentio_cli PRIVATE TORCH_AVAILABLE)
endif()

# Primary CLI now uses clean command pattern architecture (77 lines vs 1,382 lines)

add_executable(audit src/cli/audit_main.cpp)
target_link_libraries(audit PRIVATE sentio_backend)

# C++ ML Trainer executables (only if LibTorch is available)
if(TORCH_AVAILABLE AND TARGET sentio_training)
    add_executable(cpp_ppo_trainer src/cli/cpp_ppo_trainer_main.cpp)
    target_link_libraries(cpp_ppo_trainer PRIVATE sentio_training sentio_strategy)
    target_include_directories(cpp_ppo_trainer PRIVATE ${TORCH_INCLUDE_DIRS})
    target_compile_definitions(cpp_ppo_trainer PRIVATE TORCH_AVAILABLE)
    message(STATUS "C++ PPO Trainer executable will be built")
endif()

# TFM Trainer executable (Production Version - only if LibTorch is available)
if(TORCH_AVAILABLE)
    
    
    # Signal Quality Validator executable
    add_executable(signal_quality_validator src/training/signal_quality_validator.cpp)
    target_link_libraries(signal_quality_validator PRIVATE sentio_strategy sentio_common ${TORCH_LIBRARIES})
    target_include_directories(signal_quality_validator PRIVATE ${TORCH_INCLUDE_DIRS})
    target_compile_definitions(signal_quality_validator PRIVATE TORCH_AVAILABLE)
    
    # Add nlohmann/json if available
    if(nlohmann_json_FOUND)
        target_link_libraries(signal_quality_validator PRIVATE nlohmann_json::nlohmann_json)
    endif()
    
    
    


# Data Preprocessing Utility (Binary Data Pipeline)
add_executable(preprocess_data src/cli/preprocess_data.cpp)
target_link_libraries(preprocess_data PRIVATE sentio_strategy sentio_common sentio_training ${TORCH_LIBRARIES})
target_include_directories(preprocess_data PRIVATE ${TORCH_INCLUDE_DIRS})
target_compile_definitions(preprocess_data PRIVATE TORCH_AVAILABLE)

# TFM Efficient Trainer (Uses Pre-processed Binary Data)
add_executable(tfm_trainer_efficient src/cli/tfm_trainer_efficient.cpp)
target_link_libraries(tfm_trainer_efficient PRIVATE sentio_strategy sentio_common sentio_training ${TORCH_LIBRARIES})
target_include_directories(tfm_trainer_efficient PRIVATE ${TORCH_INCLUDE_DIRS})
target_compile_definitions(tfm_trainer_efficient PRIVATE TORCH_AVAILABLE)
    
    message(STATUS "TFM Signal Quality Validator, Data Preprocessor, and Efficient Trainer will be built")
endif()

add_executable(psm_demo src/cli/psm_demo.cpp)
target_link_libraries(psm_demo PRIVATE sentio_backend sentio_strategy)

# -----------------------------------------------------------------------------
# Leverage Data Generation Tool
# -----------------------------------------------------------------------------
add_executable(generate_leverage_data tools/generate_leverage_data.cpp)
target_link_libraries(generate_leverage_data PRIVATE sentio_common)

# -----------------------------------------------------------------------------
# CSV to Binary Converter Tool
# -----------------------------------------------------------------------------
add_executable(csv_to_binary_converter tools/csv_to_binary_converter.cpp)
target_link_libraries(csv_to_binary_converter PRIVATE sentio_common)

# -----------------------------------------------------------------------------
# Dataset Analysis Tool
# -----------------------------------------------------------------------------
add_executable(analyze_dataset src/tools/analyze_dataset.cpp)
target_link_libraries(analyze_dataset PRIVATE sentio_common)


# -----------------------------------------------------------------------------
# Sequence Diversity Validation Tool
# -----------------------------------------------------------------------------
add_executable(validate_sequence_diversity src/validate_sequence_diversity.cpp)
target_link_libraries(validate_sequence_diversity PRIVATE sentio_strategy sentio_common ${TORCH_LIBRARIES})
target_include_directories(validate_sequence_diversity PRIVATE ${TORCH_INCLUDE_DIRS})
target_compile_definitions(validate_sequence_diversity PRIVATE TORCH_AVAILABLE)




