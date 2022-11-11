add_library(
    dbsys22
    OBJECT
    data_layouts.cpp
)
add_dependencies(dbsys22 Mutable)

add_executable(milestone1 milestone1.cpp $<TARGET_OBJECTS:dbsys22>)
target_link_libraries(milestone1 PRIVATE mutable)
