#include "dirEntry.hpp"
#include <algorithm>
#include <sstream>
#include <vector>

using std::find_if; //returns the first element that satisfies the unary function. Returns the last element if otherwise
using std::istringstream; //used to convert int->string or string->int
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;

DirEntry::DirEntry(){
    is_locked = false;
}

shared_ptr<DirEntry> DirEntry::make_dir(const string name, 
                                        const shared_ptr<DirEntry> parent){
    
    auto sp = make_shared<DirEntry>(DirEntry());
    if(parent == nullptr){
        sp->parent = sp;
    } else {
        sp->parent = parent;
    }

    sp->type = dir;
    sp->self = sp;
    sp->name = name;
    sp->inode = nullptr;
    return sp;
}

shared_ptr<DirEntry> DirEntry::make_file(const string name, 
                                         const shared_ptr<DirEntry> parent, 
                                         const std::shared_ptr<Inode> &inode = nullptr){
    
    auto sp = make_shared<DirEntry>(DirEntry());
    if(parent == nullptr){
        sp->parent = sp;
    } else {
        sp->parent = parent;
    }

    sp->type = file;
    sp->self = sp;
    sp->name = name;
    sp->inode = nullptr;
    return sp;
}

shared_ptr<DirEntry> DirEntry::find_child(const string name) const {
    //handle current and parent directories
    if(name == ".") return self.lock();
    else if(name == "..") return parent.lock();

    //search through contents and return pointer if found, else return nullptr;
    auto names = [&] (const shared_ptr<DirEntry> de) {return de->name == name;};
    auto it = find_if(contents.begin(), contents.end(), named);
    
    if(it == contents.end()) return nullptr;

    return *it;

}

shared_ptr<DirEntry> DirEntry::add_dir(const string name){
    auto new_dir = make_dir(name, self.lock());
    contents.push_back(new_dir);
    return new_dir;
}

shared_ptr<DirEntry> DirEntry::add_file(const string name){
    auto new_file = make_file(name, self.lock(), make_shared<Inode>());
    contents.push_back(new_file);
    return new_file;
}