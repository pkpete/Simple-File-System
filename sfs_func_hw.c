//
// Simple FIle System
// Student Name : 고재욱
// Student Number : B511006
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

struct bit_map{
	char arr[SFS_BLOCKSIZE];
};

struct indirect{
	u_int32_t arr[SFS_DBPERIDB];
};

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

int numOfBitmap, numOfBit, numOfBlock;

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
	numOfBitmap = SFS_BITBLOCKS(spb.sp_nblocks); //비트맵이 할당된 inode의 개수
	numOfBit = SFS_BITMAPSIZE(spb.sp_nblocks); //비트맵 내에 있는 비트 수
	numOfBlock = spb.sp_nblocks; //블록 총 개수

}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

int findBitMap(){
	int i, j, k;
	struct bit_map bm;
	for(i = 2; i < 2+numOfBitmap; i++){
		disk_read(&bm, i);
		for(j = 0; j < SFS_BLOCKSIZE; j++){
			if(bm.arr[j] != 255){
				for(k=0; k < 8; k++){
					if(BIT_CHECK(bm.arr[j],k) == 0)
						return ((i-2)*512*8) + j*8 + k;
				}
			}
		}
	}
	return numOfBlock+1;
}

void writeBitMap(int num){
	struct bit_map bm;
	int i, j, k;
	i = 2 + (num / 4096); //블록의 위치
	j = (num % 4096) / 8;	//디렉토리 위치
	k = (num % 4096) % 8;	//비트 위치
	
	disk_read(&bm, i);
	BIT_SET(bm.arr[j], k);
	disk_write(&bm, i);
}

void clearBitMap(int num){
	struct bit_map bm;
	int i, j, k;
	i = 2 + (num / 4096);
	j = (num % 4096) / 8;
	k = (num % 4096) % 8;
	disk_read(&bm, i);
	BIT_CLEAR(bm.arr[j], k);
	disk_write(&bm, i);
}
int error;
void sfs_touch(const char* path)
{
	//skeleton implementation
	int i , j, a, b = 0;
	char *msg = "touch";
	error = 0;
	struct sfs_inode si;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	disk_read(&si, sd_cwd.sfd_ino);			// si에 현재 디렉토리 위치

	// path가 이미 있으면 error
	for(i=0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0) break;
		disk_read(sd, si.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(!strcmp(sd[j].sfd_name, path) && sd[j].sfd_ino != 0){
				error = 1;
				return error_message(msg, path, -6);	
			}
		}
	}

	//디렉토리가 꽉찬경우 error
	int full = 1;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0) { 
			full = 0; 
			a = i;
			break; 
		}
		disk_read(sd, si.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(sd[j].sfd_ino == 0){
				full = 0;
				a = i;
				b = j;
				break;
			}
		}
		if(full == 0) break;
	}
	if(full==1) {error = 1;return error_message(msg, path, -3);}

	disk_read(&si, sd_cwd.sfd_ino);
	//block access
	disk_read(sd, si.sfi_direct[a]);
	

	//for consistency	
	assert( si.sfi_type == SFS_TYPE_DIR );
	
	//allocate new block
	int findblock = findBitMap();
	if(findblock > numOfBlock){error = 1; return error_message(msg, path, -4);} // inode 할당 못 받는 경우

	//write new block
	writeBitMap(findblock);

	if(b == 0){		//새로운 inode 필요
		si.sfi_direct[a] = findblock;

		findblock = findBitMap();	//디렉토리를 위한 block
		if(findblock > numOfBlock){error = 1; return error_message(msg, path, -4);}
		writeBitMap(findblock);
	}
	sd[b].sfd_ino = findblock;		
	strncpy(sd[b].sfd_name, path, SFS_NAMELEN);
	disk_write(sd, si.sfi_direct[a]);
	

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE);
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, findblock);	
}

void sfs_cd(const char* path)
{
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	int i, j, isDir=0, inpath = 0;
	char *msg = "cd";
	if(path == NULL){
		sd_cwd.sfd_ino =  1;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
		isDir = 1;
		inpath = 1;
	}
	else{
		disk_read(&inode, sd_cwd.sfd_ino);
		for(i = 0; i < SFS_NDIRECT; i++){
			if(inode.sfi_direct[i] == 0) break;
			disk_read(dir, inode.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(dir[j].sfd_ino != 0 && !strcmp(path, dir[j].sfd_name)){
					inpath = 1;
					disk_read(&inode, dir[j].sfd_ino);
					if(inode.sfi_type == SFS_TYPE_DIR){
						sd_cwd.sfd_ino = dir[j].sfd_ino;
						strcpy(sd_cwd.sfd_name, path);	
						isDir = 1;
					}
				}
			}
		}
	}
	if(inpath == 0)
		error_message(msg, path, -1);
	else if(isDir == 0)
		error_message(msg, path, -5);
	//printf("Not Implemented\n");
}

