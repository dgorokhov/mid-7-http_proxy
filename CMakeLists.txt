cmake_minimum_required(VERSION 3.30)
project(AsyncHttpProxy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Не больше трёх ошибок за раз
    add_compile_options(-fmax-errors=3)
endif()

# Ищем необходимые библиотеки с флагом CONFIG для интеграции с Conan
find_package(Boost REQUIRED CONFIG)
find_package(GTest REQUIRED CONFIG)

# Ищем все заголовочные файлы проекта (поддерживаем и .h, и .hpp)
file(GLOB HEADER_FILES "${CMAKE_SOURCE_DIR}/include/*.h" "${CMAKE_SOURCE_DIR}/include/*.hpp")

# Создаём статическую библиотеку (БЕЗ main.cpp, чтобы тесты не конфликтовали!)
add_library(${PROJECT_NAME}_imp STATIC
  "${CMAKE_SOURCE_DIR}/src/headers.cpp"
  ${HEADER_FILES}
)

# Настраиваем инклюды и линковку для библиотеки
target_include_directories(${PROJECT_NAME}_imp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME}_imp PUBLIC Boost::boost)

# Создаём исполняемый таргет сервера
add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_imp)

#
# Тесты
#

# Рекурсивно ищем все используемые в тестах .cpp файлы
file(GLOB TEST_SRC_FILES "${CMAKE_SOURCE_DIR}/tests/*.cpp")

add_executable(unit_tests "${TEST_SRC_FILES}")

# Линкуем библиотеку проекта и корректные таргеты GTest от Conan (в нижнем регистре)
target_link_libraries(unit_tests PRIVATE 
    ${PROJECT_NAME}_imp 
    GTest::gtest 
    GTest::gtest_main
)

# Включаем тестирование
enable_testing()
add_test(NAME Tests COMMAND unit_tests)
