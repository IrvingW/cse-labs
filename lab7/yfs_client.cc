// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
  ec = NULL;
  lc = NULL;
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst, const char* cert_file)
{
	ec = new extent_client(extent_dst);
	lc = new lock_client(lock_dst);
	cur_version = 0;
	recovering = 0;
	lc->acquire(1);
	if (ec->put(1, "") != extent_protocol::OK)
	printf("error init root dir\n"); // XYB: init root dir
	lc->release(1);
}

int
yfs_client::verify(const char* name, unsigned short *uid)
{
  	int ret = OK;

	return ret;
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
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
	lc->release(inum);

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

// implete functions of DirTable
yfs_client::DirTable::DirTable(std::string s)
{
	size_t position = 0;
	size_t next;

	while(position < s.size()){
		next = s.find("/", position);
		std::string file_name = s.substr(position, next-position);
		position = next + 1;

		next = s.find("/", position);
		inum id = n2i(s.substr(position, next-position));
		position = next + 1;

		list.insert(std::pair<std::string, inum>(file_name, id));
	}
}

// dump all file contains in directory.
// return a string seperated by "/"
std::string
yfs_client::DirTable::dump2str()
{
	std::string result = "";
	std::map<std::string, inum>::iterator it = list.begin();
	while(it != list.end()){
		result = result + it->first + "/";
		result = result + filename(it->second) + "/";
		
		it++;
	}

	return result;
}

// look up ,return whether the directory contain a certain file

bool
yfs_client::DirTable::lookup(std::string name, inum& result)
{
	std::map<std::string, inum>::iterator it = list.find(name);
	if(it != list.end()){
		result = it->second;
		return true;
	}else{
		return false;
	}

}

// insert a new item into directory table list
void
yfs_client::DirTable::insert(std::string name, inum id)
{
	list.insert(std::pair<std::string, inum>(name, id));
}

// list all file in this directory
void
yfs_client::DirTable::dump2list(std::list<dirent>& target_list)
{
	std::map<std::string, inum>::iterator it = list.begin();
	while(it != list.end()){
		struct dirent entry;
		entry.name = it->first;
		entry.inum = it->second;
		target_list.push_back(entry);

		it++;
	}

}

// erase one file from directory table
void
yfs_client::DirTable::erase(std::string name)
{
	list.erase(name);
}

/*===============================================================*/



bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    // shit !! I have wasted three hours on this shit!!
	// Why not just tell me in doc ??

	extent_protocol::attr a;

	// get attr
	lc->acquire(inum);
	if(ec->getattr(inum, a) != extent_protocol::OK){
		printf("error getting attr\n");
		lc->release(inum);
		return false;
	}
	lc->release(inum);

	if(a.type == extent_protocol::T_DIR){
		printf("isdir: %lld is a directory\n", inum);
		return true;
	}

	printf("isdir: %lld is not a diretory\n", inum);
	return false;
	
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
	
	lc->acquire(inum);
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

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
	
	lc->acquire(inum);
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
yfs_client::setattr(inum ino, filestat st, unsigned long toset)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
	lc->acquire(ino);
	
	// write log
	log_setattr(ino, st.size);

	std::string buf;
	r = ec->get(ino, buf);
	if(r != OK){
		printf("setattr: file do not exist\n");
		lc->release(ino);
		return r;
	}

	// adjust size
	buf.resize(st.size);
	// write back
	r = ec->put(ino, buf);

	lc->release(ino);
	
	if(r != OK){
		printf("setattr: update file failure\n");
		return r;
	}
	
    return r;
}



