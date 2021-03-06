add_definitions(-DUNIT_TESTS)

# --- SchemaItems
add_subdirectory(./SchemaItem)

include_directories (
  ${CMAKE_SOURCE_DIR}/src/components/utils/include
  ../../../src/thirdPartyLibs/gmock-1.7.0/include
  ../../../src/thirdPartyLibs/gmock-1.7.0/gtest/include
  ../../../src/components/smart_objects/include
  ../../../src/components/formatters/include/
  ../../../src//thirdPartyLibs/jsoncpp/include/
)

# Libraries are the same for all SmartObject test
set(RTLIB rt)
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
set(RTLIB )
endif()

set (LIBRARIES
    gtest
    gtest_main
    gmock
    gmock_main
    SmartObjects
    formatters
    jsoncpp
    apr-1
    aprutil-1
    ${RTLIB}
)

set (SOURCES
  ./SmartObjectConvertionTimeTest.cc
  ./SmartObjectDraftTest.cc
  ./SmartObjectInvalidTest.cc
  ./SmartObjectStressTest.cc
  ./SmartObjectUnitTest.cc
  ./TSharedPtrTest.cc
  ./smart_object_performance_test.cc
  ./map_performance_test.cc
)

add_library("test_SmartObjectTest" ${SOURCES})

create_test("test_SmartObject_PerformanceTest" "./smart_object_performance_test.cc" "${LIBRARIES}")
create_test("test_Map_PerformanceTest" "./map_performance_test.cc" "${LIBRARIES}")

create_test("test_SmartObject_DraftTest" "./SmartObjectDraftTest.cc" "${LIBRARIES}")
create_test("test_SmartObject_UnitTest" "./SmartObjectUnitTest.cc" "${LIBRARIES}")
create_test("test_SmartObject_StressTest" "./SmartObjectStressTest.cc" "${LIBRARIES}")
create_test("test_SmartObject_InvalidTest" "./SmartObjectInvalidTest.cc" "${LIBRARIES}")
create_test("test_SmartObject_ConvertionTimeTest" "./SmartObjectConvertionTimeTest.cc" "${LIBRARIES}")
create_test("test_TSharedPtrTest" "./TSharedPtrTest.cc" "${LIBRARIES}")

# vim: set ts=2 sw=2 et:
