#include "fsImple.hpp"
#include "dirEntry.hpp"
#include "freeNode.hpp"
#include "inode.hpp"

#include <cmath>
#include <iostream>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <assert.h>

using namespace std;

#define ops_at_least(x)                                   \
  if(static_cast<int>(args.size() < x + 1)){              \
      cout << args[0] << " : missing operand" << endl;    \
      return;                                             \
  }                                                       

#define ops_less_than(x)                                  \
  if(static_cast<int>(args.size() > x + 1)){              \
      cout << args[0] << " :missing operand" << endl;     \
      return;                                             \
  }

#define ops_exact(x)            \
  ops_at_least(x);              \
  ops_less_than(x);

FSImp::FSImp(const std::string &filename,
             const uint fs_size;
             const uint block_size;
             const uint direct_blocks)

        :filename(filename), 
         block_size(block_size), 
         direct_blocks(direct_blocks)
         num_blocks(ceil(static_cast<double>(fs_size)/block_size)){
            Inode::block_size = block_size;
            Inode::freeNode_list = &freeNode_list;
            root_dir = DirEntry::make_dir("root", nullptr);
            //setting rootdir
            pwd = root_dir;
            init_disk(filename);
            freeNode_list.emplace_back(num_blocks, 0);
    }

FSImp::~FSImp(){
    disk_file.close();
    remove(filename.c_str());
}

void FSImp::init_disk(const string &filename){

    const vector<char> zeroes(num_blocks);

    disk_file.open(filename, fstream::in | fstream::out | fstream::binary | fstream::trunc);

    for(uint i = 0; i < num_blocks; ++i){
        disk_file.write(zeroes.data(), block_size);
    }
}

unique_ptr<FSImp::PathRet> FSImp::parse_path(string path_str) const{
    unique_ptr<PathRet> ret(new PathRet);

    //check if path is relative or absolute
    ret->final_node = pwd;
    if(path_str[0] == '/'){
        path_str.erase(0,1);
        ret->final_node = root_dir;
    }

    //initialize data structure
    ret->final_name = ret->final_node->name;
    ret->parent_node = ret->final_node->parent.lock();

    //Extracting path based on hierarchy 
    istringstream is(path_str);
    string token;
    vector<string> path_tokens;
    while(getline(is, token, '/')){
        path_tokens.push_back(token);
    }

    //walk the path updating pointers of parent and child
    for(auto &node_name : path_tokens){
        if(ret->final_node == nullptr){
            //the path has a nullptr before reaching the destination
            ret->invalid_path = true;
            return ret;
        }

        ret->parent_node = ret->final_node;
        ret->final_node = ret->final_node->find_child(node_name);
        ret->final_name = node_name;
    }
    return ret;
}

bool FSImp::getMode(Mode *mode, string mode_s){
    if(mode_s == "w") *mode = "W";
    else if(mode s == "r") *mode = "R";
    else if(mode s == "rw") *mode = "RW";
    else return false;
    return true;
}

bool FSImp::basic_open(Descriptor *d, vector<string> args){
    assert(args.size() == 3);

    Mode mode;

    auto path = parse_path(args[1]);
    auto node = path->final_node;
    auto parent = path->parent_node;
    bool known_mode = getMode(&mode, args[2]);

    if(path->invalid_path == true){
        cerr << args[0] << ": error: Invalid path: " << args[1] << endl;
    }else if(!known_mode){
        cerr << args[0] << ": error: Unknown mode: " << args[2] << endl;
    }else if(node == nullptr && (mode == "R" || mode == "RW")){
        cerr << args[0] << ": error: " << args[1] << " does not exist." << endl;
    }else if(node != nullptr && node->type == dir){
        cerr << args[0] << ": error: Cannot open a directory." << endl;
    }else if(node != nullptr && node->is_locked){
        cerr << args[0] << ": error: " << args[1] << " is already open." << endl;
    }else{
        //create the file if necessary
        if(node == nullptr){
            node = parent->add_file(path->final_name);
        } 

        //get a  descriptor
        uint fd = next_descriptor++;
        node->is_locked = true;
        *d = Descriptor{mode, 0, node->inode, node, fd};
        open_files[fd] = *d;

        return true;
    }

    return false;
}

void FSImp::open(vector<string> args){
    ops_exact(2);
    Descriptor desc;
    if(basic_open(&desc, args)){
        cout << "SUCCESS: fd = " << desc.fd << endl;
    }
}

