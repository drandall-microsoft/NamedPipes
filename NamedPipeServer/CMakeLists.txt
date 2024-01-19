# CMakeList.txt : CMake project for NamedPipeServer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (NamedPipeServer)

# Add source to this project's executable.
add_executable (${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE "ServerMain.cpp" "src/CompletionServer.cpp" "inc/Options.h" "src/Options.cpp" "inc/ArgParse.h" "inc/StringUtils.h" "src/StringUtils.cpp" "src/Options.test.cpp" "inc/Options.test.h" "inc/OverlapServer.h" "src/OverlapServer.cpp" "inc/NamedPipeOstream.h" "src/NamedPipeOstream.cpp" "inc/MultithreadedServer.h" "src/MultithreadedServer.cpp" "inc/BaseNamedPipe.h" "inc/NamedPipeServer.h")
target_include_directories(${PROJECT_NAME} PUBLIC inc)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NamedPipeServer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
