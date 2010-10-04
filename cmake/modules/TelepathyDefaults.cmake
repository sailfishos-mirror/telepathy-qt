# Enable testing using CTest
enable_testing()

# Always include srcdir and builddir in include path
# This saves typing ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} in about every subdir
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# put the include dirs which are in the source or build tree
# before all other include dirs, so the headers in the sources
# are prefered over the already installed ones
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# Use colored output
set(CMAKE_COLOR_MAKEFILE ON)

# Set compiler flags
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb")
    set(CMAKE_CXX_FLAGS_RELEASE        "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG          "-ggdb -O2 -fno-reorder-blocks -fno-schedule-insns -fno-inline")
    set(CMAKE_CXX_FLAGS_DEBUGFULL      "-O0 -g3 -ggdb -fno-inline")
    set(CMAKE_CXX_FLAGS_PROFILE        "-pg -g3 -ggdb -DNDEBUG")

    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-O2 -ggdb")
    set(CMAKE_C_FLAGS_RELEASE          "-O2 -DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG            "-ggdb -O2 -fno-reorder-blocks -fno-schedule-insns -fno-inline")
    set(CMAKE_C_FLAGS_DEBUGFULL        "-O0 -g3 -ggdb -fno-inline")
    set(CMAKE_C_FLAGS_PROFILE          "-pg -g3 -ggdb -DNDEBUG")

    set(CMAKE_EXE_LINKER_FLAGS_PROFILE    "-pg -ggdb")
    set(CMAKE_SHARED_LINKER_FLAGS_PROFILE "-pg -ggdb")

    set(DISABLE_WERROR 0 CACHE BOOL "compile without -Werror (normally enabled in development builds)")

    include(CompilerWarnings)
    include(TestCXXAcceptsFlag)

    CHECK_CXX_ACCEPTS_FLAG("-fvisibility=hidden" CXX_FVISIBILITY_HIDDEN)
    if (CXX_FVISIBILITY_HIDDEN)
        set(VISIBILITY_HIDDEN_FLAGS "-fvisibility=hidden")
    else (CXX_FVISIBILITY_HIDDEN)
        set(VISIBILITY_HIDDEN_FLAGS)
    endif (CXX_FVISIBILITY_HIDDEN)

    if(${CMAKE_BUILD_TYPE} STREQUAL Release)
        set(NOT_RELEASE 0)
    else(${CMAKE_BUILD_TYPE} STREQUAL Release)
        set(NOT_RELEASE 1)
    endif(${CMAKE_BUILD_TYPE} STREQUAL Release)

    set(desired
        all
        extra
        sign-compare
        pointer-arith
        format-security
        init-self
        non-virtual-dtor)
    set(undesired
        missing-field-initializers
        unused-parameter)
    compiler_warnings(CMAKE_CXX_FLAGS cxx ${NOT_RELEASE} "${desired}" "${undesired}")

    set(desired_c
        all
        extra
        declaration-after-statement
        shadow
        strict-prototypes
        missing-prototypes
        sign-compare
        nested-externs
        pointer-arith
        format-security
        init-self)
    set(undesired_c
        missing-field-initializers
        unused-parameter)
    compiler_warnings(CMAKE_C_FLAGS c ${NOT_RELEASE} "${desired_c}" "${undesired_c}")

    if(CMAKE_SYSTEM_NAME MATCHES Linux)
        add_definitions(-D_BSD_SOURCE)
    endif(CMAKE_SYSTEM_NAME MATCHES Linux)

    # Compiler coverage
    set(ENABLE_COMPILER_COVERAGE OFF CACHE BOOL "Enables compiler coverage tests through lcov. Enabling this option will build
Telepathy-Qt4 as a static library.")

    if (ENABLE_COMPILER_COVERAGE)
        check_cxx_accepts_flag("-fprofile-arcs -ftest-coverage" CXX_FPROFILE_ARCS)
        check_cxx_accepts_flag("-ftest-coverage" CXX_FTEST_COVERAGE)

        if (CXX_FPROFILE_ARCS AND CXX_FTEST_COVERAGE)
            find_program(LCOV lcov)
            find_program(LCOV_GENHTML genhtml)
            if (NOT LCOV OR NOT LCOV_GENHTML)
                message(FATAL_ERROR "You chose to use compiler coverage tests, but lcov or genhtml could not be found in your PATH.")
            else (NOT LCOV OR NOT LCOV_GENHTML)
                message(STATUS "Compiler coverage tests enabled - Telepathy-Qt4 will be compiled as a static library")
                set(COMPILER_COVERAGE_FLAGS "-fprofile-arcs -ftest-coverage")
            endif (NOT LCOV OR NOT LCOV_GENHTML)
        else (CXX_FPROFILE_ARCS AND CXX_FTEST_COVERAGE)
            message(FATAL_ERROR "You chose to use compiler coverage tests, but it appears your compiler is not able to support them.")
        endif (CXX_FPROFILE_ARCS AND CXX_FTEST_COVERAGE)
    else (ENABLE_COMPILER_COVERAGE)
        set(COMPILER_COVERAGE_FLAGS)
    endif (ENABLE_COMPILER_COVERAGE)

    # gcc under Windows
    if(MINGW)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")

        # we always link against the release version of QT with mingw
        # (even for debug builds). So we need to define QT_NO_DEBUG
        # or else QPluginLoader rejects plugins because it thinks
        # they're built against the wrong QT.
        add_definitions(-DQT_NO_DEBUG)
    endif(MINGW)
endif(CMAKE_COMPILER_IS_GNUCXX)

if(MSVC)
    set(ESCAPE_CHAR ^)
endif(MSVC)

set(LIB_SUFFIX "" CACHE STRING "Define suffix of library directory name (32/64)" )
set(LIB_INSTALL_DIR     "lib${LIB_SUFFIX}"  CACHE PATH "The subdirectory where libraries will be installed (default is ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})" FORCE)
set(INCLUDE_INSTALL_DIR "include"           CACHE PATH "The subdirectory where header files will be installed (default is ${CMAKE_INSTALL_PREFIX}/include)" FORCE)
set(DATA_INSTALL_DIR    "share/telepathy"   CACHE PATH "The subdirectory where data files will be installed (default is ${CMAKE_INSTALL_PREFIX}/share/telepathy)" FORCE)