unique_ptr<string> FSImp::basic_read(Descriptor &desc, const uint size){
    char *data = new char[size];
    char *data_p = data;
    uint &pos = desc.byte_pos;
    uint bytes_to_read = size;
    auto inode = desc.inode.lock();

    uint dbytes = direct_blocks * block_size;

    while (bytes_to_read > 0) {
    uint read_size = min(bytes_to_read, block_size - pos % block_size);
    uint read_src;
    if (pos < dbytes) {
      read_src = inode->data_blocks[pos / block_size] + pos % block_size;
    } else {
      uint i = (pos - dbytes) / (direct_blocks * block_size);
      uint j = (pos - dbytes) / block_size % direct_blocks;
      read_src = inode->inode_blocks->at(i)[j] + pos % block_size;
    }
    disk_file.seekp(read_src);
    disk_file.read(data_p, read_size);
    pos += read_size;
    data_p += read_size;
    bytes_to_read -= read_size;
  }
  return unique_ptr<string>(new string(data, size));
}

void FSImp::read(vector<string> args) {
  ops_exactly(2);

  uint fd;

  //check if the file descriptor valid
  if ( !(istringstream(args[1]) >> fd)) {
    cerr << "read: error: Unknown descriptor." << endl;
    return;
  }
  //check if the file descriptor is open
  auto desc_it = open_files.find(fd);
  if (desc_it == open_files.end()) {
    cerr << "read: error: File descriptor not open." << endl;
    return;
  }
  
  //check if read access if given to the file
  auto &desc = desc_it->second;
  if(desc.mode != R && desc.mode != RW) {
    cerr << "read: error: " << args[1] << " not open for read." << endl;
    return;
  }

  //read data from the file
  uint size;
  if (!(istringstream(args[2]) >> size)) {
    cerr << "read: error: Invalid read size." << endl;
  } else if (size + desc.byte_pos > desc.inode.lock()->size) {
    cerr << "read: error: Read goes beyond file end." << endl;
  } else {
    auto data = basic_read(desc, size);
    cout << *data << endl;
  }
}

//Helper to write to an open file based on descriptor 
uint FSImp::basic_write(Descriptor &desc, const string data) {
  const char *bytes = data.c_str();
  uint &pos = desc.byte_pos;
  uint bytes_to_write = data.size();
  uint bytes_written = 0;
  auto inode = desc.inode.lock();
  uint &file_size = inode->size;
  uint &file_blocks_used = inode->blocks_used;
  uint new_size = max(file_size, pos + bytes_to_write);
  uint new_blocks_used = ceil(static_cast<double>(new_size)/block_size);
  uint blocks_needed = new_blocks_used - file_blocks_used;
  uint dbytes = direct_blocks * block_size;

  // expand the inode to indirect blocks if needed
  if (blocks_needed && blocks_needed + file_blocks_used > 2) {
    uint ivec_used = (ceil(min(file_blocks_used - 2, 0U) / static_cast<float>(direct_blocks)));
    uint ivec_new = (ceil((new_blocks_used - 2) / static_cast<float>(direct_blocks)));
    while (ivec_used < ivec_new) {
      inode->inode_blocks->push_back(vector<uint>());
      ivec_used++;
    }
  }

  // find space
  vector<pair<uint, uint>> free_chunks;
  auto fl_it = freeNode_list.begin();
  while (blocks_needed > 0) {
    if (fl_it == freeNode_list.end()) {
      // 0 return because we ran out of free space
      return 0;}
    if (fl_it->num_blocks > blocks_needed) {
      // we found a chunk big enough to hold the rest of our write
      free_chunks.push_back(make_pair(fl_it->pos, blocks_needed));
      fl_it->pos += blocks_needed * block_size; //adjust position in the freeNode_list list.
      fl_it->num_blocks -= blocks_needed;   //number of free blocks
      break;
    }
    // we have a chunk, but will fill it and need more
    free_chunks.push_back((make_pair(fl_it->pos, fl_it->num_blocks)));
    blocks_needed -= fl_it->num_blocks;
    auto used_entry = fl_it++;
    freeNode_list.erase(used_entry);
  }

  // allocate our blocks
  for (auto fc_it : free_chunks) {
    uint block_pos = fc_it.first;
    uint num_blocks = fc_it.second;
    for (uint k = 0; k < num_blocks; ++k, ++file_blocks_used, block_pos += block_size) {
      if (file_blocks_used < direct_blocks) {
        inode->data_blocks.push_back(block_pos); //add the current data block position
      } else {
        uint i = ((file_blocks_used - direct_blocks) / direct_blocks);
        inode->inode_blocks->at(i).push_back(block_pos);    // add the current inode block position
      }
    }
  }

  // actually write our blocks
  while (bytes_to_write > 0) {
    uint write_size = min(block_size - pos % block_size, bytes_to_write);
    uint write_dest = 0;
    if (pos < dbytes) {
      write_dest = inode->data_blocks[pos / block_size] + pos % block_size;
    } else {
      uint i = (pos - dbytes) / (direct_blocks * block_size);
      uint j = (pos - dbytes) / block_size % direct_blocks;
      write_dest = inode->inode_blocks->at(i)[j] + pos % block_size;
    }
    disk_file.seekp(write_dest);
    disk_file.write(bytes + bytes_written, write_size);
    bytes_written += write_size;
    bytes_to_write -= write_size;
    pos += write_size;
  }

  disk_file.flush();
  file_size = new_size;
  return bytes_written;
}

