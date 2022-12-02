#include "data_layouts.hpp"
#include <numeric>

#include<iostream>
#include<cmath>


using namespace m;
using namespace m::storage;

std::size_t calculateOffsetAndStride(std::vector<const Type*>& types, std::vector<std::size_t>& offsets)
{
    std::size_t offset = 0;
    std::size_t max_align = 0;
    types.push_back(Type::Get_Bitmap(Type::TY_Vector, types.size()));
    for(const Type* type : types)
    {

        type->dump();
        std::cout << "Offset - " << offset << std::endl;
        std::cout << "Size - " << type->size() << std::endl;
        std::cout << "Alignment - " << type->alignment() << std::endl;
        std::size_t align = type->alignment();
        offset = offset + ((align - (offset % align)) % align);
       
        std::cout << "offset (aligned) - " << offset << std::endl; 
        offsets.push_back(offset);
        offset += type->size();

        if (align > max_align)
            max_align = align;
    }
    //std::size_t stride = offsets.back() + types.back()->size();
    std::cout << "Initial stride value - " << offset << std::endl;
    //std::size_t stride = std::max(pow(2, ceil(log2(offset))), 8.0); // change logic of stride calculation
    unsigned long byte = 8;
    max_align = std::max(max_align, byte); //ToDo - optimize implementation
    std::size_t stride = std::max(offset + (max_align - (offset % max_align)), byte);
    std::cout << "Calculated stride  " << stride; 
    return stride; // include formula in return statement

}

DataLayout MyNaiveRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    std::vector<std::size_t> offsets;
    std::size_t stride = calculateOffsetAndStride(types, offsets);
    
    DataLayout layout ;
    auto& row = layout.add_inode(/* num_tuples = */ 1, /* stride_in_bits = */ stride);
    
    std::size_t idx = 0;
    for(const Type* type : types)
    {
        row.add_leaf(
            /* type = */ type,
            /* idx = */ idx,
            /* offset = */ offsets[idx],
            /* stride = */ 0);

        idx++;
    }

    return layout;
}





bool sortTypes(const Type* lhs, const Type* rhs)
{
    return lhs->alignment() > rhs->alignment();
}

typedef std::pair<const Type*, std::size_t> typeIndex;
typedef std::pair<std::size_t, std::size_t> strideAndOffset;
//typedef std::pair<const Type*, std::size_t, std::size_t> typeIndex;
typedef std::pair<const Type*, std::pair<std::size_t, std::size_t>> typeIndexRow;


bool sortTypePairs(typeIndex left, typeIndex right)
{
    return left.first->alignment() > right.first->alignment();
}



bool sortTypePairsRow(typeIndexRow left, typeIndexRow right)
{
    return left.first->alignment() > right.first->alignment();
}



std::vector<typeIndex> sortTypesKeepIndex(std::vector<const Type*> types)
{
    std::vector<typeIndex> sortedTypes;
    std::size_t idx = 0;
    for(const Type* type : types)
    {
        sortedTypes.push_back(std::make_pair(type, idx));
        idx++;
    }
    std::sort(sortedTypes.begin(), sortedTypes.end(), sortTypePairs);
    return sortedTypes;
   
}



std::vector<typeIndexRow> sortTypesKeepIndexRow(std::vector<const Type*> types)
{
    std::vector<typeIndexRow> sortedTypes;
    std::size_t idx = 0;
   for (const Type* type : types)
    {   
        sortedTypes.push_back(std::make_pair(type, std::make_pair(idx, 0)));
        idx++;
    }
    std::sort(sortedTypes.begin(), sortedTypes.end(), sortTypePairsRow);
    return sortedTypes;



}




