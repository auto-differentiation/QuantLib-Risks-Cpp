##############################################################################
#   
#
#  This file is part of quantlib-xad, an adaptor module to enable using XAD with
#  QuantLib. XAD is a fast and comprehensive C++ library for
#  automatic differentiation.
#
#  Copyright (C) 2010-2024 Xcelerit Computing Ltd.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#   
##############################################################################

set(QLXAD_HEADERS
    qlxad.hpp
)
add_library(quantlib-xad INTERFACE)
target_include_directories(quantlib-xad INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${QL_INSTALL_INCLUDEDIR}>
)
target_compile_features(quantlib-xad INTERFACE cxx_std_14)
if(NOT QLXAD_DISABLE_AAD)
    target_compile_definitions(quantlib-xad INTERFACE QL_INCLUDE_FIRST=ql/qlxad.hpp)
else()
    target_compile_definitions(quantlib-xad INTERFACE QLXAD_DISABLE_AAD=1)
endif()
if(MSVC)
    target_compile_options(quantlib-xad INTERFACE /bigobj)
endif()
target_link_libraries(quantlib-xad INTERFACE XAD::xad)
set_target_properties(quantlib-xad PROPERTIES
    EXPORT_NAME QuantLibXad
)
install(TARGETS quantlib-xad EXPORT QuantLibTargets)
foreach(file ${QLXAD_HEADERS})
    install(FILES ${file} DESTINATION "${QL_INSTALL_INCLUDEDIR}/ql")
endforeach()


# Install a convenience config script for QuantLibXad, which allows users to
# 
# find_package(QuantLibXad REQUIRED)
# target_link_libraries(myapp PRIVATE QuantLib::QuantLib)
#
# That is, it looks for the XAD dependency before it includes the QuantLibConfig file
#
# Without that, users have to first find_package(XAD) before find_package(QuantLib)
# to achieve the same.
#

set(QLXAD_INSTALL_CMAKEDIR "lib/cmake/QuantLibXad" CACHE STRING
    "Installation directory for CMake scripts for QuantLib XAD")

include(CMakePackageConfigHelpers)
configure_file("../cmake/QuantLibXadConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/QuantLibXadConfig.cmake"
    COPYONLY
)
configure_package_config_file("../cmake/QuantLibXadConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/QuantLibXadConfig.cmake"
    INSTALL_DESTINATION "${QLXAD_INSTALL_CMAKEDIR}"
)
install(FILES "${PROJECT_BINARY_DIR}/cmake/QuantLibXadConfig.cmake"
    DESTINATION "${QLXAD_INSTALL_CMAKEDIR}"
)
