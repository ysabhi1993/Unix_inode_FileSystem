#include "inode.hpp"

#include <algorithm>
#include <vector>
#include <list>

using std::list;
using std::shared_ptr;
using std::sort;
using std::vector;

uint Inode::block_size = 0;
list<FreeNode> *Inode::freeNode_list = nullptr;

Inode::Inode()
    :size(0), blocks_used(0), inode_blocks(new vector<vector<uint> >()){}

~Inode::Inode(){
    if(blocks_used == 0)
        return;
    else if(blocks_used == 1)
        freeNode_list->emplace_front(block_size, data_blocks[0]);

    vector<uint> blocks;

    for(auto block : data_blocks){
        blocks.push_back(block);
    }

    for(auto vec : *inode_blocks){
        for(uint block : vec){
            blocks.push_back(block);
        }
    }

    sort(blocks.begin(), blocks.end());

    uint start = blocks.front();
    uint last = start;
    uint size = block_size;

    blocks.erase(blocks.begin());

    for(uint block : blocks){
        if(block - last != block_size){
            freeNode_list->emplace_back(size, start);
            start = block;
            last = start;
            size = 0;
        } else{
            last = block;
            size += block_size;
        }
    }

    freeNode_list->emplace_front(size, start);
}
