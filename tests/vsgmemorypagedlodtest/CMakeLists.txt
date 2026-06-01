set(SOURCES vsgmemorypagedlodtest.cpp)

add_executable(vsgmemorypagedlodtest ${SOURCES})

target_link_libraries(vsgmemorypagedlodtest vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgmemorypagedlodtest PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgmemorypagedlodtest vsgXchange::vsgXchange)
endif()

install(TARGETS vsgmemorypagedlodtest RUNTIME DESTINATION bin)
