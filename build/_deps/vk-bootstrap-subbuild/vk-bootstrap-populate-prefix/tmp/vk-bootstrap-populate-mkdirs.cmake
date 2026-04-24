# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-src")
  file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-src")
endif()
file(MAKE_DIRECTORY
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-build"
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix"
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/tmp"
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp"
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src"
  "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
