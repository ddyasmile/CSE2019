// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(std::string clt, uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  return extent_protocol::OK;
}

int extent_server::put(std::string clt, extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(std::string clt, extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);
  std::map<extent_protocol::extentid_t, std::string>::iterator iter;

  id &= 0x7fffffff;
  int r;
  int size = 0;
  char *cbuf = NULL;

  iter = fileManger.find(id);
  if (iter == fileManger.end()) {
    fileManger[id] = clt;
  }

  std::string owner = fileManger[id];
  if (owner.compare(clt)!=0) {
    handle(owner).safebind()->call(rextent_protocol::flush, id, r);
  }

  fileManger[id] = clt;
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(std::string clt, extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);
  int r;
  std::map<extent_protocol::extentid_t, std::string>::iterator iter;

  id &= 0x7fffffff;

  iter = fileManger.find(id);
  if (iter == fileManger.end()) {
    fileManger[id] = clt;
  }

  extent_protocol::attr attr;
  std::string owner = fileManger[id];
  if (owner.compare(clt)!=0) {
    handle(owner).safebind()->call(rextent_protocol::get_attr, id, attr);
  }else{
    memset(&attr, 0, sizeof(attr));
    im->getattr(id, attr);
  }

  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(std::string clt, extent_protocol::extentid_t id, int &)
{
  printf("extent_server: remove %lld\n", id);
  int r;
  std::map<extent_protocol::extentid_t, std::string>::iterator iter;

  id &= 0x7fffffff;

  iter = fileManger.find(id);
  if (iter == fileManger.end()) {
    fileManger[id] = clt;
  }

  std::string owner = fileManger[id];
  if (owner.compare(clt)!=0) {
    handle(owner).safebind()->call(rextent_protocol::flush_rm, id, r);
  }
  fileManger[id] = "";
  im->remove_file(id);
 
  return extent_protocol::OK;
}