int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

	lc->acquire(parent);

	// write log
	log_create(parent, name, mode);
	
	std::string buf;
	r = ec->get(parent, buf);   // read parent directory block content

	if(r != OK){
		printf("create: parent directory do not exist\n");
		lc->release(parent);
		return r;
	}

	// check if this filename already exist
	inum id;
	DirTable parent_dir(buf);
	if(parent_dir.lookup(std::string(name), id)){
		printf("create: name already exist\n");
		lc->release(parent);
		return EXIST;
	}
	
	// create file
	r = ec->create(extent_protocol::T_FILE, ino_out);
	if(r != OK){
		printf("create: create file failure\n");
		lc->release(parent);
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
	
	lc->release(parent);

	if(r != OK){
		printf("create: update parent directory failure\n");
		return r;
	}	

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

	std::string buf;
	
	lc->acquire(parent);

	// write log
	log_mkdir(parent, name, mode);

	r = ec->get(parent, buf);   // read parent directory block content
	if(r != OK){
		printf("mkdir: parent directory do not exist\n");
		lc->release(parent);
		return r;
	}

	// check if this filename already exist
	inum id;
	DirTable parent_dir(buf);
	if(parent_dir.lookup(std::string(name), id)){
		printf("mkdir: name already exist\n");
		lc->release(parent);
		return EXIST;
	}
	
	// create file
	r = ec->create(extent_protocol::T_DIR, ino_out);
	if(r != OK){
		printf("mkdir: create file failure\n");
		lc->release(parent);
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
	
	lc->release(parent);
	
	if(r != OK){
		printf("mkdir: update parent directory failure\n");
		return r;
	}	

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
	std::string buf;
	lc->acquire(parent);
	r = ec->get(parent, buf);
	lc->release(parent);
	if(r != OK){
		printf("lookup: parent directory do not exist\n");
		lc->release(parent);
		return r;
	}

	DirTable parent_dir(buf);
	found =	parent_dir.lookup(std::string(name), ino_out);

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
	std::string buf;
	lc->acquire(dir);
	r = ec->get(dir, buf);
	lc->release(dir);
	if(r != OK){
		printf("readdir: directory do not exist\n");
		return r;
	}

	DirTable parent_dir(buf);
	parent_dir.dump2list(list);

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
	std::string buf;
	lc->acquire(ino);
	r = ec->get(ino, buf);
	lc->release(ino);
	if(r != OK){
		printf("read: file do not exist\n");
		return r;
	}
	unsigned int origin_size = (unsigned int) buf.size();
	
	if(off < origin_size){
		if((off + size) <= origin_size){  
			data = buf.substr(off, size);
		}else{
			data = buf.substr(off, origin_size-off);
		}
	}

	else{
		data = "";
	}

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
     
	/*
	 * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
	std::string buf;
	
	lc->acquire(ino);

	// write log
	log_write(ino, size, off, data);

	r = ec->get(ino, buf);
	if(r != OK){
		printf("write: file do not exist\n");
		lc->release(ino);
		return r;
	}
	//unsigned int origin_size = (unsigned int) buf.length();

	if((size_t)(off + size) > buf.length()){  // expanding file if need
		buf.resize(off+size, '\0');
	}

	for(unsigned int i = 0; i < size; i++){
		buf[off+i] = data[i];
	}

	r = ec->put(ino, buf);
	lc->release(ino);

	if(r != OK){
		printf("write: updata file failure\n");
		return r;
	}
	bytes_written = size;
	
    return r;
}


int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
	std::string buf;
	lc->acquire(parent);

	// write log
	log_unlink(parent, name);

	r = ec->get(parent, buf);
	if(r != OK){
		printf("unlink: parent directory do not exist\n");
		lc->release(parent);
		return r;
	}

	inum file_id;
	DirTable parent_dir(buf);
	if(!parent_dir.lookup((std::string)name, file_id)){
		printf("unlink: file do not exist\n");
		lc->release(parent);
		return NOENT;
	}

	// remove file
	r = ec->remove(file_id);
	if(r != OK){
		printf("unlink: remove failure\n");
		lc->release(parent);
		return r;
	}

	// update directory
	parent_dir.erase(std::string(name));
	r = ec->put(parent, parent_dir.dump2str());
	lc->release(parent);
	
	if(r != OK){
		printf("unlink: remove failure\n");
		return r;
	}


    return r;
}

int
yfs_client::symlink(inum parent, const char *name,
			const char *path, inum &ino_out)
{
	int r = OK;
	
	std::string buf;
	lc->acquire(parent);

	// write log
	log_symlink(parent, name, path);

	r = ec->get(parent, buf);
	if(r != OK){
		printf("symlink: parent directory do not exist\n");
		lc->release(parent);
		return r;
	}

	// check if this file name is already used 
	DirTable parent_dir(buf);
	inum id;
	if(parent_dir.lookup(std::string(name), id)){
		printf("symlink: name already exist\n");
		lc->release(parent);
		return EXIST;
	}


	//create file
	r = ec->create(extent_protocol::T_SYMLINK, ino_out);
	if(r != OK){
		printf("symlink: create file failure\n");
		lc->release(parent);
		return r;
	}

	// write path into file
	r = ec->put(ino_out, std::string(path));
	if(r != OK){
		printf("symlink: write path failure\n");
		lc->release(parent);
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
	
	lc->release(parent);

	if(r != OK){
		printf("symlink: update parent directory failure\n");
		return r;
	}

	return r;
	
}

int 
yfs_client::readlink(inum ino, std::string &path_out)
{
	int r = OK;

	std::string buf;
	lc->acquire(ino);
	r = ec->get(ino, buf);
	lc->release(ino);
	if(r != OK){
		printf("readlink: file do not exist\n");
		return r;
	}

	path_out = buf;
	return r;
	
}


// some new instruction instroduces in lab5

// the log file looks like this:
//	some operations....
//	some operations....
//	some operations....
//  version x

//use 0 for acquiring a lock for lock file
std::string
yfs_client::i2n(yfs_client::inum inum)
{
	std::stringstream st;
	std::string result;
	st << inum;
	st >> result;
	return result;
}

void
yfs_client::version_commit()
{
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	std::string buf;
	buf += "version ";
	buf += i2n(cur_version); 
	cur_version ++;
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);
}

void
yfs_client::prev_version()
{

	if(cur_version == 0){
		printf("no more earlier version");
		return;
	}
	std::ifstream ifs("vclog");
	if(!ifs){
		printf("Open log file error!\n");
		return;
	}

	int goto_version = cur_version - 1;
	int end_of_version = 0;
	int line = 1;
	while(!ifs.eof()){
		std::string oper;
		ifs>>oper;
		if(oper == "version"){
			int version;
			ifs >> version;
			if(version == goto_version){
				end_of_version = line;
				break;
			}
		}
		
		if(oper =="setattr"){
			inum inode;
			ifs>>inode;
			int size;
			ifs>>size;
		}
		if(oper =="write"){
			inum inode;
			size_t size;
			off_t off;
			size_t size_out;
			ifs>>inode;
			ifs>>size;
			ifs>>off;
			char space;
			std::string data;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				data+=space;
			}
		}
		if(oper =="create"){
			inum parent,ino_out;
			size_t size;
			int mode;
			char space;
			std::string name;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
			ifs>>mode;
		}
		if(oper =="unlink"){
			inum parent;
			size_t size;
			std::string name;
			char space;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
		}
		if(oper =="symlink"){
			inum parent,ino_out;
			size_t size;
			std::string link,name;
			char space;
			ifs>>size;
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				link+=space;
			}
			ifs.get(space);
			ifs>>parent;
			ifs>>size;
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
		}
		if(oper =="mkdir"){
			inum parent,ino_out;
			size_t size;
			int mode;
			char space;
			std::string name;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
			ifs>>mode;
		}

		
		line ++;

	}
	ifs.close();
	// no version commit before
	if(end_of_version == 0){
		return;
	}

	printf("\n\nreturn to version %d", cur_version - 1);
	printf("recovring...\n");
	
	lc->acquire(0);
	recovery(end_of_version);
	printf("done");
	cur_version --;
	lc->release(0);

}

void
yfs_client::next_version()
{

	std::ifstream ifs("vclog");
	if(!ifs){
		printf("Open log file error!\n");
		return;
	}
	
	int goto_version = cur_version + 1;
	int end_of_version = 0;
	int line = 1;
	while(!ifs.eof()){
		std::string oper;
		ifs>>oper;
		if(oper == "version"){
			int version;
			ifs >> version;
			if(version == goto_version){
				end_of_version = line;
				break;
			}
		}

		if(oper =="setattr"){
			inum inode;
			ifs>>inode;
			int size;
			ifs>>size;
		}
		if(oper =="write"){
			inum inode;
			size_t size;
			off_t off;
			size_t size_out;
			ifs>>inode;
			ifs>>size;
			ifs>>off;
			char space;
			std::string data;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				data+=space;
			}
		}
		if(oper =="create"){
			inum parent,ino_out;
			size_t size;
			int mode;
			char space;
			std::string name;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
			ifs>>mode;
		}
		if(oper =="unlink"){
			inum parent;
			size_t size;
			std::string name;
			char space;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
		}
		if(oper =="symlink"){
			inum parent,ino_out;
			size_t size;
			std::string link,name;
			char space;
			ifs>>size;
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				link+=space;
			}
			ifs.get(space);
			ifs>>parent;
			ifs>>size;
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
		}
		if(oper =="mkdir"){
			inum parent,ino_out;
			size_t size;
			int mode;
			char space;
			std::string name;
			ifs>>parent;
			ifs>>size;
			ifs.get(space);
			for(size_t i=0;i<size;i++){
				ifs.get(space);
				name+=space;
			}
			ifs>>mode;
		}

		line ++;  // reach a end of a log entry

	}
	ifs.close();
	// no version commit before
	if(end_of_version == 0){
		printf("no more further version\n");
		return;
	}

	printf("\n\nfoward to version %d", cur_version + 1);
	printf("recovring...\n");
	
	lc->acquire(0);
	recovery(end_of_version);
	printf("done\n\n");
	cur_version ++;
	lc->release(0);

}

void
yfs_client::recovery(int end_line)
{
	recovering = 1;
	std::ifstream ifs("vclog");
	if(!ifs){
		printf("Open log file error!\n");
	    recovering = 0;	
		return;
	}
	// clear the file system
	ec->clearfs(0);
	int cur_line = 1;
	while(!ifs.eof())
	{
		// reach the end line, which is "version x"
		if(cur_line == end_line)
			break;
	
		// different log entry
		std::string buf;
		ifs >> buf;
		
		if(buf == "setattr"){
			inum ino;
			ifs >> ino;
			size_t size;
			ifs >> size;
			filestat st;
			st.size = size;

			unsigned long toset;
			setattr(ino, st, toset);
		}

		if(buf == "create"){
			inum parent;
			ifs >> parent;
			int name_len;
			ifs >> name_len;
			char buf;
			std::string name;
			ifs.get(buf);  // the space
			for(int i = 0; i < name_len; i++){
				ifs.get(buf);
				name += buf;
			}
			
			int mode;
			ifs >> mode;
			inum ino_out;
			create(parent, name.c_str(), mode, ino_out); 

		}

		if(buf == "write"){
			inum ino;
			ifs >> ino;
			size_t size;
			ifs >> size;
			off_t off;
			ifs >> off;
			std::string data;
			char buf;
			ifs.get(buf); //space
			for(size_t i = 0; i < size; i++){
				ifs.get(buf);
				data += buf;
			}

			size_t bytes_written;
			write(ino, size, off, data.c_str(), bytes_written);
		}

		if(buf == "unlink"){
			inum parent;
			ifs >> parent;
			int name_len;
			ifs >> name_len;
			char buf;
			std::string name;
			ifs.get(buf);   // space
			for(int i = 0; i < name_len; i++){
				ifs.get(buf);
				name += buf;
			}

			unlink(parent, name.c_str());
		}

		if(buf == "mkdir"){
			inum parent;
			ifs >> parent;
			int name_len;
			ifs >> name_len;
			char buf;
			std::string name;
			ifs.get(buf);   // space
			for(int i = 0; i < name_len; i++){
				ifs.get(buf);
				name += buf;
			}
			
			mode_t mode;
			ifs >> mode;
			inum ino_out;

			mkdir(parent, name.c_str(), mode, ino_out);
		}

		if(buf == "symlink"){
			inum parent;
			ifs >> parent;
			int name_len;
			ifs >> name_len;
			char buf;
			std::string name;
			std::string path;
			ifs.get(buf);   // space
			for(int i = 0; i < name_len; i++){
				ifs.get(buf);
				name += buf;
			}
			int path_len;
			ifs >> path_len;
			ifs.get(buf);   // space
			for(int i = 0; i < path_len; i++){
				ifs.get(buf);
				path += buf;
			}
			inum ino_out;

			symlink(parent, name.c_str(), path.c_str(), ino_out);
		}

		if(buf == "version"){
			int version;
			ifs >> version;
		}

		cur_line ++;  // reach the end of a log entry
		
	}
	recovering = 0;
}

void 
yfs_client::log_setattr(inum ino, size_t size)
{
	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;

	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	std::string buf;
	buf += "setattr ";
	buf += i2n(ino);
	buf += " ";
	buf += i2n(size);
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);
}

void 
yfs_client::log_create(inum parent, const char *name, mode_t mode)
{

	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;
	
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	int name_len = strlen(name);
	std::string name_str = std::string(name);

	std::string buf;
	buf += "create ";
	buf += i2n(parent);
	buf += " ";
	buf += i2n(name_len);
	buf += " ";
	buf += name_str;
	buf += " ";
	buf += i2n(mode);
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);
}

void
yfs_client::log_write(inum ino, size_t size, off_t off, const char *data)
{
	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;
	
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	std::string buf;
	buf += "write ";
	buf += i2n(ino);
	buf += " ";
	buf += i2n(size);
	buf += " ";
	buf += i2n(off);
	buf += " ";
	for(size_t i = 0; i < size; i++){
		buf += data[i];
	}
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);

}

void
yfs_client::log_unlink(inum parent, const char *name)
{
	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;
	
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	int name_len = strlen(name);
	std::string name_str = std::string(name);

	std::string buf;
	buf += "unlink ";
	buf += i2n(parent);
	buf += " ";
	buf += i2n(name_len);
	buf += " ";
	buf += name_str;	
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);

}

void
yfs_client::log_mkdir(inum parent, const char *name, mode_t mode)
{
	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;
	
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	int name_len = strlen(name);
	std::string name_str = std::string(name);

	std::string buf;
	buf += "mkdir ";
	buf += i2n(parent);
	buf += " ";
	buf += i2n(name_len);
	buf += " ";
	buf += name_str;
	buf += " ";
	buf += i2n(mode);
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);

}

void
yfs_client::log_symlink(inum parent, const char *name, const char *path)
{
	//use 0 for acquiring a lock for lock file
	// do not write log when recovering	
	if(recovering)
	  return;
	
	lc->acquire(0);
	std::ofstream ofs("vclog", std::ios::app);  // append
	if(!ofs){
		printf("Open log file error!\n");
		return;
	}

	int name_len = strlen(name);
	int path_len = strlen(path);
	std::string name_str = std::string(name);
	std::string path_str = std::string(path);

	std::string buf;
	buf += "symlink ";
	buf += i2n(parent);
	buf += " ";
	buf += i2n(name_len);
	buf += " ";
	buf += name_str;
	buf += " ";
	buf += i2n(path_len);
	buf += " ";
	buf += path_str;
	buf += "\n";
	ofs << buf;
	ofs.close();
	lc->release(0);

}
