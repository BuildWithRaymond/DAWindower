cmake_minimum_required(VERSION 3.24)
project(DAWindower VERSION 0.2.2 LANGUAGES CXX)

# Runtime contract: Windows x86, C++17 and a statically linked CRT.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "DAWindower requires Windows x86. Use -A Win32.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
add_compile_definitions(DAW_VERSION="${PROJECT_VERSION}")
if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
endif()

include(CTest)
option(DAW_BUILD_PROXY_TESTS "Build the legacy development-only proxy comparison" OFF)

# Geometry is shared by the DLL and tests; it has no desktop dependency.
add_library(daw_geometry STATIC src/window/geometry.cpp)
target_include_directories(daw_geometry PUBLIC include)
set(DAW_GRAPHICS
  src/graphics/draw.cpp
  src/graphics/surface.cpp
  src/graphics/gdi.cpp
  src/window/window.cpp
  src/window/sizing.cpp)
set(DAW_SOURCES
  src/main.cpp
  src/compatibility/bootstrap.cpp
  src/compatibility/imports.cpp
  src/compatibility/mutex.cpp
  src/cursor/diagnostics.cpp
  ${DAW_GRAPHICS})

# The only runtime distribution target.
add_library(DAWindower SHARED ${DAW_SOURCES} src/DAWindower.def)
target_include_directories(DAWindower PRIVATE include src)
target_link_libraries(DAWindower PRIVATE daw_geometry dxguid user32 gdi32 comctl32 wtsapi32)

if(BUILD_TESTING)
  # Direct source tests exercise recovery without injecting into a retail client.
  add_executable(geometry_tests tests/geometry_tests.cpp)
  target_link_libraries(geometry_tests PRIVATE daw_geometry)
  add_test(NAME geometry COMMAND geometry_tests)

  add_executable(recovery_tests tests/recovery_tests.cpp ${DAW_GRAPHICS})
  target_include_directories(recovery_tests PRIVATE include src)
  target_link_libraries(recovery_tests PRIVATE daw_geometry ddraw dxguid user32 gdi32 comctl32 wtsapi32)
  add_test(NAME surface_recovery_contract COMMAND recovery_tests)

  add_executable(presentation_recovery_tests
    tests/presentation_recovery_tests.cpp
    src/window/window.cpp
    src/window/sizing.cpp
    src/graphics/gdi.cpp
    src/compatibility/imports.cpp)
  target_include_directories(presentation_recovery_tests PRIVATE include src)
  target_link_libraries(presentation_recovery_tests PRIVATE daw_geometry dxguid user32 gdi32 comctl32 wtsapi32)
  add_test(NAME presentation_recovery_contract COMMAND presentation_recovery_tests)

  # LoadLibrary tests use the actual DLL beside their executable. The retail
  # helper is built for manual acceptance but is deliberately not a CTest case.
  add_executable(ddraw_harness tools/ddraw_harness.cpp)
  add_executable(retail_loadlibrary_test tools/retail_loadlibrary_test.cpp)
  target_link_libraries(retail_loadlibrary_test PRIVATE user32 gdi32)
  foreach(target ddraw_harness retail_render_tests gdi_render_tests resize_tests resize_cursor_tests)
    if(NOT TARGET ${target})
      add_executable(${target} tests/${target}.cpp)
    endif()
    target_include_directories(${target} PRIVATE include)
    target_link_libraries(${target} PRIVATE ddraw dxguid user32 gdi32)
    add_dependencies(${target} DAWindower)
  endforeach()

  add_test(NAME directdraw_contract COMMAND ddraw_harness --self-test)
  add_test(NAME loadlibrary_only_contract COMMAND ddraw_harness --load-only)
  add_test(NAME dormant_unload_contract COMMAND ddraw_harness --dormant-unload)

  # Preserve compatibility with launchers that keep the old dawnd.dll filename.
  add_custom_command(TARGET ddraw_harness POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/renamed/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ddraw_harness>" "${CMAKE_BINARY_DIR}/renamed/$<CONFIG>/ddraw_harness.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:DAWindower>" "${CMAKE_BINARY_DIR}/renamed/$<CONFIG>/dawnd.dll")
  add_test(NAME renamed_loadlibrary_contract COMMAND "${CMAKE_BINARY_DIR}/renamed/$<CONFIG>/ddraw_harness.exe" --load-only-renamed)

  add_test(NAME retail_render_contract COMMAND retail_render_tests)
  add_test(NAME gdi_render_contract COMMAND gdi_render_tests)
  add_test(NAME retail_render_loadlibrary_contract COMMAND retail_render_tests --load-only)
  add_test(NAME gdi_render_loadlibrary_contract COMMAND gdi_render_tests --load-only)
  add_test(NAME smooth_resize_contract COMMAND resize_tests)
  add_test(NAME resize_cursor_contract COMMAND resize_cursor_tests)

  # Geometry is safe without a desktop. Native rendering tests share desktop
  # state and must not compete for visible windows/cursor state under ctest -j.
  set_tests_properties(geometry PROPERTIES LABELS "unit")
  set_tests_properties(
    surface_recovery_contract presentation_recovery_contract directdraw_contract
    loadlibrary_only_contract dormant_unload_contract renamed_loadlibrary_contract
    retail_render_contract gdi_render_contract retail_render_loadlibrary_contract
    gdi_render_loadlibrary_contract smooth_resize_contract resize_cursor_contract
    PROPERTIES LABELS "desktop" RUN_SERIAL TRUE)

  # Optional historical comparison route. Never installed with the standalone DLL.
  if(DAW_BUILD_PROXY_TESTS)
    add_library(ddraw_proxy SHARED ${DAW_SOURCES} src/ddraw.def)
    target_compile_definitions(ddraw_proxy PRIVATE DAW_PROXY)
    target_include_directories(ddraw_proxy PRIVATE include src)
    target_link_libraries(ddraw_proxy PRIVATE daw_geometry dxguid user32 gdi32 comctl32 wtsapi32)
    set_target_properties(ddraw_proxy PROPERTIES OUTPUT_NAME ddraw
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/proxy/$<CONFIG>"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/proxy/$<CONFIG>")
    foreach(target ddraw_harness retail_render_tests gdi_render_tests)
      add_dependencies(${target} ddraw_proxy)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${target}>" "${CMAKE_BINARY_DIR}/proxy/$<CONFIG>/${target}.exe")
      if(target STREQUAL "ddraw_harness")
        set(proxy_arg --proxy-self-test)
      else()
        set(proxy_arg --proxy)
      endif()
      add_test(NAME ${target}_proxy_contract COMMAND "${CMAKE_BINARY_DIR}/proxy/$<CONFIG>/${target}.exe" ${proxy_arg})
      set_tests_properties(${target}_proxy_contract PROPERTIES LABELS "desktop" RUN_SERIAL TRUE)
    endforeach()
  endif()
endif()

# Installation is one file. Development tools, optional proxy, diagnostics,
# headers and documentation are not runtime distribution files.
install(TARGETS DAWindower RUNTIME DESTINATION .)
