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
    else ()
        set(VISIBILITY_HIDDEN_FLAGS)
    endif ()

    CHECK_CXX_ACCEPTS_FLAG("-fvisibility-inlines-hidden" CXX_FVISIBILITY_INLINES_HIDDEN)
    if (CXX_FVISIBILITY_INLINES_HIDDEN)
        set(VISIBILITY_HIDDEN_FLAGS "${VISIBILITY_HIDDEN_FLAGS} -fvisibility-inlines-hidden")
    endif ()

    CHECK_CXX_ACCEPTS_FLAG("-Wdeprecated-declarations" CXX_DEPRECATED_DECLARATIONS)
    if (CXX_DEPRECATED_DECLARATIONS)
        set(DEPRECATED_DECLARATIONS_FLAGS "-Wdeprecated-declarations -DTP_QT_DEPRECATED_WARNINGS")
    else ()
        set(DEPRECATED_DECLARATIONS_FLAGS)
    endif ()

    if(NOT TelepathyQt_VERSION_TWEAK)
        set(NOT_RELEASE 0)
    else()
        set(NOT_RELEASE 1)
    endif()

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
        unused-parameter
        unused-local-typedefs)
    compiler_warnings(CMAKE_CXX_FLAGS_WARNINGS cxx ${NOT_RELEASE} "${desired}" "${undesired}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_WARNINGS}")

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
        unused-parameter
        unused-local-typedefs)
    compiler_warnings(CMAKE_C_FLAGS_WARNINGS c ${NOT_RELEASE} "${desired_c}" "${undesired_c}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_WARNINGS}")

    # Link development builds with -Wl,--no-add-needed
    # TODO: binutils 2.21 renames the flag to --no-copy-dt-needed-entries, though it keeps the old
    # one as a deprecated alias.
    if(${NOT_RELEASE} EQUAL 1)
        set(CMAKE_EXE_LINKER_FLAGS            "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-add-needed")
        set(CMAKE_SHARED_LINKER_FLAGS         "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-add-needed")
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES Linux)
        add_definitions(-D_BSD_SOURCE -D_DEFAULT_SOURCE)
    endif()

    # Compiler coverage
    set(ENABLE_COMPILER_COVERAGE OFF CACHE BOOL "Enables compiler coverage tests through lcov. Enabling this option will build
Telepathy-Qt as a static library.")

    if (ENABLE_COMPILER_COVERAGE)
        check_cxx_accepts_flag("-fprofile-arcs -ftest-coverage" CXX_FPROFILE_ARCS)
        check_cxx_accepts_flag("-ftest-coverage" CXX_FTEST_COVERAGE)

        if (CXX_FPROFILE_ARCS AND CXX_FTEST_COVERAGE)
            find_program(LCOV lcov)
            find_program(LCOV_GENHTML genhtml)
            if (NOT LCOV OR NOT LCOV_GENHTML)
                message(FATAL_ERROR "You chose to use compiler coverage tests, but lcov or genhtml could not be found in your PATH.")
            else ()
                message(STATUS "Compiler coverage tests enabled - Telepathy-Qt will be compiled as a static library")
                set(COMPILER_COVERAGE_FLAGS "-fprofile-arcs -ftest-coverage")
            endif ()
        else ()
            message(FATAL_ERROR "You chose to use compiler coverage tests, but it appears your compiler is not able to support them.")
        endif ()
    else ()
        set(COMPILER_COVERAGE_FLAGS)
    endif ()

    # gcc under Windows
    if(MINGW)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")

        # we always link against the release version of QT with mingw
        # (even for debug builds). So we need to define QT_NO_DEBUG
        # or else QPluginLoader rejects plugins because it thinks
        # they're built against the wrong QT.
        add_definitions(-DQT_NO_DEBUG)
    endif()
endif()

if(MSVC)
    set(ESCAPE_CHAR ^)
endif()

include(GNUInstallDirs)

if((DEFINED LIB_SUFFIX) AND (NOT DEFINED LIB_INSTALL_DIR))
    set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")
endif()

if(DEFINED LIB_INSTALL_DIR)
    message(STATUS "Warning! LIB_SUFFIX and LIB_INSTALL_DIR options are deprecated. Use GNUInstallDirs options instead.")
    if(NOT IS_ABSOLUTE "${LIB_INSTALL_DIR}")
        set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
    endif()
else()
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}")
endif()

if(DEFINED INCLUDE_INSTALL_DIR)
    message(STATUS "Warning! INCLUDE_INSTALL_DIR option is deprecated. Use GNUInstallDirs options instead.")
    if(NOT IS_ABSOLUTE "${INCLUDE_INSTALL_DIR}")
        set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${INCLUDE_INSTALL_DIR}")
    endif()
else()
    set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
endif()
