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