#ifndef _FSIMP_H_
#define _FSIMP_H_

#include "dirEntry.hpp"
#include "freeNode.hpp"
#include "inode.hpp"

#include <fstream>
#include <list>
#include <map>
#include <string>
#include <vector>

class FSImp{
    
    enum Mode {R, W, RW};
    struct Descriptor{
        Mode mode;      //access rights for the file/dir
        uint byte_pos;  
        std::weak_ptr<Inode> inode;     //pointer to inode of file/dir
        std::weak_ptr<DirEntry> from;   //pointer to the file/dir
        uint fd;        //number of last file descriptor
    };
    bool getMode(Mode *mode, std::string mode_s);

    struct PathRet{
        bool invalid_path = false;
        std::string final_name;
        std::shared_ptr<DirEntry> parent_node;
        std::shared_ptr<DirEntry> final_node;
    }

    const std::string filename;
    std::fstream disk_file;
    const uint block_size;
    const uint direct_blocks;
    const uint num_blocks;

    //DirEntry root
    std::list<FreeNode> freeNode_list;
    std::shared_ptr<DirEntry> root_dir;
    std::shared_ptr<DirEntry> pwd;
    std::map<uint, Descriptor> open_files;
    uint next_descriptor = 0;

    void init_disk(const std::string &file_name);
    std::unique_ptr<PathRet> parse_path(std::string path_str) const;
    bool basic_open(Descriptor *d, std::vector< std::string > args);
    std::unique_ptr<std::string> basic_read(descriptor &desc, const uint size);
    uint basic_write(Descriptor &desc, const std::string data);
    bool basic_close(uint fd);

  public:
    FSImp(const std::string &filename,
          const uint fs_size;
          const uint block_size;
          const uint direct_blocks);
    ~FSImp();
    void open(std::vector<std::string> args);
    void read(std::vector<std::string> args);
    void write(std::vector<std::string> args);
    void seek(std::vector<std::string> args);
    void close(std::vector<std::string> args);
    void mkdir(std::vector<std::string> args);
    void rmdir(std::vector<std::string> args);
    void cd(std::vector<std::string> args);
    void link(std::vector<std::string> args);
    void unlink(std::vector<std::string> args);
    void stat(std::vector<std::string> args);
    void ls(std::vector<std::string> args);
    void cat(std::vector<std::string> args);
    void copy(std::vector<std::string> args);
    void tree(std::vector<std::string> args);
    void printwd(std::vector<std::string> args);
};

#endif