void sfs_ls(const char* path)
{
	int i , j, k, l, inpath = 0;
	char *msg = "ls";
	struct sfs_inode inode, temp;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK], tmp[SFS_DENTRYPERBLOCK];
	disk_read(&inode, sd_cwd.sfd_ino);
	if(path==NULL){
		inpath = 1;
		for(i = 0; i < SFS_NDIRECT; i++){
			if(inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(dir_entry[j].sfd_ino != 0){
					disk_read(&temp, dir_entry[j].sfd_ino);
					printf("%s", dir_entry[j].sfd_name);
					if(temp.sfi_type == SFS_TYPE_DIR)
						printf("/");
					printf("\t");
				}
			}
		}
		printf("\n");
	}
	else{
		for(i = 0; i< SFS_NDIRECT; i++){
			if(inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(dir_entry[j].sfd_ino != 0 && !strcmp(path, dir_entry[j].sfd_name)){
					//printf("%s", path);
					inpath = 1;
					disk_read(&temp, dir_entry[j].sfd_ino);
					if(temp.sfi_type == SFS_TYPE_FILE){
						printf("%s", path);
						break;
					}
					for(k = 0; k < SFS_NDIRECT; k++){
						if(temp.sfi_direct[k] == 0) break;
						disk_read(tmp, temp.sfi_direct[k]);
						for(l = 0; l < SFS_DENTRYPERBLOCK; l++){
							if(tmp[l].sfd_ino != 0){
								disk_read(&temp, tmp[l].sfd_ino);
								printf("%s", tmp[l].sfd_name);
								if(temp.sfi_type == SFS_TYPE_DIR)
									printf("/");
								printf("\t");
							}
						}

					}
					break;
				}
			}
			if(inpath) { printf("\n"); break;}
		}
	}
	if(inpath != 1){
		error_message(msg, path, -1);
	}
	//printf("Not Implemented\n");
}

void sfs_mkdir(const char* org_path) 
{
	int i, j, a, b = 0;
	char *msg = "mkdir";
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	disk_read(&inode, sd_cwd.sfd_ino);
	// path가 이미 있으면
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0) break;
		disk_read(dir, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(!strcmp(dir[j].sfd_name, org_path) && dir[j].sfd_ino != 0)
				return error_message(msg, org_path, -6);
		}
	}
	//디렉토리가 꽉찬경우 error
	int full = 1;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0) { 
			full = 0; 
			a = i;
			break; 
		}
		disk_read(dir, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(dir[j].sfd_ino == 0){
				full = 0;
				a = i;
				b = j;
				break;
			}
		}
		if(full == 0) break;
	}
	if(full==1) {return error_message(msg, org_path, -3);}
	
	disk_read(&inode, sd_cwd.sfd_ino);
	disk_read(dir, inode.sfi_direct[a]);

	int findblock = findBitMap();
	if(findblock > numOfBlock) return error_message(msg, org_path, -4);
	writeBitMap(findblock);

	if(b==0){
		inode.sfi_direct[a] = findblock;
		findblock = findBitMap();
		if(findblock > numOfBlock) return error_message(msg, org_path, -4);
		writeBitMap(findblock);
	}
	dir[b].sfd_ino = findblock;
	strncpy(dir[b].sfd_name, org_path, SFS_NAMELEN);
	disk_write(dir, inode.sfi_direct[a]);
	inode.sfi_size += sizeof(struct sfs_dir);
	disk_write(&inode, sd_cwd.sfd_ino);
	
	//디렉토리의 inode
	struct sfs_inode newbie;
	bzero(&newbie, SFS_BLOCKSIZE);
	newbie.sfi_size = sizeof(struct sfs_dir)*2;
	newbie.sfi_type = SFS_TYPE_DIR;
	findblock = findBitMap();
	if(findblock > numOfBlock) return error_message(msg, org_path, -4);
	writeBitMap(findblock);
	newbie.sfi_direct[0] = findblock;
	disk_write(&newbie, dir[b].sfd_ino);

	//inode의 디렉토리 . // .. 추가하기
	struct sfs_dir newdir[SFS_DENTRYPERBLOCK];
	bzero(newdir, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
	newdir[0].sfd_ino = dir[b].sfd_ino; //나
	strncpy(newdir[0].sfd_name, ".", SFS_NAMELEN);
	newdir[1].sfd_ino = sd_cwd.sfd_ino;	//부모노드
	strncpy(newdir[1].sfd_name, "..", SFS_NAMELEN);
	disk_write(newdir, findblock);

	//printf("Not Implemented\n");
}