std::size_t calculateOffsetAndStride1(std::vector<const Type*>& types, std::map<const Type*, std::size_t>& offsets, std::vector<typeIndexRow>& sortedTypes, std::vector<std::size_t>& offsetsVec)
{
    std::size_t offset = 0;
    std::size_t max_align = 0;
    types.push_back(Type::Get_Bitmap(Type::TY_Vector, types.size()));
    std::size_t idx = 0;



   /* Sort vector types in descending alignment order */
    sortedTypes = sortTypesKeepIndexRow(types);



   for(typeIndexRow& type : sortedTypes)
    {



        type.first->dump();
        std::cout << "Offset - " << offset << std::endl;
        std::cout << "Size - " << type.first->size() << std::endl;
        std::cout << "Alignment - " << type.first->alignment() << std::endl;
        std::size_t align = type.first->alignment();
        offset = offset + ((align - (offset % align)) % align);



        std::cout << "offset (aligned) - " << offset << std::endl;
        //offsets[type.first] = offset;
        //offsets.insert(std::make_pair(type.first, offset));
        //offsets[type.first] = offset;
        
        offsetsVec.push_back(offset);
        type.second.second = offset;
        offset += type.first->size();



       if (align > max_align)
            max_align = align;
    }
    //std::size_t stride = offsets.back() + types.back()->size();
    std::cout << "Initial stride value - " << offset << std::endl;
    //std::size_t stride = std::max(pow(2, ceil(log2(offset))), 8.0); // change logic of stride calculation



   unsigned long byte = 8;
    max_align = std::max(max_align, byte); //ToDo - optimize implementation
    std::size_t stride = offset;
    if(stride%max_align != 0) // optimize
        stride = std::max(offset + (max_align - (offset % max_align)), byte);
    //std::size_t stride = offset + (max_align - (offset % max_align));
    std::cout << "Calculated stride  " << stride << std::endl;
    idx++;



   return stride ; // include formula in return statement



}

DataLayout MyOptimizedRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    std::map<const Type*, std::size_t> offsets;;
    std::vector<std::size_t> offsetsVec;
    std::vector<typeIndexRow> sortedTypes;
    std::size_t stride = calculateOffsetAndStride1(types, offsets, sortedTypes, offsetsVec);



    DataLayout layout;
    auto& row = layout.add_inode(/* num_tuples = */ 1, /* stride_in_bits = */ stride);



    std::size_t idx = 0;
    for (const Type* type : types)
    {
        auto it = std::find_if( sortedTypes.begin(), sortedTypes.end(), [idx](const std::pair<const Type*, std::pair<std::size_t, std::size_t>>& element){ return element.second.first == idx;} );
        row.add_leaf(
            /* type = */ type,
            /* idx = */ idx,
            /* offset = */ it->second.second,
            /* stride = */ 0);

       idx++;
    }

   return layout;
}

std::vector<strideAndOffset> calculateIndexedStrideAndOffset (std::vector<const Type*> types, std::size_t total_tuples)
{
    /* Create new vector with the size of types vector */
    std::vector<strideAndOffset> indexedStrideAndOffset = std::vector<strideAndOffset>(types.size());

    /* Sort vector types in descending alignment order */
    std::vector<typeIndex> orderedTypesWithIndex = sortTypesKeepIndex(types);

    /* Calculate offset and stride for each type, save in vector with corresponding index in original types vector */
    std::size_t offset = 0;
    for(const typeIndex type : orderedTypesWithIndex)
    {
        std::size_t size = type.first->size();
        indexedStrideAndOffset[type.second] = std::make_pair(size, offset);
        offset += size * total_tuples;
    }
    return indexedStrideAndOffset;

}

DataLayout MyPAX4kLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    std::size_t block_size = 4096 * 8;
    types.push_back(Type::Get_Bitmap(Type::TY_Vector, types.size()));
    DataLayout layout;

    /* Calculate num_tuples */
    std::size_t total_bits = 0;
    for(const Type* type : types)
    {
        std::size_t size = type->size();
        total_bits += size;
    }
    std::size_t total_tuples = block_size / total_bits;

    auto& row = layout.add_inode(/* num_tuples = */ total_tuples, /* stride_in_bits = */ block_size);
    /* Reorder leafs to optimize memory allocation */
    std::vector<strideAndOffset> indexedStrideAndOffset = calculateIndexedStrideAndOffset(types, total_tuples);
    std::size_t idx = 0;
    for(const Type* type : types)
    {
        strideAndOffset strideAndOffset = indexedStrideAndOffset[idx];
        row.add_leaf(
            /* type = */ type,
            /* idx = */ idx,
            /* offset = */ strideAndOffset.second,
            /* stride = */ strideAndOffset.first);
        idx++;
    }

    return layout;
}