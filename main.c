#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "scanNTFS.h"
#include "system_call_search.h"

static void showHelp(void)
{
	printf("-h: show this massege\n");
	printf("-b: show all files in partition by block device, eg: -b /dev/sda4\n");
	printf("-m: show all files in partition by mount directory, eg: -m /tmp/mnt/XXXXX\n");
	printf("-s: search files in specific directory by using NTFS lib,\n\
    before use, need to create search_key.txt to specific search keys,\n\
    and results will be saved in search log\n\
    eg: -s /tmp/mnt/XXXXX path_need_to_be_searched\n");
	printf("-r: search files in specific directory by using system call,\n\
	before use, need to create search_key.txt to specific search keys,\n\
    and results will be saved in search log\n\
    eg: -r /tmp/mnt/XXXXX/path_need_to_be_searched\n");
	return;
}

//从search_key.txt获得搜索关键字
static int allocSearchkey(char *keyList[32], int *i)
{
	int ret = 0;
	FILE *fp = NULL;
	char buffer[1024] = {0};
	fp = fopen("./search_key.txt", "r");
	*i = 0;
	if(fp == NULL)
	{
		ret = -1;
		fprintf(stderr, "can't open ./search_key.txt in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		goto EXIT;
	}

	if(fgets(buffer, 1024, fp) == NULL)
	{
		ret = -1;
		fprintf(stderr, "can't read ./search_key.txt in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		goto EXIT;
	}	

	printf("buffer = %s\n", buffer);

	char *temp = buffer;
	while(*i < 32)
	{
		for(; *temp != '\0' && *temp == ' '; temp++);
		if(*temp == '\0')
			break;
		else
		{
			char *temp2 = temp;
			for(; *temp2 != '\0' && *temp2 != ' '; temp2++);
			keyList[*i] = (char *)calloc(1, temp2 - temp + 1);
			memcpy(keyList[*i], temp, temp2-temp);
			printf("keyList[%d] = %s\n", *i, keyList[*i]);
			temp = temp2;
		}

		(*i)++;
	}
	
EXIT:
	if(fp)
		fclose(fp);
	return ret;
}

static int freeSearchKey(char *keyList[32])
{
	int i = 0;
	for(; i < 32; i++)
	{
		if(keyList[i] != NULL)
		{
			free(keyList[i]);
			keyList[i] = NULL;
		}
		else
			break;
	}

	return 0;
}

unsigned long long n = 0;
int NTFS_getFileList_callback(FILE_INFORMATION fileInformation, void *self)
{
	FILE *fp = NULL;
	fp = fopen("./search_log", "a+");
	fprintf(fp, "n = %llu\n", ++n);
	fprintf(fp, "MftNum = %lld\n", fileInformation.MftNum);
	fprintf(fp, "fatherMftNum = %lld\n", fileInformation.fatherMftNum);
	fprintf(fp, "fileFlag = %llx\n", fileInformation.fileFlag);
	fprintf(fp, "createTime = %lld\n", fileInformation.createTime);
	fprintf(fp, "alterTime = %lld\n", fileInformation.alterTime);
	fprintf(fp, "readTime = %lld\n", fileInformation.readTime);
	fprintf(fp, "allocSize = %lld\n", fileInformation.allocSize);
	fprintf(fp, "validSize = %lld\n", fileInformation.validSize);
	fprintf(fp, "fileName = %s\n", fileInformation.fileName);
	fprintf(fp, "pFullFileName = %s\n", fileInformation.pFullFileName);
	fprintf(fp, "-----------------------------------\n");
	fclose(fp);
	
	return 0;
}



int NTFS_getUSNjournalInformation_callback(USN_INFORMATION USNInformation, void *self)
{
	printf("MFTnum = %lld\n", USNInformation.MFTnum);
	printf("FatherMFTnum = %lld\n", USNInformation.FatherMFTnum);
	printf("TimeStamp = %lld\n", USNInformation.TimeStamp);
	printf("Reason = %x\n", USNInformation.Reason);
	printf("FileAttributes = %x\n", USNInformation.FileAttributes);
	printf("fileName = %s\n", USNInformation.fileName);
	printf("pFullFileName = %s\n", USNInformation.pFullFileName);
	printf("-----------------------------------\n");
	
	return 0;
}





int main(int argc, char *argv[])
{
	int opt = 0;
	
	unsigned long long sec = 0, us = 0;
	struct timeval oldTimeVal, newTimeVal;
	if( gettimeofday(&oldTimeVal, NULL) < 0 )
	{
		fprintf(stderr, "parameters in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		return -1;
	}
	
	//清空内核缓存
	system("echo 3 > /proc/sys/vm/drop_caches");
	
	while( ( opt = getopt(argc, argv, "r:b:s:u:m:h") ) != -1 )
	{
		switch(opt)
		{
			//帮助
			case 'h':
			{
				showHelp();
				break;
			}
			
			//通过块设备文件显示所有文件
			case 'b':
			{
				int ret = 0;
				int *self = NULL;
				
				if(optarg == NULL)
				{
					fprintf(stderr, "parameters in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				ret = NTFS_getFileListByBlkDev(optarg, NTFS_getFileList_callback, self);
				if(ret < 0)
				{
					fprintf(stderr, "NTFS_getFileListByBlkDev in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				break;
			}	
			
			//通过挂载路径显示所有文件
			case 'm':
			{
				int ret = 0;
				int *self = NULL;
				
				if(optarg == NULL)
				{
					fprintf(stderr, "parameters in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				ret = NTFS_getFileListByMountDir(optarg, NTFS_getFileList_callback, self);
				if(ret < 0)
				{
					fprintf(stderr, "NTFS_getFileListByBlkDev in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				break;
			}	
			
			//搜索(NTFS优化)
			case 's':
			{
				int ret = 0;
				char *pSearchKey[32] = {NULL};
				int searchKeyCnt = 0;
				int *self = NULL;
				
				//获得搜索关键字
				ret = allocSearchkey(pSearchKey, &searchKeyCnt);
				if(ret < 0)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "allocSearchkey in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				if(optarg == NULL || argv[optind] == NULL)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "parameters in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				ret = NTFS_searchKeys(optarg, argv[optind], pSearchKey, searchKeyCnt, NTFS_getFileList_callback, self);
				if(ret < 0)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "NTFS_searchKeys in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				freeSearchKey(pSearchKey);
				
				
				break;
			}
			
			//搜索(系统调用)
			case 'r':
			{
				int ret = 0;
				char *pSearchKey[32] = {NULL};
				int searchKeyCnt = 0;
				int *self = NULL;
				
				//获得搜索关键字
				ret = allocSearchkey(pSearchKey, &searchKeyCnt);
				if(ret < 0)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "allocSearchkey in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				if(optarg == NULL)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "parameters in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				ret = systemCallSearch(optarg, pSearchKey, searchKeyCnt, 0, NTFS_getFileList_callback, self);
				if(ret < 0)
				{
					freeSearchKey(pSearchKey);
					fprintf(stderr, "systemCallSearch in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				freeSearchKey(pSearchKey);
				
				break;
			}
			
			//显示USN信息
			case 'u':
			{
				int ret = 0;
				int *self = NULL;
				
				if(optarg == NULL)
				{
					fprintf(stderr, "parameters in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				ret = NTFS_getUSNjournalInformation(optarg, NTFS_getUSNjournalInformation_callback, self);
				if(ret < 0)
				{
					fprintf(stderr, "NTFS_getUSNjournalInformation in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
					return -1;
				}
				
				break;
			}
				
			default:
			{
				showHelp();
				break;
			}
		}
	}
	
	if( gettimeofday(&newTimeVal, NULL) < 0 )
	{
		fprintf(stderr, "parameters in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		return -1;
	}
	
	us = ((unsigned long long)newTimeVal.tv_sec * 1000000 + newTimeVal.tv_usec) 
		- ((unsigned long long)oldTimeVal.tv_sec * 1000000 + oldTimeVal.tv_usec);
	sec = us / 1000000;
	us %= 1000000;
	printf("run time : sec = %llu, us = %llu\n", sec, us);
	
	return 0; 
}



 






