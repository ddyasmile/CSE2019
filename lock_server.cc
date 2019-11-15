// the lock server implementation

#include "lock_server.h"

#include <map>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#define LOCKED false
#define FREE true

struct lock_data
{
  pthread_cond_t _cond;
  bool _lock;
  int _clt_id;
};


pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
std::map<lock_protocol::lockid_t, lock_data> lock_map;

lock_server::lock_server():
  nacquire (0)
{
  lock_map.clear();
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  printf("acquire request from clt %d\n", clt);

  pthread_mutex_lock(&mut);

  if (lock_map.count(lid)) {
    while (!lock_map[lid]._lock)
    {
      pthread_cond_wait(&lock_map[lid]._cond, &mut);
    }
    lock_map[lid]._lock = LOCKED;
    lock_map[lid]._clt_id = clt;
    pthread_mutex_unlock(&mut);
    return ret;
  }else{
    lock_map[lid]._cond = PTHREAD_COND_INITIALIZER;
    lock_map[lid]._lock = LOCKED;
    lock_map[lid]._clt_id = clt;
    pthread_mutex_unlock(&mut);
    return ret;
  }
  
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  printf("release request from clt %d\n", clt);

  pthread_mutex_lock(&mut);

  while (lock_map[lid]._clt_id!=clt||lock_map[lid]._lock)
  {
    pthread_cond_wait(&lock_map[lid]._cond, &mut);
  }

  lock_map[lid]._lock = FREE;
  pthread_cond_signal(&lock_map[lid]._cond);
  pthread_mutex_unlock(&mut);

  return ret;
}
