query_tools_version()

set(FFVA_AP fixed_delay)
set(PL_NAME fixed_delay)

set(FFVA_INT_COMPILE_DEFINITIONS
${APP_COMPILE_DEFINITIONS}
    appconfEXTERNAL_MCLK=0
    appconfI2S_ENABLED=0
    appconfUSB_ENABLED=0
    appconfUSB_AUDIO_ENABLED=0
    appconfUSB_AUDIO_MODE=0
    appconfUSB_CDC_ENABLED=0
    appconfAEC_REF_DEFAULT=appconfAEC_REF_I2S
    appconfI2S_MODE=appconfI2S_MODE_MASTER
    appconfI2S_AUDIO_SAMPLE_RATE=48000
    appconfDEVICE_CTRL_SPI=0
    appconfLED_RING=0
    appconfINPUT_SAMPLES_MIC_DELAY_MS=0
    appconfXSCOPE_4MIC_ENABLED=0
    appconfPIPELINE_BYPASS=0
    appconfBATCH_FILEIO_MODE=1
    XSCOPE_HOST_IO_ENABLED=1
)

# Additional include paths for DSP headers (aec_memory_pool.h, audio_pipeline_dsp.h)
set(BATCH_EXTRA_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/audio_pipelines/reference
    ${CMAKE_CURRENT_LIST_DIR}/audio_pipelines/reference/fixed_delay
)

#**********************
# Tile Targets
#**********************
set(TARGET_NAME tile0_sq66_fileio_batch)
add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL)
target_sources(${TARGET_NAME} PUBLIC ${APP_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${APP_INCLUDES} ${BATCH_EXTRA_INCLUDES})
target_compile_definitions(${TARGET_NAME}
    PUBLIC
        ${FFVA_INT_COMPILE_DEFINITIONS}
        THIS_XCORE_TILE=0
)
target_compile_options(${TARGET_NAME}
    PRIVATE
        ${APP_COMPILER_FLAGS}
        -fxscope
        ${CMAKE_CURRENT_LIST_DIR}/src/config-fileio-batch.xscope
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${APP_COMMON_LINK_LIBRARIES}
        fph::ffva::sq66
        fph::ffva::ap::${PL_NAME}
        xscope_fileio
        fwk_voice::aec
        fwk_voice::agc
        fwk_voice::ic
        fwk_voice::ns
        fwk_voice::vnr::features
        fwk_voice::vnr::inference
        ${CMAKE_CURRENT_LIST_DIR}/src/config-fileio-batch.xscope
)
target_link_options(${TARGET_NAME} PRIVATE ${APP_LINK_OPTIONS})
unset(TARGET_NAME)

set(TARGET_NAME tile1_sq66_fileio_batch)
add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL)
target_sources(${TARGET_NAME} PUBLIC ${APP_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${APP_INCLUDES} ${BATCH_EXTRA_INCLUDES})
target_compile_definitions(${TARGET_NAME}
    PUBLIC
        ${FFVA_INT_COMPILE_DEFINITIONS}
        THIS_XCORE_TILE=1
)
target_compile_options(${TARGET_NAME}
    PRIVATE
        ${APP_COMPILER_FLAGS}
        -fxscope
        ${CMAKE_CURRENT_LIST_DIR}/src/config-fileio-batch.xscope
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ${APP_COMMON_LINK_LIBRARIES}
        fph::ffva::sq66
        fph::ffva::ap::${PL_NAME}
        sln_voice::app::ffva::sp::passthrough
        xscope_fileio
        ${CMAKE_CURRENT_LIST_DIR}/src/config-fileio-batch.xscope
)
target_link_options(${TARGET_NAME} PRIVATE ${APP_LINK_OPTIONS})
unset(TARGET_NAME)

#*********************
# Create version.h
#*********************
SET(VERSIONING_CMD "build")
if(USE_DEV_TRACKING)
    list(APPEND VERSIONING_CMD "--track")
endif()

add_custom_target(sq66_fileio_batch_versioning
    COMMAND ${Python3_EXECUTABLE} ${VERSIONING_SCRIPT} ${VERSIONING_CMD} sq66_fileio_batch
    COMMENT "Running versioning.py build sq66_fileio_batch"
    VERBATIM
)
add_dependencies(tile0_sq66_fileio_batch sq66_fileio_batch_versioning)
add_dependencies(tile1_sq66_fileio_batch sq66_fileio_batch_versioning)

#**********************
# Merge binaries
#**********************
merge_binaries(sq66_fileio_batch tile0_sq66_fileio_batch tile1_sq66_fileio_batch 1)

#**********************
# Create run and debug targets
#**********************
create_run_target(sq66_fileio_batch)
create_debug_target(sq66_fileio_batch)
