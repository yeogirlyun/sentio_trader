cmake_minimum_required(VERSION 3.16)
project(sentio_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(sentio_common
    src/common/types.cpp
    src/common/utils.cpp
)

add_library(sentio_strategy
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
)
target_link_libraries(sentio_strategy PRIVATE sentio_common)

add_library(sentio_backend
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
    src/backend/audit_component.cpp
)
target_link_libraries(sentio_backend PRIVATE sentio_common)


