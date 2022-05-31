cmake_minimum_required(VERSION 3.13)
project(psimulator)
enable_language(C)


set(srcs armcopro.c
         armemu.c
         arminit.c
         armio.c
         armlcd.c
         armmem.c
         armmmu.c
         armsupp.c
         armvirt.c
         bag.c
         psion.c
)
list(TRANSFORM srcs PREPEND src/)

set(tgt ${CMAKE_PROJECT_NAME})
add_executable(${tgt} ${srcs})

target_compile_options(${tgt} PRIVATE -m32)
target_link_options(${tgt} PRIVATE -m32)
target_link_libraries(${tgt} -lnsl -lX11 -lXext -lm)
set_source_files_properties(src/armemu.c PROPERTIES COMPILE_DEFINITIONS MODE32)
