##############################################################################
#   
#
#  This file is part of quantlib-xad, an adaptor module to enable using XAD with
#  QuantLib. XAD is a fast and comprehensive C++ library for
#  automatic differentiation.
#
#  Copyright (C) 2010-2023 Xcelerit Computing Ltd.
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
    EXPORT_NAME ${PACKAGE_NAME}
)
install(TARGETS quantlib-xad EXPORT QlXadTargets)
foreach(file ${QLXAD_HEADERS})
    install(FILES ${file} DESTINATION "${QL_INSTALL_INCLUDEDIR}/ql")
endforeach()

# Install config scripts and targets
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadConfigVersion.cmake"
    VERSION ${QL_VERSION}
    COMPATIBILITY AnyNewerVersion
)
export(EXPORT QlXadTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadTargets.cmake"
    NAMESPACE QuantLib::
)
configure_file("../cmake/QlXadConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadConfig.cmake"
    COPYONLY
)
configure_package_config_file("../cmake/QlXadConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadConfig.cmake"
    INSTALL_DESTINATION "${QL_INSTALL_CMAKEDIR}"
)
install(EXPORT QlXadTargets
    FILE QlXadTargets.cmake
    NAMESPACE QuantLib::
    DESTINATION "${QL_INSTALL_CMAKEDIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/../cmake/QlXadConfigVersion.cmake"
    DESTINATION "${QL_INSTALL_CMAKEDIR}"
)