// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
 private:
  rpcc *cl;
  int rextent_port;
  std::string clt_id;

  enum CacheState {
    NONE,
    OK
  };

  struct FileData {
    CacheState data_stat;
    CacheState attr_stat;
    std::string file_data;
    extent_protocol::attr file_attr;
    FileData () {
      data_stat = NONE;
      attr_stat = NONE;
      file_data = "";
    }
  };

  std::map<extent_protocol::extentid_t, FileData *> fileManager;
  

 public:
  static int last_port;
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  rextent_protocol::status flush_handler(extent_protocol::extentid_t, int &);
  rextent_protocol::status flush_rm_handler(extent_protocol::extentid_t, int &);
  rextent_protocol::status get_attr_handler(extent_protocol::extentid_t, extent_protocol::attr &);
};

#endif 

