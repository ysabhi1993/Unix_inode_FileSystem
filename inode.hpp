/*
Every Inode object contains
1. a list of pointers to freeNodes
2. block size
3. blocks used
4. A list of pointers to inode blocks that are owned by a unique pointer.
*/
#ifndef _INODE_H_
#define _INODE_H_

#include "freeNode.hpp"

#include <sys/types.h>
#include <list>
#include <vector>
#include <memory>
#include <string>

class Inode{
    public:
        static uint block_size;
        static std::list<FreeNode> *freeNode_list;
        uint size;
        uint blocks_used; 
        std::vector<uint> data_blocks;
        std::unique_ptr<std::vector<std::vector<int> > > inode_blocks;
        
        Inode();
        ~Inode();
};

#endif
