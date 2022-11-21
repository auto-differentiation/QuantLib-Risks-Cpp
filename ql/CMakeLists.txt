##############################################################################
#   
#
#  This file is part of qlxad, an adaptor module to enable using XAD with
#  QuantLib. XAD is a fast and comprehensive C++ library for
#  automatic differentiation.
#
#  Copyright (C) 2010-2022 Xcelerit Computing Ltd.
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
add_library(qlxad INTERFACE)
target_include_directories(qlxad INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${QL_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(qlxad INTERFACE QL_INCLUDE_FIRST=ql/qlxad.hpp)
if(MSVC)
    target_compile_options(qlxad INTERFACE /bigobj)
endif()
target_link_libraries(qlxad INTERFACE XAD::xad)
set_target_properties(qlxad PROPERTIES
    EXPORT_NAME ${PACKAGE_NAME}
)
install(TARGETS qlxad EXPORT QlXadTargets)
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
