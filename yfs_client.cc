// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  cache_map.clear();
  es = new extent_server();
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::finddir(std::string directory, std::string name)
{
    inum finum = 0;
    char delim1 = '\n';
    char delim2 = '/';
    std::string line;
    std::stringstream strstream(directory);

    while (std::getline(strstream, line, delim1)) {
        std::string cur_name;
        std::stringstream linestream(line);
        std::getline(linestream, cur_name, delim2);
        if (cur_name.compare(name)==0) {
            linestream >> finum;
            break;
        }
    }
    return finum;
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    if(n[0]!='/') return 0;

    inum finum = 1;
    char delim = '/';
    std::string name;
    std::stringstream strstream(n);

    std::getline(strstream, name, delim);
    if (name.compare("")==0) {}

    extent_protocol::attr a;
    while (std::getline(strstream, name, delim)) {
        if (name.compare("")==0)
            break;
        ec->getattr(finum, a);
        if (a.type==extent_protocol::T_FILE)
            break;
        if (a.type==extent_protocol::T_DIR) {
            std::string directory;
            ec->get(finum, directory);
            finum = finddir(directory, name);
        }
    }
    
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        lc->release(inum);
        return true;
    } 
    lc->release(inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */
bool
yfs_client::islink(inum inum)
{
    extent_protocol::attr a;

    lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_LINK) {
        printf("isfile: %lld is a link\n", inum);
        lc->release(inum);
        return true;
    } 
    lc->release(inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    lc->acquire(inum);

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        lc->release(inum);
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        lc->release(inum);
        return true;
    } 
    lc->release(inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    lc->acquire(inum);

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    lc->acquire(inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    lc->acquire(ino);

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::attr a;

    //std::cout << "setattr: " << std::endl;

    if (ec->getattr(ino, a)!=extent_protocol::OK) {
        r = RPCERR;
        lc->release(ino);
        return r;
    }

    if (a.size==size) {
        lc->release(ino);
        return r;
    }else {
        std::string file_str;
        if (ec->get(ino, file_str)!=extent_protocol::OK) {
            r = RPCERR;
            lc->release(ino);
            return r;
        }

        if (a.size>size) {
            file_str.erase(size, a.size);
        }else{
            int length = size - a.size;
            //std::cout << "length: " << length << std::endl;
            char fill[length];
            for (int i = 0; i < length; i++)
                fill[i] = 0;
            std::string fill_str;
            fill_str.assign(fill, length);
            file_str.append(fill_str);
        }

        if (ec->put(ino, file_str)!=extent_protocol::OK) {
            r = RPCERR;
            lc->release(ino);
            return r;
        }
    }

    lc->release(ino);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    inum create_flag = 0;

    lc->acquire(create_flag);
    lc->acquire(parent);

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    std::string dir_str;
    std::string name_str = name;

    if (ec->get(parent, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(create_flag);
        lc->release(parent);
        return r;
    }
    yfs_client::directory direct(dir_str);

    if (direct.find(name_str)!=0) {
        r = EXIST;
    }else if (ec->create(extent_protocol::T_FILE, ino_out)!=extent_protocol::OK) {
        r = CREERR;
    }else{
        dirent dire;
        dire.name = name_str;
        dire.inum = ino_out;
        direct.addent(dire);
        if (ec->put(parent, direct.tostring())!=extent_protocol::OK) {
            r = WRTERR;
        }
        //printf("\tdirectory: %s", direct.tostring().c_str());
    }
    lc->release(create_flag);
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    inum create_flag = 0;

    lc->acquire(create_flag);
    lc->acquire(parent);

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    std::string dir_str;
    std::string name_str = name;

    if (ec->get(parent, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(create_flag);
        lc->release(parent);
        return r;
    }
    yfs_client::directory direct(dir_str);

    if (direct.find(name_str)!=0) {
        r = EXIST;
    }else if (ec->create(extent_protocol::T_DIR, ino_out)!=extent_protocol::OK) {
        r = CREERR;
    }else{
        dirent dire;
        dire.name = name_str;
        dire.inum = ino_out;
        direct.addent(dire);
        if (ec->put(parent, direct.tostring())!=extent_protocol::OK) {
            r = WRTERR;
        }
        //printf("\tdirectory: %s", direct.tostring().c_str());
    }
    lc->release(create_flag);
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    lc->acquire(parent);

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::string dir_str;
    std::string name_str = name;

    //std::cout <<  "name_str: " << name_str << std::endl;

    if (ec->get(parent, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(parent);
        return r;
    }
    yfs_client::directory direct(dir_str);

    if ((ino_out = direct.find(name_str))==0) {
        std::cout << ino_out << std::endl;
        r = NOENT;
        found = false;
    }else{
        found = true;
    }

    lc->release(parent);
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    lc->acquire(dir);

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string dir_str;

    if (ec->get(dir, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(dir);
        return r;
    }
    yfs_client::directory direct(dir_str);
    direct.getdir(list);

    lc->release(dir);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    lc->acquire(ino);

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string file_str;
    if (ec->get(ino, file_str)!=extent_protocol::OK) {
        r = RPCERR;
        lc->release(ino);
        return r;
    }
    
    if(off >= file_str.size()) {
        data = "";
    }else{
        data = file_str.substr(off, size);
    }

    lc->release(ino);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    lc->acquire(ino);

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string file_str;
    if (ec->get(ino, file_str)!=extent_protocol::OK) {
        r = RPCERR;
        lc->release(ino);
        return r;
    }
    int file_size = file_str.size();

    //std::cout << "file_size: " << file_size << std::endl;
    //std::cout << "off: " << off << std::endl;
    std::string data_str;
    data_str.assign(data, size);
    if (off < file_size) {
        file_str.replace(off, size, data_str);
    }else{
        int length = off - file_size;
        char fill[length];
        std::cout << "length: " << length << std::endl;
        for (int i = 0; i < length; i++)
            fill[i] = 0;
        std::string fill_str;
        fill_str.assign(fill, length);
        std::cout << "fill: " << fill_str << std::endl;
        file_str.append(fill_str);
        file_str.append(data_str, 0, size);
    }

    if (ec->put(ino, file_str)!=extent_protocol::OK)
        r = RPCERR;

    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    lc->acquire(parent);

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    std::string dir_str;
    std::string name_str = name;
    yfs_client::inum finum;

    if (ec->get(parent, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(parent);
        return r;
    }
    yfs_client::directory direct(dir_str);

    if (finum = direct.find(name_str)==0) {
        lc->release(parent);
        return r;
    }else{
        direct.delent(name_str);
        if (ec->put(parent, direct.tostring())!=extent_protocol::OK) {
            r = WRTERR;
        }
    }

    if (!finum == 0) {
        lc->acquire(finum);
        ec->remove(finum);
        lc->release(finum);
    }
    
    lc->release(parent);
    return r;
}

int 
yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino_out)
{
    int r = OK;
    std::string dir_str;
    std::string name_str = name, link_str = link;
    inum create_flag = 0;

    printf("symlink test\n");
    lc->acquire(create_flag);
    lc->acquire(parent);

    if (ec->get(parent, dir_str)!=extent_protocol::OK) {
        r = READERR;
        lc->release(create_flag);
        lc->release(parent);
        return r;
    }
    yfs_client::directory direct(dir_str);

    if (direct.find(name_str)!=0) {
        r = EXIST;
    }else if (ec->create(extent_protocol::T_LINK, ino_out)!=extent_protocol::OK) {
        r = CREERR;
    }else{
        ec->put(ino_out, link_str);

        dirent dire;
        dire.name = name_str;
        dire.inum = ino_out;
        direct.addent(dire);
        if (ec->put(parent, direct.tostring())!=extent_protocol::OK) {
            r = WRTERR;
        }
        //printf("\tdirectory: %s", direct.tostring().c_str());
    }
    lc->release(create_flag);
    lc->release(parent);
    return r;
}

int
yfs_client::readlink(inum ino, std::string &link)
{
    int r =  OK;
    lc->acquire(ino);
    if (ec->get(ino, link)!=extent_protocol::OK) {
        r = RPCERR;
        return r;
    }

    lc->release(ino);
    return r;
}





yfs_client::directory::directory(std::string dir_str) 
{
    dir_string = dir_str;

    inum finum;
    char delim1 = '/';
    char delim2 = '/';
    std::string name;
    std::stringstream strstream(dir_str);

    while (std::getline(strstream, name, delim1))
    {
        std::string str_finum;
        std::getline(strstream, str_finum, delim2);
        std::stringstream inumss(str_finum);
        inumss >> finum;
        dirent dire;
        dire.name = name;
        dire.inum = finum;
        dir.push_back(dire);
    }
}

yfs_client::inum
yfs_client::directory::find(std::string name)
{
    inum finum = 0;
    std::list<dirent>::iterator it = dir.begin();
    for(it; it != dir.end(); it++) {
        //std::cout << "it->name: " << it->name << std::endl;
        //std::cout << "name: " << name << std::endl;
        //std::cout << "name.compare(it->name): " << name.compare(it->name) << std::endl;
        if(name.compare(it->name)==0) {
            finum = it->inum;
            break;
        }
    }
    return finum;
}

void
yfs_client::directory::getdir(std::list<dirent> &list)
{
    std::list<dirent>::iterator it = dir.begin();
    for(it; it != dir.end(); it++) {
        dirent dire;
        dire.name = it->name;
        dire.inum = it->inum;
        list.push_back(dire);
    } 
}

void
yfs_client::directory::addent(yfs_client::dirent dire)
{
    std::string str1 = "/", str2 = "/";
    std::stringstream ss;
    ss << dire.name << str1 << dire.inum << str2;
    dir_string += ss.str();
    dir.push_back(dire);
}

void
yfs_client::directory::delent(std::string name)
{
    std::stringstream new_string_stream("");
    std::string delim = "/";
    std::list<dirent> new_list;
    std::list<dirent>::iterator it = dir.begin();
    for(it; it != dir.end(); it++) {
        if(name.compare(it->name)!=0) {
            dirent dire;
            dire.name = it->name;
            dire.inum = it->inum;
            new_list.push_back(dire);
            new_string_stream << it->name << delim;
            new_string_stream << it->inum << delim;
        }
    }
    dir = new_list;
    dir_string = new_string_stream.str();
}

std::string
yfs_client::directory::tostring()
{
    return dir_string;
}

bool
yfs_client::cache_miss(inum finum)
{
  return true;
}

bool
yfs_client::write_through(inum finum)
{
  return true;
}