//Write to the file
void FSImp::write(vector<string> args) {
  ops_exactly(2);

  uint fd;
  //check for limit on the file size
  uint max_size = block_size * (direct_blocks + direct_blocks * direct_blocks);
  if ( !(istringstream(args[1]) >> fd)) {
    cerr << "write: error: Unknown descriptor." << endl;
  } else {
    auto desc = open_files.find(fd);
    if (desc == open_files.end()) {
      cerr << "write: error: File descriptor not open." << endl;
    } else if (desc->second.mode != W && desc->second.mode != RW) {
      cerr << "write: error: " << args[1] << " not open for write." << endl;
    } else if (desc->second.byte_pos + args[2].size() > max_size) {
      cerr << "write: error: File to large for inode." << endl;
    } else if (!basic_write(desc->second, args[2])) {
      cerr << "write: error: Insufficient disk space." << endl;
    }
  }
}

//Helper to remove the file from open_files map and unlock it so that file can be accessed by other processes
bool FSImp::basic_close(uint fd) {
  auto kv = open_files.find(fd);
  if(kv == open_files.end()) {
    return false;
  } else {
    kv->second.from.lock()->is_locked = false;
    open_files.erase(fd);
  }
  return true;
}

//close the file using helper function.
void FSImp::close(vector<string> args) {
  ops_exactly(1);
  uint fd;

  if (! (istringstream (args[1]) >> fd)) {
    cerr << "close: error: File descriptor not recognized" << endl;
  } else {
    if (!basic_close(fd)) {
      cerr << "close: error: File descriptor not open" << endl;
    } else {
      cout << "closed " << fd << endl;
    }
  }
}

//create directory.
void FSImp::mkdir(vector<string> args) {
  ops_at_least(1);
  /* add each new directory one at a time */
  for (uint i = 1; i < args.size(); i++) {
    auto path = parse_path(args[i]);
    auto node = path->final_node;
    auto dirname = path->final_name;
    auto parent = path->parent_node;

    if (path->invalid_path) {
      cerr << "mkdir: error: Invalid path: " << args[i] << endl;
      return;
    } else if (node == root_dir) {
      cerr << "mkdir: error: Cannot recreate root." << endl;
      return;
    } else if (node != nullptr) {
      cerr << "mkdir: error: " << args[i] << " already exists." << endl;
      continue;
    }

    /* actually add the directory */
    parent->add_dir(dirname);
  }
}

//Get the directory name and its parent.
void FSImp::rmdir(vector<string> args) {
  ops_at_least(1);

  for (uint i = 1; i < args.size(); i++) {
    auto path = parse_path(args[i]);
    auto node = path->final_node;
    auto parent = path->parent_node;

    if (node == nullptr) {
      cerr << "rmdir: error: Invalid path: " << args[i] << endl;
    } else if (node == root_dir) {
      cerr << "rmdir: error: Cannot remove root." << endl;
    } else if (node == pwd) {
      cerr << "rmdir: error: Cannot remove working directory." << endl;
    } else if (node->contents.size() > 0) {
      cerr << "rmdir: error: Directory not empty." << endl;
    } else if (node->type != dir) {
      cerr << "rmdir: error: " << node->name << " must be directory." << endl;
    } else {
      parent->contents.remove(node);
    }
  }
}

