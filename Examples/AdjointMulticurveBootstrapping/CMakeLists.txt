add_executable(
    AdjointMulticurveBootstrapping 
    AdjointMulticurveBootstrappingXAD.cpp)
target_link_libraries(AdjointMulticurveBootstrapping ql_library)
if(QL_INSTALL_EXAMPLES)
    install(TARGETS AdjointMulticurveBootstrapping RUNTIME DESTINATION  ${QL_INSTALL_EXAMPLESDIR})
endif()