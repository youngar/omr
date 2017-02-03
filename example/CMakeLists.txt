add_executable(example
  main.cpp
)

target_link_libraries(example
  PRIVATE
    omrgc
)

add_test(example example)

add_subdirectory(glue)
