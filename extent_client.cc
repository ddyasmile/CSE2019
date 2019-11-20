// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client::last_port = 1024;

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  srand(time(NULL)^last_port);
  rextent_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rextent_port;
  clt_id = host.str();
  last_port = rextent_port;
  rpcs *resrpc = new rpcs(rextent_port);
  resrpc->reg(rextent_protocol::flush, this, &extent_client::flush_handler);
  resrpc->reg(rextent_protocol::flush_rm, this, &extent_client::flush_rm_handler);
  resrpc->reg(rextent_protocol::get_attr, this, &extent_client::get_attr_handler);
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  ret = cl->call(extent_protocol::create, clt_id, type, id);
  std::cout<<"create: "<< id <<std::endl;
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  std::map<extent_protocol::extentid_t, FileData *>::iterator iter;
  iter = fileManager.find(eid);

  if (iter == fileManager.end()) {
    fileManager[eid] = new FileData();
  }

  CacheState stat = fileManager[eid]->data_stat;
  if (stat == NONE) {
    ret = cl->call(extent_protocol::get, clt_id, eid, buf);
    fileManager[eid]->file_data = buf;
    fileManager[eid]->data_stat = OK;
    if (fileManager[eid]->attr_stat == NONE) {
      getattr(eid, fileManager[eid]->file_attr);
    }
  }else if (stat == OK) {
    buf = fileManager[eid]->file_data;
  }

  unsigned int now_time = (unsigned int)time(NULL);
  fileManager[eid]->file_attr.atime = now_time;

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  std::map<extent_protocol::extentid_t, FileData *>::iterator iter;
  iter = fileManager.find(eid);

  if (iter == fileManager.end()) {
    fileManager[eid] = new FileData();
  }

  CacheState stat = fileManager[eid]->attr_stat;
  if (stat == NONE) {
    std::cout<<"getattr[uncache]: "<<eid<<std::endl;
    ret = cl->call(extent_protocol::getattr, clt_id, eid, attr);
    fileManager[eid]->file_attr = attr;
    fileManager[eid]->attr_stat = OK;
  }else if (stat == OK) {
    std::cout<<"getattr[cache]: "<<eid<<std::endl;
    attr = fileManager[eid]->file_attr;
  }
  // ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  std::map<extent_protocol::extentid_t, FileData *>::iterator iter;
  iter = fileManager.find(eid);

  if (iter == fileManager.end()) {
    fileManager[eid] = new FileData();
  }

  if (fileManager[eid]->data_stat == NONE ||
      fileManager[eid]->attr_stat == NONE) {
    get(eid, fileManager[eid]->file_data);
  }

  unsigned int now_time = (unsigned int)time(NULL);
  fileManager[eid]->file_data = buf;
  fileManager[eid]->file_attr.size = buf.size();
  fileManager[eid]->file_attr.ctime = now_time;
  fileManager[eid]->file_attr.mtime = now_time;

  fileManager[eid]->data_stat = OK;
  fileManager[eid]->attr_stat = OK;

  // ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  ret = cl->call(extent_protocol::remove, clt_id, eid, r);
  return ret;
}

rextent_protocol::status
extent_client::flush_handler(extent_protocol::extentid_t eid, int &r)
{
  int ret = rextent_protocol::OK;
  std::map<extent_protocol::extentid_t, FileData *>::iterator iter;

  iter = fileManager.find(eid);
  if (iter == fileManager.end()) {
    return ret;
  }
  if (fileManager[eid]->data_stat == NONE) {
    return ret;
  }

  int rr;
  std::string buf = fileManager[eid]->file_data;
  ret = cl->call(extent_protocol::put, clt_id, eid, buf, rr);

  fileManager[eid]->data_stat = NONE;
  fileManager[eid]->attr_stat = NONE;
  return ret;
}

rextent_protocol::status
extent_client::flush_rm_handler(extent_protocol::extentid_t eid, int &r)
{
  int ret = rextent_protocol::OK;
  // std::map<extent_protocol::extentid_t, FileData *>::iterator iter;
  // iter = fileManager.find(eid);
  // if (iter == fileManager.end()) {
  //  return ret;
  // }

  fileManager[eid]->data_stat = NONE;
  fileManager[eid]->attr_stat = NONE;

  return ret;
}

rextent_protocol::status
extent_client::get_attr_handler(extent_protocol::extentid_t eid, 
                                extent_protocol::attr &a)
{
  int ret = rextent_protocol::OK;
  // std::map<extent_protocol::extentid_t, FileData *>::iterator iter;
  // iter = fileManager.find(eid);
  // if (iter == fileManager.end()) {
  //  return ret;
  // }
  // if (fileManager[eid]->data_stat==NONE) {
  //  return ret;
  // }

  a = fileManager[eid]->file_attr;
  return ret;
}