void sfs_rmdir(const char* org_path) 
{
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	char *msg = "rmdir";
	disk_read(&inode, sd_cwd.sfd_ino);
	if(!strcmp(org_path, "..")) return error_message(msg, org_path, -7);
	if(!strcmp(org_path, ".")) return error_message(msg, org_path, -8);
	//path가 없는경우-1
	int inpath=0, i, j, a, b;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] != 0) {
			disk_read(dir, inode.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(!strcmp(dir[j].sfd_name, org_path) && dir[j].sfd_ino != 0){
					inpath = 1;
					a = i;
					b = j;
					break;
				}
			}
			if(inpath == 1) break;
		}
	}
	if(inpath == 0) return error_message(msg, org_path, -1);

	//디렉토리가 아닌경우 -2
	disk_read(&inode, dir[b].sfd_ino); 	//제거해야하는 디렉토리 inode
	if(inode.sfi_type == SFS_TYPE_FILE) return error_message(msg, org_path, -2);
	
	//디렉토리가 empty가 아닌경우 -7
	if(inode.sfi_size != 2 * sizeof(struct sfs_dir)) return error_message(msg, org_path, -7);

	
	//제거해야되는 inode의 디렉토리 제거-> 할필요 없네,,, 디렉토리가 비어져있어야 rmdir할수있으니까
	int k, l;
/*	for(k=0; k < SFS_NDIRECT;k++){
		if(inode.sfi_direct[k] != 0){
			disk_read(dir, inode.sfi_direct[k]);
			for(l = 0; l < SFS_DENTRYPERBLOCK; l++){
				clearBitMap(dir[l].sfd_ino);
			}
//			bzero(dir, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
			disk_write(dir, inode.sfi_direct[k]);
		}
	}*/

	//제거해야되는 디렉토리의 inode 제거
	for(k = 0; k < SFS_NDIRECT; k++)
		if (inode.sfi_direct[k] != 0){
			clearBitMap(inode.sfi_direct[k]);
//			inode.sfi_direct[k] = SFS_NOINO;	 -> 이거 하면 안됨.... ㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋ
		}
//	inode.sfi_size = 0;
//	inode.sfi_type = SFS_TYPE_INVAL;
//	bzero(&inode, SFS_BLOCKSIZE);
	disk_write(&inode, dir[b].sfd_ino);

	//현재 디렉토리에 있는 제거해야되는 디렉토리 제거
	disk_read(&inode, sd_cwd.sfd_ino);
	inode.sfi_size -= sizeof(struct sfs_dir);
	disk_write(&inode, sd_cwd.sfd_ino);

	disk_read(dir, inode.sfi_direct[a]);
	clearBitMap(dir[b].sfd_ino);
	dir[b].sfd_ino = SFS_NOINO;
	disk_write(dir, inode.sfi_direct[a]);
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	disk_read(&inode, sd_cwd.sfd_ino);	// 현재 디렉토리의 inode
	int i, j;
	char *msg = "mv";
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0) break;	//inode가 point하고있는 블록이 비어 있지 않다면 디렉토리로 읽어라
		disk_read(dir, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			//dst_name이 이미 있다면 error message
			if(!strcmp(dir[j].sfd_name, dst_name) && dir[j].sfd_ino != 0)
				return error_message(msg, dst_name, -6);
		}
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			//src_name을 찾으면
			if(!strcmp(dir[j].sfd_name, src_name) && dir[j].sfd_ino !=0){
				strcpy(dir[j].sfd_name, dst_name);
				disk_write(dir, inode.sfi_direct[i]);
				return;
			}
		}
	}
	error_message(msg,src_name,-1);
}

