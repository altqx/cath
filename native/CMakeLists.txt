cmake_minimum_required(VERSION 3.20)
project(cath LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavformat libavcodec libavutil libswscale libswresample)

find_package(glm QUIET)
if(NOT glm_FOUND)
  find_path(GLM_INCLUDE_DIR glm/glm.hpp)
  if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "glm not found")
  endif()
endif()

add_library(volk STATIC third_party/volk.c)
target_include_directories(volk PUBLIC third_party ${Vulkan_INCLUDE_DIRS})
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(volk PUBLIC Vulkan::Vulkan ${CMAKE_DL_LIBS})

add_library(cath_core STATIC
  src/cath/platform/paths.cpp
  src/cath/platform/log.cpp
  src/cath/nif/nif_loader.cpp
  src/cath/tex/dds.cpp
  src/cath/kf/kf_loader.cpp
  src/cath/render/vulkan_renderer.cpp
  src/cath/media/movie_player.cpp
  src/cath/audio/audio_engine.cpp
  src/cath/ui/tea.cpp
  src/cath/ui/sp2.cpp
  src/cath/puzzle/pzl_map.cpp
  src/cath/puzzle/puzzle_world.cpp
  src/cath/event/story.cpp
  src/cath/game/save.cpp
  src/cath/game/game_app.cpp
  src/cath/app/viewer_app.cpp
)
target_include_directories(cath_core PUBLIC src third_party)
target_link_libraries(cath_core PUBLIC SDL3::SDL3 volk PkgConfig::FFMPEG)
if(TARGET glm::glm)
  target_link_libraries(cath_core PUBLIC glm::glm)
else()
  target_include_directories(cath_core PUBLIC ${GLM_INCLUDE_DIR})
endif()
target_compile_definitions(cath_core PUBLIC VK_NO_PROTOTYPES)

add_executable(cath-viewer src/tools/cath-viewer/main.cpp)
target_link_libraries(cath-viewer PRIVATE cath_core)

add_executable(cath-movie src/tools/cath-movie/main.cpp)
target_link_libraries(cath-movie PRIVATE cath_core)

add_executable(cath src/tools/cath/main.cpp)
target_link_libraries(cath PRIVATE cath_core)

find_program(GLSLC glslc REQUIRED)
set(SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BIN ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN})
foreach(SHADER mesh.vert mesh.frag)
  add_custom_command(
    OUTPUT ${SHADER_BIN}/${SHADER}.spv
    COMMAND ${GLSLC} -O ${SHADER_SRC}/${SHADER} -o ${SHADER_BIN}/${SHADER}.spv
    DEPENDS ${SHADER_SRC}/${SHADER}
    COMMENT "Compiling ${SHADER}"
  )
  list(APPEND SPV_SHADERS ${SHADER_BIN}/${SHADER}.spv)
endforeach()
add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(cath-viewer shaders)
add_dependencies(cath-movie shaders)
add_dependencies(cath shaders)

foreach(TGT cath-viewer cath-movie cath)
  add_custom_command(TARGET ${TGT} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TGT}>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BIN} $<TARGET_FILE_DIR:${TGT}>/shaders
  )
endforeach()
