cmake_minimum_required (VERSION 3.11)
project (test_project)

set(CMAKE_CXX_STANDARD 17)

if (MSVC)
	add_definitions(-DBOOST_ALL_NO_LIB)
endif()

find_package(Boost 1.68	REQUIRED COMPONENTS system)
find_package(wxWidgets REQUIRED COMPONENTS core base)

include(${wxWidgets_USE_FILE})

# win32 flag is necessary, for wxWidgets
add_executable(sockets_tester WIN32 main.cpp client.hpp)
target_link_libraries(sockets_tester PRIVATE Boost::boost Boost::system ${wxWidgets_LIBRARIES}) 
if (WIN32)
    target_link_libraries(sockets_tester PRIVATE wsock32 ws2_32)
endif()

if (MSVC AND CMAKE_BUILD_TYPE MATCHES DEBUG)
	# stupid lambda debugging (love GDB :( )
	target_compile_options(sockets_tester PRIVATE "/Od")
endif()

target_compile_definitions(sockets_tester PRIVATE "NO_CONSOLE")
