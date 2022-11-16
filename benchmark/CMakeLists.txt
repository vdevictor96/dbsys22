add_executable(milestone1_bench milestone1.cpp)
target_link_libraries(milestone1_bench PRIVATE $<TARGET_OBJECTS:dbsys22> mutable)
