add_executable(
    AdjointEuropeanEquityOption 
    AdjointEuropeanEquityOptionXAD.cpp)
target_link_libraries(AdjointEuropeanEquityOption ql_library)
if(QL_INSTALL_EXAMPLES)
    install(TARGETS AdjointEuropeanEquityOption RUNTIME DESTINATION  ${QL_INSTALL_EXAMPLESDIR})
endif()