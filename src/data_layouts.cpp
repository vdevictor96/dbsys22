#include "data_layouts.hpp"
#include <numeric>

#include<iostream>
#include<cmath>


using namespace m;
using namespace m::storage;


typedef std::pair<const Type*, std::size_t> typeIndex;
typedef std::pair<std::size_t, std::size_t> strideAndOffset;
//typedef std::pair<const Type*, std::size_t, std::size_t> typeIndex;
typedef std::pair<const Type*, std::pair<std::size_t, std::size_t>> typeIndexRow;

bool sortTypePairsRow(typeIndexRow left, typeIndexRow right)
{
    return left.first->alignment() > right.first->alignment();
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

std::size_t calculateOffsetAndStride(std::vector<const Type*>& types, std::vector<typeIndexRow>& sortedTypes, std::vector<std::size_t>& offsetsVec, bool isOptim)
{
    std::size_t offset = 0;
    std::size_t max_align = 0;
    types.push_back(Type::Get_Bitmap(Type::TY_Vector, types.size()));
    std::size_t idx = 0;

    if (! isOptim) {
        for (const Type* type : types)
        {

            std::size_t align = type->alignment();
            offset = offset + ((align - (offset % align)) % align);

            offsetsVec.push_back(offset);
            offset += type->size();

            if (align > max_align)
                max_align = align;
        }
    }

    else {
        /* Sort vector types in descending alignment order */
        sortedTypes = sortTypesKeepIndexRow(types);
        for (typeIndexRow& type : sortedTypes)
        {

            std::size_t align = type.first->alignment();
            offset = offset + ((align - (offset % align)) % align);
            offsetsVec.push_back(offset);
            type.second.second = offset;
            offset += type.first->size();

            if (align > max_align)
                max_align = align;
        }
    }
    unsigned long byte = 8;
    max_align = std::max(max_align, byte); // optimize implementation
    std::size_t stride = offset;
    if (stride % max_align != 0) // optimize
        stride = std::max(offset + (max_align - (offset % max_align)), byte);
    idx++;

    return stride; // include formula in return statement
}


DataLayout MyNaiveRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    std::vector<std::size_t> offsetsVec;
    bool isOptim = false;
    std::vector<typeIndexRow> sortedTypes;
    std::size_t stride = calculateOffsetAndStride(types, sortedTypes, offsetsVec, isOptim);

    //std::size_t stride = calculateOffsetAndStride(types, offsets);
    
    DataLayout layout ;
    auto& row = layout.add_inode(/* num_tuples = */ 1, /* stride_in_bits = */ stride);
    
    std::size_t idx = 0;
    for(const Type* type : types)
    {
        row.add_leaf(
            /* type = */ type,
            /* idx = */ idx,
            /* offset = */ offsetsVec[idx],
            /* stride = */ 0);

        idx++;
    }

    return layout;
}


DataLayout MyOptimizedRowLayoutFactory::make(std::vector<const Type*> types, std::size_t num_tuples) const
{
    std::vector<std::size_t> offsetsVec;
    std::vector<typeIndexRow> sortedTypes;
    bool isOptim = true;
    std::size_t stride = calculateOffsetAndStride(types, sortedTypes, offsetsVec, isOptim);
    std::size_t idx = 0;

    DataLayout layout;
    auto& row = layout.add_inode(/* num_tuples = */ 1, /* stride_in_bits = */ stride);
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

bool sortTypePairs(typeIndex left, typeIndex right)
{
    return left.first->alignment() > right.first->alignment();
}


std::vector<typeIndex> sortTypesKeepIndex(std::vector<const Type*> types)
{
    std::vector<typeIndex> sortedTypes;
    std::size_t idx = 0;
    for (const Type* type : types)
    {
        sortedTypes.push_back(std::make_pair(type, idx));
        idx++;
    }
    std::sort(sortedTypes.begin(), sortedTypes.end(), sortTypePairs);
    return sortedTypes;

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