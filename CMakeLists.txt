cmake_minimum_required(VERSION 3.16)
project(sentio_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

## Dropping SQLite linkage; file-based trade books will be used

add_library(sentio_common
    src/common/types.cpp
    src/common/utils.cpp
)

add_library(sentio_strategy
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/sigor_config.cpp
    src/strategy/sigor_strategy.cpp
    src/strategy/momentum_scalper.cpp
)
target_link_libraries(sentio_strategy PRIVATE sentio_common)

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

add_executable(sentio_cli src/cli/sentio_cli_main.cpp)
target_link_libraries(sentio_cli PRIVATE sentio_backend sentio_strategy sentio_common)

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


