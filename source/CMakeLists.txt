# Copyright 2020 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(LIB_SOURCES
        js110_statistics.c
        device_change_notifier.c
)

foreach(f IN LISTS SOURCES)
    get_filename_component(b ${f} NAME)
    set_source_files_properties(${f} PROPERTIES
            COMPILE_DEFINITIONS "__FILENAME__=\"${b}\"")
endforeach()

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

add_library(js110_objlib OBJECT ${LIB_SOURCES})

# The shared library
add_library(js110_statistics SHARED $<TARGET_OBJECTS:js110_objlib>)
add_dependencies(js110_statistics js110_objlib)
target_link_libraries(js110_statistics Setupapi Winusb)

# The executable example
add_executable(js110_stats main.c $<TARGET_OBJECTS:js110_objlib>)
target_link_libraries(js110_stats Setupapi Winusb)
