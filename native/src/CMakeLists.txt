# manus_core: business rules and pure functions. No sockets, no environment, no Win32.
add_library(manus_core STATIC
  util/bounded_json.cpp
  util/js_number.cpp
  util/utf.cpp
  crypto/crypto.cpp
  auth/pairing.cpp
  config/config.cpp
  inject/jis_vk_map.cpp
  inject/command.cpp
  inject/keyboard.cpp
  inject/mouse.cpp
  inject/input_plan.cpp
  http/url.cpp
  http/api.cpp
  log/jsonl_record.cpp)
target_include_directories(manus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(manus_core PUBLIC nlohmann_json::nlohmann_json Lorica::boundary OpenSSL::Crypto)
manus_target(manus_core)

# manus_io: adapters for files, environment, logs and the OS input API.
# SendInput only exists on Windows; elsewhere the sink reports "helper_down".
set(MANUS_INPUT_SINK_SOURCE inject/unavailable_input_sink.cpp)
if(WIN32)
  set(MANUS_INPUT_SINK_SOURCE inject/win32_input_sink.cpp)
endif()
add_library(manus_io STATIC
  auth/pairing_store.cpp
  config/config_loader.cpp
  log/jsonl_logger.cpp
  platform/environment.cpp
  platform/file_read.cpp
  ${MANUS_INPUT_SINK_SOURCE})
target_link_libraries(manus_io PUBLIC manus_core)
if(WIN32)
  target_link_libraries(manus_io PUBLIC user32)
endif()
manus_target(manus_io)

# manus_transport: HTTP/1.1 + WebSocket listener (Boost.Beast).
add_library(manus_transport STATIC
  transport/server.cpp
  transport/http_connection.cpp
  transport/ws_connection.cpp)
target_link_libraries(manus_transport PUBLIC manus_core manus_boost)
manus_target(manus_transport)

# manus_app: use cases that wire the rules to the transport.
add_library(manus_app STATIC
  app/manus_app.cpp
  app/cli.cpp)
target_link_libraries(manus_app PUBLIC manus_core manus_io manus_transport)
manus_target(manus_app)

add_executable(manus app/main.cpp)
target_link_libraries(manus PRIVATE manus_app)
set_target_properties(manus PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")
manus_target(manus)
install(TARGETS manus RUNTIME DESTINATION bin)
