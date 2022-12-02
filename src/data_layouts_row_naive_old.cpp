#include "data_layouts.hpp"
#include <numeric>


using namespace m;
using namespace m::storage;


DataLayout MyNaiveRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    DataLayout layout;
    /* Iterate over the vector types to calculate the stride*/
    int offset = 0;
    int alignment = 0;
    for (std::vector<const Type *>::iterator it = types.begin(); it != types.end(); ++it) {
        int id = std::distance(types.begin(), it);
        int size = (*it)->size();
        // auto type = (*it)->Type();
        if (id == 0) {
            alignment = (*it)->alignment();
            std::cout << "\nalignment\n";
            std::cout << alignment;
            std::cout << "\nalignment\n";
        }
        /* Update offset for the next leaf with the value of the current type */
        offset += (*it)->size();
    }
    /* At the end add a bitmap leaf with size of total number of attributes */
    int bitmap_size = types.size();
    offset += bitmap_size;

    // add inode with num_tuples 1 and and stride of ??
    std::cout << "\stride\n";
    int stride = alignment * ((offset / alignment) + 1);
    int complete_byte = stride % 8;
    if (complete_byte != 0) {
        stride += (8 - complete_byte);
    }
    std::cout << stride;
    std::cout << "\stride\n";
    auto &row = layout.add_inode(1, stride);

    /* Iterate over the vector types to add the leafs */
    offset = 0;
    for (std::vector<const Type *>::iterator it = types.begin(); it != types.end(); ++it) {
        int id = std::distance(types.begin(), it);
        int size = (*it)->size();
        // auto type = (*it)->Type();
        std::cout << "\nsize\n";
        std::cout << (*it)->size();
        std::cout << "\nsize\n";
        std::cout << "\nindex\n";
        std::cout << id;
        std::cout << "\nindex\n";

        /* For each attr add leaf with its type, increasing id, offset of the ammount of bits, stride 0 */
        row.add_leaf(
            (*it),
            id, // id
            offset,
            0
        );
        /* Update offset for the next leaf with the value of the current type */
        offset += (*it)->size();
    }
    /* At the end add a bitmap leaf with size of total number of attributes */
    bitmap_size = types.size();
    row.add_leaf(
        Type::Get_Bitmap(Type::TY_Vector, bitmap_size),
        bitmap_size, // id
        offset,
        0
    );
    
    return layout;
    
}

DataLayout MyOptimizedRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    // TODO 1.3: implement computing an optimized row layout
    return DataLayout();
}

DataLayout MyPAX4kLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    // TODO 1.4: implement computing a PAX layout
    return DataLayout();
}
