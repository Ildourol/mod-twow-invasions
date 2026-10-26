# Optional CMake hooks for mod-twow-invasions.
#
# Source files under src/ are discovered automatically, conf/*.conf.dist files are copied
# automatically, and Addmod_twow_invasionsScripts() is invoked by the generated module loader.
#
# This file is included twice:
#   TORTOISE_MODULE_CMAKE_PHASE=DISCOVERY
#   TORTOISE_MODULE_CMAKE_PHASE=POST_TARGETS

if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")

  if(TORTOISE_CURRENT_MODULE_LINKAGE STREQUAL "dynamic")
    GetModuleProjectName("${TORTOISE_CURRENT_MODULE}" INVASIONS_MODULE_TARGET)
  else()
    set(INVASIONS_MODULE_TARGET modules)
  endif()

  # ---------------------------------------------------------------------------
  # 1. Discover TortoiseBots (Native Modular Companion Engine for tortoise-wow)
  # ---------------------------------------------------------------------------
  set(TORTOISEBOTS_FOUND FALSE)
  if(NOT DEFINED MODULE_TORTOISEBOTS OR NOT "${MODULE_TORTOISEBOTS}" STREQUAL "disabled")
    set(TORTOISEBOTS_SEARCH_PATHS
      "${CMAKE_SOURCE_DIR}/modules/TortoiseBots"
      "${CMAKE_SOURCE_DIR}/../TortoiseBots"
      "${CMAKE_CURRENT_LIST_DIR}/../TortoiseBots"
    )
    foreach(SEARCH_PATH ${TORTOISEBOTS_SEARCH_PATHS})
      if(EXISTS "${SEARCH_PATH}/runtime/BotManager.h")
        set(TORTOISEBOTS_ROOT "${SEARCH_PATH}")
        set(TORTOISEBOTS_FOUND TRUE)
        break()
      endif()
    endforeach()
  endif()

  if(TORTOISEBOTS_FOUND)
    message(STATUS "mod-twow-invasions: Discovered TortoiseBots at ${TORTOISEBOTS_ROOT}")
    target_compile_definitions(${INVASIONS_MODULE_TARGET} PRIVATE
      HAVE_TORTOISEBOTS=1
      MANGOSBOT_ZERO=1
      CMANGOS=1
    )
    target_include_directories(${INVASIONS_MODULE_TARGET}
      PUBLIC
        "${TORTOISEBOTS_ROOT}"
        "${TORTOISEBOTS_ROOT}/ai"
        "${TORTOISEBOTS_ROOT}/ai/playerbot"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/actions"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/triggers"
        "${TORTOISEBOTS_ROOT}/ai/playerbot/strategy/values"
        "${TORTOISEBOTS_ROOT}/host"
        "${TORTOISEBOTS_ROOT}/runtime"
    )
  endif()

endif()
