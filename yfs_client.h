#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST, CREERR, WRTERR, READERR };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };
  class directory {
    private:
      std::string dir_string;
      std::list<yfs_client::dirent> dir;
    public:
      directory(std::string);
      yfs_client::inum find(std::string);
      void getdir(std::list<dirent> &);
      void addent(yfs_client::dirent);
      void delent(std::string);
      std::string tostring();
  };

 private:
  std::string filename(inum);
  inum n2i(std::string);
  inum finddir(std::string, std::string);

 public:
  yfs_client();
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool islink(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  
  /** you may need to add symbolic link related methods here.*/
  int symlink(inum, const char*, const char*, inum &);
  int readlink(inum, std::string &);
};

#endif 
