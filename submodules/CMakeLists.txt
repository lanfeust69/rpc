#[[
   Copyright (c) 2024 Edward Boggis-Rolfe
   All rights reserved.
]]

cmake_minimum_required(VERSION 3.24)

if(RPC_STANDALONE)
  if(WIN32)
    if(${BUILD_TYPE} STREQUAL "release")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MD")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MDd")
    endif()
  endif()

  set(FMT_INSTALL ON)
  add_subdirectory(fmt)
  # fmt CMakeLists.txt defines a PUBLIC_HEADER property which triggers a warning when used as a subproject
  set_property(TARGET fmt PROPERTY PUBLIC_HEADER)

  add_subdirectory(args)
  add_library(args::args ALIAS args)
  add_subdirectory(googletest)

  add_library(yas_common INTERFACE)

  target_include_directories(yas_common INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/yas/include>")

  add_subdirectory(idlparser)

  message("spdlog this is used in tests only and does not need to be installed for other users")
  set(SPDLOG_FMT_EXTERNAL
      ON
      CACHE BOOL "external fmt" FORCE)
  set(SPDLOG_NO_EXCEPTIONS
      ON
      CACHE BOOL "no exceptions" FORCE)
  set(SPDLOG_INSTALL OFF)
  add_subdirectory(spdlog)
endif()
