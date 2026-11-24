cmake_minimum_required(VERSION 3.25)

project(gfx-research-clouds LANGUAGES C CXX)

option(GFX_RESEARCH_BUILD_APP "Build the interactive OpenGL experiment" ON)

set(GFX_RESEARCH_BASE_GIT_TAG v0.3 CACHE STRING "gfx-research-base Git revision")
set(GFX_RESEARCH_BASE_LOCAL_PATH "" CACHE PATH "Optional local gfx-research-base checkout")

if(GFX_RESEARCH_BUILD_APP)
if(GFX_RESEARCH_BASE_LOCAL_PATH)
    add_subdirectory("${GFX_RESEARCH_BASE_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/gfx-research-base")
else()
    if(NOT DEFINED CPM_SOURCE_CACHE)
        set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.cpm-cache" CACHE PATH "CPM source cache")
    endif()

    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    set(GFX_RESEARCH_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty" CACHE PATH "Third-party source directory")

    CPMAddPackage(
        NAME gfx_research_base
        GITHUB_REPOSITORY Terrasil/gfx-research-base
        GIT_TAG "${GFX_RESEARCH_BASE_GIT_TAG}"
        SOURCE_DIR "${GFX_RESEARCH_THIRD_PARTY_DIR}/gfx-research-base"
        OPTIONS
            "GFX_RESEARCH_BASE_BUILD_EXAMPLE OFF"
    )
endif()

add_executable(gfx-research-clouds
    src/main.cpp
)

target_compile_features(gfx-research-clouds PRIVATE cxx_std_20)
target_link_libraries(gfx-research-clouds PRIVATE gfx::research_base)
target_compile_definitions(gfx-research-clouds PRIVATE
    GFX_RESEARCH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    GFX_RESEARCH_BUILD_TYPE="$<CONFIG>"
    GFX_RESEARCH_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

if(COMMAND gfx_research_configure_executable)
    gfx_research_configure_executable(gfx-research-clouds)
endif()



endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(cloud-reference-tests tests/reference.cpp)
    target_compile_features(cloud-reference-tests PRIVATE cxx_std_20)
    add_test(NAME cloud-reference-tests COMMAND cloud-reference-tests)
endif()
