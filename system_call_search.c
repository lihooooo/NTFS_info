#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "scanNTFS.h"
#include "system_call_search.h"

//匹配返回0，不匹配返回1，错误返回-1
int search_file_by_key(char *pFilename, char *key[32])
{
	int res = 1;
	char *pFind = NULL;
	char **ppKey = key;
	
	if(ppKey == NULL)
		return 1;
	
	while(*ppKey != NULL)
	{
		
		pFind = strstr(pFilename, *ppKey);
		if(pFind != NULL)
		{
			res = 0;
			break;
		}
		
		ppKey++;
	}
	
EXIT:
	return res;
}



int systemCallSearch(const char *pDirPath, char **search_key_list, int search_key_count, int currentOnly, 
		int (*callback)(FILE_INFORMATION fileInformation, void *self), void *self)
{
	char *pFilePath = NULL;
	int res = 0;
	DIR *pDir = NULL;
	struct dirent *pEntry = (struct dirent *)calloc(1, sizeof(struct dirent));
	struct dirent *pResult = NULL;
	char *pPath = NULL;
	
	FILE_INFORMATION fileInformation;
	memset(&fileInformation, 0, sizeof(FILE_INFORMATION));
	
	pPath = (char *)calloc(1, strlen(pDirPath) + 1);
	if(pPath == NULL)
	{
		printf("calloc error:%s\n", strerror(errno));
		goto EXIT;
	}
	else
	{
		snprintf(pPath, strlen(pDirPath) + 1, "%s", pDirPath);
	}
	
	if(pPath == NULL)
	{
		res = -1;
		goto EXIT;
	}
	else
	{
		char *p = pPath + strlen(pPath) - 1;
		while(*p == '/')
		{
			*p = 0;
			p--;
		}
	}
	
	pDir = opendir(pPath);
	if(pDir == NULL)
	{
		printf("opendir error:%s\n", strerror(errno));
		goto EXIT;
	}
	
	while(1)
	{
		res = readdir_r(pDir, pEntry, &pResult);
		if(res == 0)
		{
			if(pResult == NULL)
			{
				break;
			}
			
			//跳过.和..
			if(*(pEntry->d_name) == '.')
				continue;
			
			//打印文件名
			pFilePath = (char *)calloc(1, strlen(pPath) + strlen(pEntry->d_name) + 2);
			if(pFilePath == NULL)
			{
				printf("calloc error:%s\n", strerror(errno));
				goto EXIT;
			}
			snprintf(pFilePath, strlen(pPath) + strlen(pEntry->d_name) + 2, "%s/%s", pPath, pEntry->d_name);
			
			//printf("file name = %s\n", pFilePath);
			res = search_file_by_key(pEntry->d_name, search_key_list);
			if(res == 0)
			{
				//printf("filename = %s\n", pFilePath);
				struct stat st;
				res = stat(pFilePath, &st);
				if(res < 0)
				{
					printf("stat error:%s\n", strerror(errno));
					goto EXIT;
				}
				
				memset(&fileInformation, 0, sizeof(fileInformation));
				
				fileInformation.fileFlag = S_ISDIR(st.st_mode);
				
				fileInformation.createTime = st.st_ctime;
				fileInformation.alterTime = st.st_mtime;
				fileInformation.readTime = st.st_atime;
				
				fileInformation.allocSize = st.st_blksize;
				fileInformation.validSize = st.st_size;
				
				strcat(fileInformation.fileName, pEntry->d_name);
				fileInformation.pFullFileName = pFilePath;
				
				if(callback)
					callback(fileInformation, self);
				
			}
			else if(res < 0)
			{
				printf("search_file_by_key error\n");
				goto EXIT;
			}
			
			
			if(pEntry->d_type == DT_DIR && currentOnly == 0)
			{
				systemCallSearch(pFilePath, search_key_list, search_key_count, currentOnly, callback, self);
			}
			
			if(pFilePath)
			{
				free(pFilePath);
				pFilePath = NULL;
			}
		}
		else
		{
			printf("readdir error:%s\n", strerror(errno));
			goto EXIT;
		}
	}
	
	
EXIT:
	if(pDir)
	{
		closedir(pDir);
		pDir = NULL;
	}
	
	if(pEntry)
	{
		free(pEntry);
		pEntry = NULL;
	}
	
	if(pFilePath)
	{
		free(pFilePath);
		pFilePath = NULL;
	}
	
	if(pPath)
	{
		free(pPath);
		pPath = NULL;
	}

	return res;
}




























