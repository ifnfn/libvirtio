#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

project(libvirtio C)


file(GLOB sources
       *.c
)

add_library(virtio STATIC EXCLUDE_FROM_ALL ${sources})
target_compile_options(virtio PRIVATE -Werror -g -DVIRTIO_USE_MMIO=1)

target_include_directories(virtio PUBLIC .)
target_link_libraries(virtio
       PUBLIC
       muslc
       core
)
