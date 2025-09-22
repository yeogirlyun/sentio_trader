cmake_minimum_required(VERSION 3.16)
project(sentio_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find LibTorch for GRU strategy support (optional)
find_package(Torch QUIET)
if(Torch_FOUND)
    message(STATUS "LibTorch found - GRU strategy support enabled")
    set(TORCH_AVAILABLE TRUE)
else()
    message(STATUS "LibTorch not found - GRU strategy support disabled")
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
)

# Strategy library with conditional GRU support
set(STRATEGY_SOURCES
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/sigor_config.cpp
    src/strategy/sigor_strategy.cpp
    src/strategy/momentum_scalper.cpp
)

# Add GRU strategy if LibTorch is available
if(TORCH_AVAILABLE)
    list(APPEND STRATEGY_SOURCES src/strategy/gru_strategy.cpp)
    message(STATUS "Including GRU strategy in build")
endif()

add_library(sentio_strategy ${STRATEGY_SOURCES})
target_link_libraries(sentio_strategy PRIVATE sentio_common)

# Link LibTorch if available
if(TORCH_AVAILABLE)
    target_link_libraries(sentio_strategy PRIVATE ${TORCH_LIBRARIES})
    target_compile_definitions(sentio_strategy PRIVATE TORCH_AVAILABLE)
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
target_link_libraries(sentio_cli PRIVATE sentio_backend sentio_strategy sentio_common)

# Primary CLI now uses clean command pattern architecture (77 lines vs 1,382 lines)

add_executable(audit src/cli/audit_main.cpp)
target_link_libraries(audit PRIVATE sentio_backend sentio_common)

add_executable(psm_demo src/cli/psm_demo.cpp)
target_link_libraries(psm_demo PRIVATE sentio_backend sentio_strategy sentio_common)

# -----------------------------------------------------------------------------
# Leverage Data Generation Tool
# -----------------------------------------------------------------------------
add_executable(generate_leverage_data
    tools/generate_leverage_data.cpp
)
target_link_libraries(generate_leverage_data PRIVATE sentio_common)


