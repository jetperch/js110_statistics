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

cmake_minimum_required (VERSION 3.1)
set(PARENT_PROJECT_DIR ${PROJECT_SOURCE_DIR})
project(JS110_STATISTICS
        VERSION 0.1.0
        LANGUAGES C)
SET(PROJECT_PREFIX JS110_STATIS)
SET(VERSION_STRING "${PROJECT_VERSION}")
SET(VERSION_UNDERSCORE "${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}_${PROJECT_VERSION_PATCH}")

# add_subdirectory(third-party)
# add_definitions(-std=c99 -Wall -Werror -Wpedantic -Wextra -fPIC)
# add_definitions(/Wall)
include_directories(include)
add_subdirectory(source)

# enable_testing()
# add_subdirectory(test)
