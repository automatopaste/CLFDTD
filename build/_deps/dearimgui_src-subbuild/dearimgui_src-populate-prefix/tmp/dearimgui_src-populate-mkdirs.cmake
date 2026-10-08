# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-src")
  file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-src")
endif()
file(MAKE_DIRECTORY
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-build"
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix"
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/tmp"
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/src/dearimgui_src-populate-stamp"
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/src"
  "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/src/dearimgui_src-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/src/dearimgui_src-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/dev/FDTD/CLFDTD/build/_deps/dearimgui_src-subbuild/dearimgui_src-populate-prefix/src/dearimgui_src-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
