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

#include <fstream>

//yfs_client::yfs_client()
//{
//    ec = new extent_client("");

//}

yfs_client::yfs_client(std::string extent_dst)
{
    ec = new extent_client(extent_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

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
	if(ec->getattr(inum, a) != extent_protocol::OK){
		printf("error getting attr\n");
		return false;
	}

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
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

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

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
	std::string buf;
	r = ec->get(ino, buf);
	if(r != OK){
		printf("setattr: file do not exist\n");
		return r;
	}

	// adjust size
	buf.resize(size);
	// write back
	r = ec->put(ino, buf);

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

	std::string buf;
	r = ec->get(parent, buf);   // read parent directory block content

	if(r != OK){
		printf("create: parent directory do not exist\n");
		return r;
	}

	// check if this filename already exist
	inum id;
	DirTable parent_dir(buf);
	if(parent_dir.lookup(std::string(name), id)){
		printf("create: name already exist\n");
		return EXIST;
	}
	
	// create file
	r = ec->create(extent_protocol::T_FILE, ino_out);
	if(r != OK){
		printf("create: create file failure\n");
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
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
	r = ec->get(parent, buf);   // read parent directory block content

	if(r != OK){
		printf("mkdir: parent directory do not exist\n");
		return r;
	}

	// check if this filename already exist
	inum id;
	DirTable parent_dir(buf);
	if(parent_dir.lookup(std::string(name), id)){
		printf("mkdir: name already exist\n");
		return EXIST;
	}
	
	// create file
	r = ec->create(extent_protocol::T_DIR, ino_out);
	if(r != OK){
		printf("mkdir: create file failure\n");
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
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
	r = ec->get(parent, buf);
	if(r != OK){
		printf("lookup: parent directory do not exist\n");
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
	r = ec->get(dir, buf);
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
	r = ec->get(ino, buf);
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
	r = ec->get(ino, buf);
	if(r != OK){
		printf("write: file do not exist\n");
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
	r = ec->get(parent, buf);
	if(r != OK){
		printf("unlink: parent directory do not exist\n");
		return r;
	}

	inum file_id;
	DirTable parent_dir(buf);
	if(!parent_dir.lookup((std::string)name, file_id)){
		printf("unlink: file do not exist\n");
		return NOENT;
	}

	// remove file
	r = ec->remove(file_id);
	if(r != OK){
		printf("unlink: remove failure\n");
		return r;
	}

	// update directory
	parent_dir.erase(std::string(name));
	r = ec->put(parent, parent_dir.dump2str());
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
	
	//std::fstream ofs("logsymlk.me", std::ios::out);

	std::string buf;
	r = ec->get(parent, buf);
	if(r != OK){
		printf("symlink: parent directory do not exist\n");
		return r;
	}
	//ofs<<"parent:\t"<<parent<<"\n";
	//ofs<<"name:\t"<<std::string(name)<<"\n";
	//ofs<<"path:\t"<<std::string(path)<<"\n";

	// check if this file name is already used 
	DirTable parent_dir(buf);
	inum id;
	if(parent_dir.lookup(std::string(name), id)){
		printf("symlink: name already exist\n");
		return EXIST;
	}


	//create file
	r = ec->create(extent_protocol::T_SYMLINK, ino_out);
	if(r != OK){
		printf("symlink: create file failure\n");
		return r;
	}
	//ofs<<"inode out:"<<ino_out<<"\n";

	// write path into file
	r = ec->put(ino_out, std::string(path));
	if(r != OK){
		printf("symlink: write path failure\n");
		return r;
	}

	// update parent directory
	parent_dir.insert(std::string(name), ino_out);
	r = ec->put(parent, parent_dir.dump2str());
	if(r != OK){
		printf("symlink: update parent directory failure\n");
		return r;
	}
	//ofs<<"finish: r:"<<r<<"\n";

	//ofs<<"inode out:"<<ino_out<<"\n";
	return r;
	
}

int 
yfs_client::readlink(inum ino, std::string &path_out)
{
	int r = OK;

	std::string buf;
	r = ec->get(ino, buf);
	if(r != OK){
		printf("readlink: file do not exist\n");
		return r;
	}

	path_out = buf;
	return r;
	
}

