cmake_minimum_required(VERSION 3.13)

set(PROJECT_NAME libiomgr)
PROJECT(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 11)
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES C CXX)

find_package(GTest REQUIRED)
find_package(glog REQUIRED)
find_package(Threads REQUIRED)

set(LIBIOMGR_PUBLIC_INCLUDE_DIR "include/iomgr")

include_directories(
  "${PROJECT_SOURCE_DIR}/include"
  "."
)
include_directories(SYSTEM ${GTEST_INCLUDE_DIRS})
include_directories(SYSTEM ${GLOG_INCLUDE_DIRS})

add_library(${PROJECT_NAME} "")
target_sources(${PROJECT_NAME}
  PUBLIC
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/export.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/io_buffer.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/io_watcher.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/ref_counted.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/slice.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/status.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/statusor.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/tcp/inet_address.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/tcp/tcp_client.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/tcp/tcp_server.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/http_types.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/http_request.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/http_response.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/http_client.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/http_server.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/http/uri_parser.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/time.h"
  "${LIBIOMGR_PUBLIC_INCLUDE_DIR}/timer.h"
  PRIVATE
  "io/io_buffer.cc"
  "io/io_manager.h"
  "io/io_manager.cc"
  "io/io_poller.h"
  "io/io_poller.cc"
  "io/io_watcher.cc"
  "io/tcp_client_impl.h"
  "io/tcp_client_impl.cc"
  "io/tcp_server_impl.h"
  "io/tcp_server_impl.cc"
  "io/http_request.cc"
  "io/http_response.cc"
  "io/http_client.cc"
  "io/http_server.cc"
  "io/test/async_test_callback"
  "threading/task_handle.h"
  "threading/task_handle.cc"
  "threading/task_runner.h"
  "threading/task_runner.cc"
  "threading/task_runner_task.h"
  "threading/task_runner_task.cc"
  "threading/thread.h"
  "threading/thread.cc"
  "timer/time.cc"
  "timer/timer.cc"
  "timer/timer_heap.h"
  "timer/timer_heap.cc"
  "timer/timer_manager.h"
  "timer/timer_manager.cc"
  "util/averaged_stats.h"
  "util/averaged_stats.cc"
  "util/file_op.h"
  "util/file_op.cc"
  "util/http_parser.h"
  "util/http_parser.cc"
  "util/inet_address.cc"
  "util/notification.h"
  "util/notification.cc"
  "util/os_error.h"
  "util/os_error.cc"
  "util/pointer_hash.h"
  "util/scoped_fd.h"
  "util/sockaddr_storage.h"
  "util/sockaddr_storage.cc"
  "util/socket_op.h"
  "util/socket_op.cc"
  "util/status.cc"
  "util/sync.h"
  "util/uri_parser.cc"
)
target_link_libraries(${PROJECT_NAME} Threads::Threads glog::glog)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()
function(libiomgr_test test_file)
  get_filename_component(test_target_name "${test_file}" NAME_WE)
  add_executable("${test_target_name}" "")
  target_sources("${test_target_name}"
    PRIVATE
      "${test_file}"
  )
  target_link_libraries("${test_target_name}" ${PROJECT_NAME} gtest gmock glog::glog)
  add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
endfunction(libiomgr_test)

libiomgr_test("io/io_manager_test.cc")
libiomgr_test("io/io_poller_test.cc")
libiomgr_test("io/io_watcher_test.cc")
libiomgr_test("io/tcp_client_impl_test.cc")
libiomgr_test("io/tcp_server_impl_test.cc")
libiomgr_test("io/http_request_test.cc")
libiomgr_test("io/http_response_test.cc")
libiomgr_test("io/http_server_test.cc")
libiomgr_test("threading/task_runner_test.cc")
libiomgr_test("timer/time_test.cc")
libiomgr_test("timer/timer_heap_test.cc")
libiomgr_test("timer/timer_manager_test.cc")
libiomgr_test("threading/thread_test.cc")
libiomgr_test("util/averaged_stats_test.cc")
libiomgr_test("util/file_op_test.cc")
libiomgr_test("util/http_parser_test.cc")
libiomgr_test("util/notification_test.cc")
libiomgr_test("util/ref_counted_test.cc")
libiomgr_test("util/scoped_fd_test.cc")
libiomgr_test("util/sockaddr_storage_test.cc")
libiomgr_test("util/status_test.cc")
libiomgr_test("util/statusor_test.cc")
libiomgr_test("util/uri_parser_test.cc")

#### Example ###
libiomgr_test("example/tcp/server.cc")
libiomgr_test("example/tcp/client.cc")