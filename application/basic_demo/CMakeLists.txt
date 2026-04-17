cmake_minimum_required(VERSION 3.16)

set(BASIC_DEMO_ESP_IDF_PATCH "${CMAKE_SOURCE_DIR}/tools/esp-idf.patch")

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "IDF_PATH environment variable is not set")
endif()

find_program(GIT_EXECUTABLE git REQUIRED)

# if(EXISTS "${BASIC_DEMO_ESP_IDF_PATCH}")
#     execute_process(
#         COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${BASIC_DEMO_ESP_IDF_PATCH}"
#         WORKING_DIRECTORY "$ENV{IDF_PATH}"
#         RESULT_VARIABLE BASIC_DEMO_PATCH_ALREADY_APPLIED
#         OUTPUT_QUIET
#         ERROR_QUIET
#     )

#     if(BASIC_DEMO_PATCH_ALREADY_APPLIED EQUAL 0)
#         message(STATUS "ESP-IDF patch already applied: ${BASIC_DEMO_ESP_IDF_PATCH}")
#     else()
#         execute_process(
#             COMMAND ${GIT_EXECUTABLE} apply "${BASIC_DEMO_ESP_IDF_PATCH}"
#             WORKING_DIRECTORY "$ENV{IDF_PATH}"
#             RESULT_VARIABLE BASIC_DEMO_PATCH_APPLY_RESULT
#             OUTPUT_VARIABLE BASIC_DEMO_PATCH_APPLY_STDOUT
#             ERROR_VARIABLE BASIC_DEMO_PATCH_APPLY_STDERR
#         )

#         if(NOT BASIC_DEMO_PATCH_APPLY_RESULT EQUAL 0)
#             message(FATAL_ERROR
#                 "Failed to apply ESP-IDF patch: ${BASIC_DEMO_ESP_IDF_PATCH}\n"
#                 "stdout:\n${BASIC_DEMO_PATCH_APPLY_STDOUT}\n"
#                 "stderr:\n${BASIC_DEMO_PATCH_APPLY_STDERR}")
#         endif()

#         message(STATUS "Applied ESP-IDF patch: ${BASIC_DEMO_ESP_IDF_PATCH}")
#     endif()
# else()
#     message(FATAL_ERROR "ESP-IDF patch file not found: ${BASIC_DEMO_ESP_IDF_PATCH}")
# endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(basic_demo)

include("${CMAKE_SOURCE_DIR}/tools/cmake/skills_sync.cmake")

fatfs_create_spiflash_image(storage fatfs_image FLASH_IN_PROJECT)
basic_demo_enable_component_skills_sync(
    TARGET fatfs_storage_bin
    DEMO_SKILLS_DIR "${CMAKE_SOURCE_DIR}/fatfs_image/skills"
    MANIFEST_PATH "${CMAKE_BINARY_DIR}/component_skills_manifest.json"
)