void sfs_rm(const char* path) 
{
	char *msg = "rm";
	struct sfs_inode  inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	disk_read(&inode, sd_cwd.sfd_ino);
	//path가 없는 경우 -1
	int i, j, inpath = 0, a=0, b=0;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0) break;
		disk_read(dir, inode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(!strcmp(dir[j].sfd_name, path) && dir[j].sfd_ino != 0){
				inpath = 1;
				a=i;
				b=j;
				break;
			}
		}
		if(inpath==1)break;
	}
	if(inpath == 0) return error_message(msg, path, -1);
	//file이 아닌경우-10
	disk_read(&inode, dir[b].sfd_ino);	//제거해야되는 파일
	if(inode.sfi_type == SFS_TYPE_DIR) return error_message(msg, path, -9);
	
	//direct file들 제거
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] != 0){
			clearBitMap(inode.sfi_direct[i]);
		}
	}
	//indirect
	struct sfs_inode inode2;
	struct indirect block;
	if(inode.sfi_indirect != 0){
		clearBitMap(inode.sfi_indirect);
		printf("%d\n", inode.sfi_indirect);
		disk_read(&block, inode.sfi_indirect);
		for(i = 0; i < SFS_DBPERIDB; i++){
			if(block.arr[i] != 0){
				clearBitMap(block.arr[i]);
			}
		}
		disk_write(&block, inode.sfi_indirect);
	}
	disk_read(&inode, sd_cwd.sfd_ino);
	disk_read(dir, inode.sfi_direct[a]);
	clearBitMap(dir[b].sfd_ino);
	dir[b].sfd_ino = SFS_NOINO;
	inode.sfi_size -= sizeof(struct sfs_dir);
	disk_write(dir, inode.sfi_direct[a]);
	disk_write(&inode, sd_cwd.sfd_ino);
	
	
	//printf("Not Implemented\n");
}

/*
void file_read(void *data, u_int32_t block){
	char *cdata = data;
	u_int32_t tot = 0;
	int len;
	
	assert(file >= 0);
	int file_size;
	file_size = lseek(file, block*SFS_BLOCKSIZE, SEEK_END);	// 파일의 마지막을 기준으로 offset 계산 -> 파일의 크기
	if(file_size > 143 * 512) return err(1, "file too big");
	if(lseek(file, block*SFS_BLOCKSIZE, SEEK_SET) < 0) err(1,"lseek");
	while(tot < SFS_BLOCKSIZE){
		len = read(file, cdata + tot, SFS_BLOCKSIZE - tot);
		if(len<0){
			if(errno == EINTR || errno == EAGAIN){
				continue;
			}
			err(1,"read");
		}
		if(len==0) err(1,"unexpected EOF in mid-secotr");
		tot += len;
	}
	printf("tot =%d\n", tot);
}	*/

void sfs_cpin(const char* local_path, const char* path) 
{
	int i, j, a, b, inpath = 0;
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	bzero(&inode, SFS_BLOCKSIZE);
	int file = -1;
	assert(file < 0);
	file = open(path, O_RDWR);
	if(file < 0){
		printf("cpin: can't open %s input file\n", path);
		return;
	}
	assert(file >= 0);
	int file_size = lseek(file, 0, SEEK_END);
	if(file_size > 143*512){
		printf("cpin: input file size exceeds the max file size\n", path);
		return;
	}
	if(lseek(file, 0, SEEK_SET) < 0) err(1,"lseek");
	
//	sfs_touch(local_path);	//local_path 만들어줬음
//	if(error == 1) return;
	disk_read(&inode, sd_cwd.sfd_ino);
	for(i=0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0) break;
		disk_read(dir, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(!strcmp(dir[j].sfd_name, local_path) && dir[j].sfd_ino != 0){
				return error_message("cpin", local_path, -6);
			}
		}
	}
	int full = 1;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] == 0){
			full = 0;
			a = i;
			break;
		}
		disk_read(dir, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++){
			if(dir[j].sfd_ino == 0){
				full = 0;
				a = i;
				b = j;
				break;
			}
		}
		if(full == 0) break;
	}
	if(full==1) return error_message("cpin", local_path, -3);
	disk_read(&inode, sd_cwd.sfd_ino);
	disk_read(dir, inode.sfi_direct[a]);
	assert( inode.sfi_type == SFS_TYPE_DIR );
	int findblock = findBitMap();
	if(findblock > numOfBlock) return error_message("cpin", local_path, -4);
	writeBitMap(findblock);

	if(b==0){
		inode.sfi_direct[a] = findblock;
		findblock = findBitMap();
		if(findblock > numOfBlock){ return error_message("cpin", local_path, -4);}
		writeBitMap(findblock);
	}
	dir[b].sfd_ino = findblock;
	strncpy(dir[b].sfd_name, local_path, SFS_NAMELEN);
	disk_write(dir, inode.sfi_direct[a]);

	inode.sfi_size += sizeof(struct sfs_dir);
	disk_write(&inode, sd_cwd.sfd_ino);

	struct sfs_inode newbie;
	bzero(&newbie, SFS_BLOCKSIZE);
	newbie.sfi_size = 0;
	newbie.sfi_indirect = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, findblock);
