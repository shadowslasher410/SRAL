# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/Sheryl/Documents/My_Projects/SRAL/SRC")
  file(MAKE_DIRECTORY "C:/Users/Sheryl/Documents/My_Projects/SRAL/SRC")
endif()
file(MAKE_DIRECTORY
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sandbox-desktop-build"
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix"
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/tmp"
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/src/sral_desktop_sandbox-stamp"
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/src"
  "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/src/sral_desktop_sandbox-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/src/sral_desktop_sandbox-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Sheryl/Documents/My_Projects/SRAL/build/desktop-debug/sral_desktop_sandbox-prefix/src/sral_desktop_sandbox-stamp${cfgdir}") # cfgdir has leading slash
endif()
