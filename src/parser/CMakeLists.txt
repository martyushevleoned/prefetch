option(LIBSCCA_PARSER "enable libscca parser" OFF) # apt install -y pkg-config libscca-dev libbfio-dev
option(NTDLL_PARSER "enable ntdll parser" OFF) # windows required
option(LIBPREFETCH_PARSER "enable libprefetch parser" OFF) # TODO rust lib

target_sources(prefetch PRIVATE parser.cpp)

if(NTDLL_PARSER)
    target_sources(prefetch PRIVATE ntdll_parser.cpp)
    target_compile_definitions(prefetch PRIVATE NTDLL_PARSER)
endif()
if(LIBSCCA_PARSER)
    find_package(PkgConfig)
    pkg_check_modules(LIBSCCA REQUIRED libscca)
    target_include_directories(prefetch PRIVATE ${LIBSCCA_INCLUDE_DIRS})
    target_link_libraries(prefetch PRIVATE ${LIBSCCA_LIBRARIES})
    target_compile_options(prefetch PRIVATE ${LIBSCCA_CFLAGS_OTHER})
    target_compile_definitions(prefetch PRIVATE LIBSCCA_PARSER)
    target_sources(prefetch PRIVATE libscca_parser.cpp)
endif()