///////////////////////////////////////////////
//	disk_read(&inode, dir[b].sfd_ino);	//만들어진 파일의 inode
//	inode.sfi_size = file_size;
//	disk_write(&inode, dir[b].sfd_ino);
	struct indirect indirect_block;
	for(i = 0; i < SFS_DBPERIDB; i++){
		indirect_block.arr[i] = 0;
	}
	int len;
//	int tot = 0;
	struct bit_map bm;
	//file은 direct 15개 indirect 128개 총 143개
	j = 0;
	i = 0;
	for(i=0;i<143;i++){
		if((len = read(file, bm.arr, SFS_BLOCKSIZE)) > 0){
		//	disk_write(&newbie, dir[b].sfd_ino);
			findblock = findBitMap();
			if(findblock > numOfBlock) return error_message("cpin", local_path, -4);
			if(i < 15){//direct읽어
				writeBitMap(findblock);
				newbie.sfi_direct[i] = findblock;
				disk_write(&newbie, dir[b].sfd_ino);
			}
			else if(i == 15){//indirect
				writeBitMap(findblock);
				newbie.sfi_indirect = findblock;

				findblock = findBitMap();
				if(findblock > numOfBlock) return error_message("cpin", local_path, -4);
				writeBitMap(findblock);
				indirect_block.arr[j++] = findblock;
				disk_write(&indirect_block, newbie.sfi_indirect);
				disk_write(&newbie, dir[b].sfd_ino);
			}
			else{
				writeBitMap(findblock);
				indirect_block.arr[j++] = findblock;
				disk_write(&indirect_block, newbie.sfi_indirect);
			}
			newbie.sfi_size += len;	
			disk_write(&newbie,dir[b].sfd_ino);
		}
		else break;
	}
//	close(file);
//	file = -1;
}

void sfs_cpout(const char* local_path, const char* path) 
{
	struct sfs_inode inode;
	struct sfs_dir dir[SFS_DENTRYPERBLOCK];
	int i, j, inpath = 0, a, b;
	char *msg = "cpout";
	disk_read(&inode, sd_cwd.sfd_ino);
	//local_path가 없을때
	for(i=0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] != 0) {
			disk_read(dir, inode.sfi_direct[i]);
			for(j=0; j < SFS_DENTRYPERBLOCK; j++){
				if(!strcmp(dir[j].sfd_name, local_path) && dir[j].sfd_ino != 0){
					inpath = 1;
					a = i;
					b = j;
					break;
				}
			}
		}if(inpath == 1) break;
	}
	if(inpath == 0) return error_message(msg, local_path, -1);
	int file = -1;
	assert(file < 0);
	file = open(path, O_RDWR | O_EXCL | O_CREAT, S_IRWXU);
	if(file <= 0) return error_message(msg, path, -6);
	if(lseek(file, 0, SEEK_SET)<0) err(1,"lseek");

	disk_read(&inode, dir[b].sfd_ino);	// local_path
	int file_size = inode.sfi_size;
	struct bit_map bm;
	int len = 0;
	for(i = 0; i < SFS_NDIRECT; i++){
		if(inode.sfi_direct[i] != 0){
			disk_read(bm.arr, inode.sfi_direct[i]);
			if(SFS_BLOCKSIZE > file_size - len){
				len += write(file, bm.arr, file_size - len);
			}
			else{
				len += write(file, bm.arr, SFS_BLOCKSIZE);
			}
		}
	}
	struct indirect indirect_block;
	for(i = 0; i < SFS_DBPERIDB; i++) indirect_block.arr[i] = 0;
	if(inode.sfi_indirect != 0){
		disk_read(&indirect_block, inode.sfi_indirect);
		for(i=0; i < SFS_DBPERIDB; i++){
			if(indirect_block.arr[i] != 0)
				disk_read(bm.arr, indirect_block.arr[i]);
				if(SFS_BLOCKSIZE > file_size - len){
					len += write(file, bm.arr, file_size - len);
				}
				else
					len += write(file, bm.arr, SFS_BLOCKSIZE);
		}
	}

	close(file);
	file = -1;
//	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);	//현재 디렉토리의 inode
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name); //현재 디렉토리의 inode번호 & 이름	
	dump_inode(c_inode);
	printf("\n");

}

