// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <map>

lock_server::lock_server():
  nacquire (0)
{
	pthread_mutex_init(&mutex, NULL);
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
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);  // get map lock
  if(lock_map.find(lid) != lock_map.end()){  // if the lock exist
	  while(lock_map[lid].granted == true){  // cond wake up, check granted
		  pthread_cond_wait(&lock_map[lid].cond, &mutex);
		  // unlock mutex before sleep	
		  // lock mutex when wake up
	  }
  }
	
  // if the lock do not exist, just get it
  // ok, wake up and get the lock
  lock_map[lid].granted = true;

  pthread_mutex_unlock(&mutex);  // release map lock
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here

  // prevent a acquire and a release sent together and the lock will always be granted
  pthread_mutex_lock(&mutex);
  lock_map[lid].granted = false;
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&lock_map[lid].cond);  // wake up another thread
  return ret;
}