//use a deque to store the path to the root directory and print each element in it.
void FSImp::printwd(vector<string> args) {
  ops_exactly(0);

  if (pwd == root_dir) {
      cout << "/" << endl;
      return;
  }

  auto wd = pwd;
  deque<string> plist;
  while (wd != root_dir) {
    plist.push_front(wd->name);
    wd = wd->parent.lock();
  }

  for (auto dirname : plist) {
      cout << "/" << dirname;
  }
  cout << endl;
}

//check if the directory is valid. If it is make it the working directory 
void FSImp::cd(vector<string> args) {
  ops_exactly(1);

  auto path = parse_path(args[1]);
  auto node = path->final_node;

  if (node == nullptr) {
    cerr << "cd: error: Invalid path: " << args[1] << endl;
  } else if (node->type != dir) {
    cerr << "cd: error: " << args[1] << " must be a directory." << endl;
  } else {
    pwd = node;
  }
}

//get the paths for source and destination. add the new file to the destination
void FSImp::link(vector<string> args) {
  ops_exactly(2);

  auto src_path = parse_path(args[1]);
  auto src = src_path->final_node;
  auto src_parent = src_path->parent_node;
  auto dest_path = parse_path(args[2]);
  auto dest = dest_path->final_node;
  auto dest_parent = dest_path->parent_node;
  auto dest_name = dest_path->final_name;

  if (src == nullptr) {
    cerr << "link: error: Cannot find " << args[1] << endl;
  } else if (dest != nullptr) {
    cerr << "link: error: " << args[2] << " already exists." << endl;
  } else if (src->type != file) {
    cerr << "link: error: " << args[1] << " must be a file." << endl;
  } else if (src_parent == dest_parent) {
    cerr << "link: error: src and dest must be in different directories." << endl;
  } else {
    auto new_file = DirEntry::make_file(dest_name, dest_parent, src->inode);
    dest_parent->contents.push_back(new_file);
  }
}

//
void FSImp::unlink(vector<string> args) {
  ops_exactly(1);

  auto path = parse_path(args[1]);
  auto node = path->final_node;
  auto parent = path->parent_node;

  if (node == nullptr) {
    cerr << "unlink: error: File not found." << endl;
  } else if (node->type != file) {
    cerr << "unlink: error: " << args[1] << " must be a file." << endl;
  } else if (node->is_locked) {
    cerr << "unlink: error: " << args[1] << " is open." << endl;
  } else {
    parent->contents.remove(node);
  }
}

//Print some stats related to input 
void FSImp::stat(vector<string> args) {
  ops_at_least(1);

  for (uint i = 1; i < args.size(); i++) {
    auto path = parse_path(args[i]);
    auto node = path->final_node;

    if (node == nullptr) {
      cerr << "stat: error: " << args[i] << " not found." << endl;
    } else {
      cout << "  File: " << node->name << endl;
      if (node->type == file) {
        cout << "  Type: file" << endl;
        cout << " Inode: " << node->inode.get() << endl;
        cout << " Links: " << node->inode.use_count() << endl;
        cout << "  Size: " << node->inode->size << endl;
        cout << "Blocks: " << node->inode->blocks_used << endl;
      } else if(node->type == dir) {
        cout << "  Type: directory" << endl;
      }
    }
  }
}

//print the contents of current directory
void FSImp::ls(vector<string> args) {
  ops_exactly(0);
  for (auto dir : pwd->contents) {
    cout << dir->name << endl;
  }
}

//
void FSImp::cat(vector<string> args) {
  ops_at_least(1);

  for(uint i = 1; i < args.size(); i++) {
    Descriptor desc;
    if(!basic_open(&desc, vector<string> {args[0], args[i], "r"})) {
      /* failed to open */
      continue;
    }
    
    auto size = desc.inode.lock()->size;
    read(vector<string>
            {args[0], std::to_string(desc.fd), std::to_string(size)});
    basic_close(desc.fd);
  }
}

//
void FSImp::cp(vector<string> args) {
  ops_exactly(2);
  
  Descriptor src, dest;
  if(basic_open(&src, vector<string> {args[0], args[1], "r"})) {
    if(!basic_open(&dest, vector<string> {args[0], args[2], "w"})) {
      basic_close(src.fd);
    } else {
      auto data = basic_read(src, src.inode.lock()->size);
      if (!basic_write(dest, *data)) {
        cerr << args[0] << ": error: out of free space or file too large"
             << endl;
      }
      basic_close(src.fd);
      basic_close(dest.fd);
    }
  }
}