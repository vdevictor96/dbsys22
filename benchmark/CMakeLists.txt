add_executable(milestone1_bench milestone1.cpp $<TARGET_OBJECTS:dbsys22>)
target_link_libraries(milestone1_bench PRIVATE mutable)
