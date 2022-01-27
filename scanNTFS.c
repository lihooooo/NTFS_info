#define __USE_FILE_OFFSET64
#define __USE_LARGEFILE64
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <mntent.h>
#include <errno.h>
#include "scanNTFS.h"

/*---------------------------------文件结构-------------------------------------*/
typedef unsigned char 		u8;
typedef unsigned short 		u16;
typedef unsigned int 		u32;
typedef unsigned long long 	u64;

/*------------------------------------NTFS DBR扇区结构体---------------------------*/
typedef struct _NTFS_DBR 
{
    u8 JMP[3]; 					//跳转指令
    u8 FsID[8]; 				//文件系统ID
    u16 bytePerSector;  		//每扇区字节数
    u8 secPerCluster;       	//每簇扇区数
    u8 reservedBytes[2];    	//2个保留字节
    u8 zeroBytes[3];    		//三个0字节
    u8 unusedBytes1[2];			//2个未用字节
    u8 mediaType;				//媒体类型
    u8 unusedBytes2[2]; 		//2个未用字节
    u16 secPerTrack;    		//每磁道扇区数
    u16 Heads; 					//磁头数
    u32 hideSectors; 			//隐藏扇区数
    u8 unusedBytes3[4]; 		//4个未用字节
    u8 usedBytes[4];    		//4个固定字节
    u64 totalSectors; 			//总扇区数
    u64 MFT; 					//MFT文件起始簇号
    u64 MFTMirror;    			//MFTMirror文件起始簇号
    u8 unusedBytes4[4]; 		//3个未用字节
    u8 unusedBytes5[4]; 		//未用字节
    u8 volumeSerialID64[8]; 	//卷序列号
    u64 checkSum;    			//校验和
    u8 bootCode[422];   		//引导代码
    u8 endSignature[2]; 		//结束标志
}__attribute__((packed)) NTFS_DBR;

/*------------------------------------MFT表项的结构--------------------------------*/
//MFT头
typedef struct _MFT_HEADER
{
	u8 Type[4];       			// 固定值'FILE'
	u16 USNOffset;    	  		// 更新序列号偏移, 与操作系统有关
	u16 USNCount;      	 		// 固定列表大小Size in words of Update Sequence Number & Array (S)
	u64 Lsn;               		// 日志文件序列号(LSN)
	u16 SequenceNumber;   		// 序列号(用于记录文件被反复使用的次数)
	u16 LinkCount;        		// 硬连接数
	u16 AttributeOffset;  		// 第一个属性偏移
	u16 Flags;            		// flags, 00表示删除文件,01表示正常文件,02表示删除目录,03表示正常目录
	u32 BytesInUse;       		// 文件记录实时大小(字节) 当前MFT表项长度,到FFFFFF的长度+4
	u32 BytesAllocated;   		// 文件记录分配大小(字节)
	u64 BaseFileRecord;   		// = 0 基础文件记录 File reference to the base FILE record
	u16 NextAttributeNumber;	// 下一个自由ID号
	u16 Pading;           		// 边界
	u32 MFTRecordNumber;  		// windows xp中使用,本MFT记录号
	u16 USN;     		 		// 更新序列号
	u8 UpdateArray[0];      	// 更新数组
}__attribute__((packed)) MFT_HEADER;

//常驻属性和非常驻属性的公用部分
typedef struct _MFT_CommonAttributeHeader 
{
	u32 ATTR_Type; 		//属性类型
	u32 ATTR_Size; 		//属性头和属性体的总长度
	u8 ATTR_ResFlag; 	//是否是常驻属性（0常驻 1非常驻）
	u8 ATTR_NamSz; 		//属性名的长度
	u16 ATTR_NamOff; 	//属性名的偏移 相对于属性头
	u16 ATTR_Flags; 	//标志（0x0001压缩 0x4000加密 0x8000稀疏）
	u16 ATTR_Id; 		//属性唯一ID
}__attribute__((packed)) MFT_CommonAttributeHeader;

//常驻属性 属性头
typedef struct _MFT_ResidentAttributeHeader 
{
	MFT_CommonAttributeHeader ATTR_Common;
	u32 ATTR_DatSz; 		//属性数据的长度
	u16 ATTR_DatOff; 		//属性数据相对于属性头的偏移
	u8 ATTR_Indx; 			//索引
	u8 ATTR_Resvd; 			//保留
	u8 ATTR_AttrNam[0];		//属性名，Unicode，结尾无0
}__attribute__((packed)) MFT_ResidentAttributeHeader;

//非常驻属性 属性头
typedef struct _MFT_NonResidentAttributeHeader 
{
	MFT_CommonAttributeHeader ATTR_Common;
	u64 ATTR_StartVCN; 		//本属性中数据流起始虚拟簇号 
	u64 ATTR_EndVCN; 		//本属性中数据流终止虚拟簇号
	u16 ATTR_DatOff; 		//簇流列表相对于属性头的偏移
	u16 ATTR_CmpSz; 		//压缩单位 2的N次方
	u32 ATTR_Resvd;
	u64 ATTR_AllocSz; 		//属性分配的大小
	u64 ATTR_ValidSz; 		//属性的实际大小
	u64 ATTR_InitedSz; 		//属性的初始大小
	u8 ATTR_AttrNam[0];
}__attribute__((packed)) MFT_NonResidentAttributeHeader;

//ATTRIBUTE_LIST 0X20属性体
typedef struct _MFT_Attribute20
{
	u32 AL_RD_Type;
	u16 AL_RD_Len;
	u8 AL_RD_NamLen;
	u8 AL_RD_NamOff;
	u64 AL_RD_StartVCN;		//本属性中数据流开始的簇号
	u64 AL_RD_BaseFRS;		/*本属性记录所属的MFT记录的记录号
									注意：该值的低6字节是MFT记录号，高2字节是该MFT记录的序列号*/
	u16 AL_RD_AttrId;
	//BYTE AL_RD_Name[0];
	u16 AlignmentOrReserved[3];
}__attribute__((packed)) MFT_Attribute20;

//FILE_NAME 0X30属性体
typedef struct _MFT_Attribute30 
{
	u64 FN_ParentFR; 		/*父目录的MFT记录的记录索引。
							注意：该值的低6字节是MFT记录号，高2字节是该MFT记录的序列号*/
	u64 FN_CreatTime;		//文件创建时间
	u64 FN_AlterTime;		//文件最后修改时间
	u64 FN_MFTChg;			//文件记录最后修改时间
	u64 FN_ReadTime;		//文件最后访问时间
	u64 FN_AllocSz;			//文件分配大小
	u64 FN_ValidSz;			//文件的真实尺寸
	u32 FN_DOSAttr;			//DOS文件属性
	u32 FN_EA_Reparse;		//扩展属性与链接
	u8 FN_NameSz;			//文件名的字符数
	u8 FN_NamSpace;			/*命名空间，该值可为以下值中的任意一个
								0：POSIX　可以使用除NULL和分隔符“/”之外的所有UNICODE字符，最大可以使用255个字符。注意：“：”是合法字符，但Windows不允许使用。
								1：Win32　Win32是POSIX的一个子集，不区分大小写，可以使用除““”、“＊”、“?”、“：”、“/”、“<”、“>”、“/”、“|”之外的任意UNICODE字符，但名字不能以“.”或空格结尾。
								2：DOS　DOS命名空间是Win32的子集，只支持ASCII码大于空格的8BIT大写字符并且不支持以下字符““”、“＊”、“?”、“：”、“/”、“<”、“>”、“/”、“|”、“+”、“,”、“;”、“=”；同时名字必须按以下格式命名：1~8个字符，然后是“.”，然后再是1~3个字符。
								3：Win32&DOS　这个命名空间意味着Win32和DOS文件名都存放在同一个文件名属性中。*/
	u8 FN_FileName[0];
}__attribute__((packed)) MFT_Attribute30;

//INDEX_ALLOCATION 0X80属性体
typedef struct _MFT_Attribute80
{
	u8 IA_DataRuns[0];		//UINT64 IA_DataRuns;
}__attribute__((packed)) MFT_Attribute80;

//索引头结构
typedef struct _INDEX_HEADER 
{
	u32 IH_EntryOff;		//第一个目录项的偏移
	u32 IH_TalSzOfEntries;	//目录项的总尺寸(包括索引头和下面的索引项)
	u32 IH_AllocSize;		//目录项分配的尺寸
	u8 IH_Flags;			/*标志位，此值可能是以下和值之一：
								0x00       小目录(数据存放在根节点的数据区中)
								0x01       大目录(需要目录项存储区和索引项位图)*/
	u8 IH_Resvd[3];
}__attribute__((packed)) INDEX_HEADER;

//INDEX_ROOT 0X90属性体
typedef struct _MFT_Attribute90 
{
	//索引根
	u32 IR_AttrType;		//属性的类型
	u32 IR_ColRule;			//整理规则
	u32 IR_EntrySz;			//目录项分配尺寸
	u8 IR_ClusPerRec;		//每个目录项占用的簇数
	u8 IR_Resvd[3];			//保留用于字节对齐

	INDEX_HEADER IH;		//索引头
	
	u8 IR_IndexEntry[0];	//索引项  可能不存在
}__attribute__((packed)) MFT_Attribute90;

//INDEX_ALLOCATION 0XA0属性体
typedef struct _MFT_AttributeA0 
{
	u8 IA_DataRuns[0];		//UINT64 IA_DataRuns;
}__attribute__((packed)) MFT_AttributeA0;


/*------------------------------------INDX表项的结构-------------------------------*/
//标准索引头的结构
typedef struct _INDX_HEADER 
{
	u8 SIH_Flag[4]; 				//固定值 "INDX"
	u16 SIH_USNOffset;				//更新序列号偏移
	u16 SIH_USNSize;				//更新序列号和更新数组大小
	u64 SIH_Lsn;            	 	//日志文件序列号(LSN)
	u64 SIH_IndexCacheVCN;			//本索引缓冲区在索引分配中的VCN
	u32 SIH_IndexEntryOffset;		//索引项的偏移 相对于当前位置
	u32 SIH_IndexEntrySize;			//索引项的大小
	u32 SIH_IndexEntryAllocSize;	//索引项分配的大小
	u8 SIH_HasLeafNode;				//置一 表示有子节点
	u8 SIH_Fill[3];					//填充
	u16 SIH_USN;					//更新序列号
	u8 SIH_USNArray[0];				//更新序列数组
}__attribute__((packed)) INDX_HEADER;

//标准索引项的结构
typedef struct _INDX_ENTRY 
{
	u64 SIE_MFTReferNumber;				//文件的MFT参考号
	u16 SIE_IndexEntrySize;				//索引项的大小
	u16 SIE_FileNameAttriBodySize;		//文件名属性体的大小
	u16 SIE_IndexFlag;					//索引标志
	u8 SIE_Fill[2];						//填充
	u64 SIE_FatherDirMFTReferNumber;	//父目录MFT文件参考号
	u64 SIE_CreatTime;					//文件创建时间
	u64 SIE_AlterTime;					//文件最后修改时间
	u64 SIE_MFTChgTime;					//文件记录最后修改时间
	u64 SIE_ReadTime;					//文件最后访问时间
	u64 SIE_FileAllocSize;				//文件分配大小
	u64 SIE_FileRealSize;				//文件实际大小
	u64 SIE_FileFlag;					//文件标志
	u8 SIE_FileNameSize;				//文件名长度
	u8 SIE_FileNamespace;				//文件命名空间
	u8 SIE_FileNameAndFill[0];			//文件名和填充
}__attribute__((packed)) INDX_ENTRY;



//MFT数据
typedef struct _MFT_AttributeData
{
	char *pData;
	u64 size;
	struct _MFT_AttributeData *next;
} MFT_AttributeData;

/*-------------------------------------以16进制显示读到的数据----------------------------*/

//以16进制显示读到的数据
static void showDataInHex(const unsigned char buffer[], int n)
{
	
	printf("\n\n Addr  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	int i = 0;
	for(; i < n; i++)
	{
		if(i % 16 == 0)
		{
			printf("\n0x%04X ", i);
		}
		
		printf("%02x ", buffer[i]);
	}
	
	printf("\n");
	
	return;
}


/*-------------------------将windows的UTC时间转为unix时间戳---------------------------------*/

//功能：将windows的UTC时间转为unix时间戳
//参数：Filetime：UTC时间
//返回：失败-1，成功返回unix时间戳
static unsigned long long _NTFS_Filetime2Unixtime(unsigned long long Filetime)
{
	Filetime /= 10000000;
	Filetime -= 11644473600;
	
	return Filetime;
}


/*-------------------------对文件名进行ucs2到utf8的转换---------------------------------*/

//功能：将ucs2大端转为小端
//参数：ucs2bige：ucs2的大端数据
//		size：文件名的大小
//返回：转换后的长度
static int _NTFS_Ucs2BeToUcs2Le(unsigned short *ucs2bige, unsigned int size)
{
    if (!ucs2bige) 
	{
        return 0;
    }
     
    unsigned int length = size;
    unsigned short *tmp = ucs2bige;
    
    while (*tmp && length) 
	{
        
        length--;
        unsigned char val_high = *tmp >> 8;
        unsigned char val_low = (unsigned char)*tmp;
        
        *tmp = val_low << 8 | val_high;
        
        tmp++;
    }
    
    return size - length;
}

//功能：将ucs2编码转换为utf8编码
//参数：ucs2：ucs2编码数据
//		ucs2Size：ucs2编码数据的长度(16位为一个长度)
//		utf8：存放utf8编码缓存区的指针
//		utf8Size：utf8编码缓存区的大小
//返回：成功返回utf8字符串的大小，失败-1，utf8==NULL，不转换，直接返回utf8字符串的大小
static int Ucs2ToUtf8(unsigned short *ucs2, unsigned int ucs2Size, 
					unsigned char *utf8, unsigned int utf8Size)
{
    unsigned int length = 0;
    
    if (!ucs2) 
	{
        return -1;
    }
    
    unsigned short *inbuf = ucs2;
    unsigned char *outbuf = utf8;
    
    if (!utf8) 
	{
        unsigned int insize = ucs2Size;
        
        while (*inbuf && insize) 
		{
            insize--;
            
            if (0x0080 > *inbuf) 
			{
                length++;
            } 
			else if (0x0800 > *inbuf) 
			{
                length += 2;                
            } 
			else 
			{
                length += 3;
            }
            
            inbuf++;
        }
        return length;
        
    } 
	else 
	{        
        unsigned int insize = ucs2Size;
        
        while (*inbuf && insize && length < utf8Size) 
		{            
            insize--;
            
            if (*inbuf == 0xFFFE) 
			{
                inbuf++;
                continue;
            }
            
            if (0x0080 > *inbuf) 
			{
                /* 1 byte UTF-8 Character.*/
                *outbuf++ = (unsigned char)(*inbuf);
                length++;
            } 
			else if (0x0800 > *inbuf) 
			{
                /*2 bytes UTF-8 Character.*/
                *outbuf++ = 0xc0 | ((unsigned char)(*inbuf >> 6));
                *outbuf++ = 0x80 | ((unsigned char)(*inbuf & 0x3F));
                length += 2;

            } 
			else 
			{
                /* 3 bytes UTF-8 Character .*/
                *outbuf++ = 0xE0 | ((unsigned char)(*inbuf >> 12));
                *outbuf++ = 0x80 | ((unsigned char)((*inbuf >> 6) & 0x3F));
                *outbuf++ = 0x80 | ((unsigned char)(*inbuf & 0x3F));
                length += 3; 
            }
            
            inbuf++;
        }
        
        return length;
    }
}


/*-----------------------------------函数----------------------------------------*/

//功能：获取DBR数据,并判断是否为ntfs文件系统
//参数：blkfd：磁盘设备的文件描述符
//		pDbr：DBR数据
//返回：成功0，失败-1
static int _NTFS_getDbrData(int blkfd, NTFS_DBR *pDbr)
{
	//NTFS固定标志
	const char NtfsFlag[8] = {0x4e, 0x54, 0x46, 0x53, 0x20, 0x20, 0x20, 0x20};

	//读数据
	if(lseek64(blkfd, 0, SEEK_SET) < 0)
	{
		printf("lseek64 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	if(read(blkfd, pDbr, sizeof(NTFS_DBR)) <= 0)
	{
		printf("read in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//判断扇区是不是512字节，不是返回错误
	if(pDbr->bytePerSector != 0x200)
	{
		printf("pDbr->bytePerSector = 0x%x, not 0x200\n", pDbr->bytePerSector);
		return -1;
	}
	
	//判断NTFS标志
	if(memcmp(&(pDbr->FsID), NtfsFlag, 8) != 0)
	{
		printf("NTFS flag error\n");
		return -1;
	}
	
	return 0;
}

//功能：打开MFT文件
//参数：pBlkDir：磁盘设备路径
//		pMftFd：MFT文件的描述符指针
//返回：成功0，失败-1
static int _NTFS_openMft(const char *pBlkDir, int *pMftFd)
{
	struct mntent mntent;
	memset(&mntent, 0, sizeof(struct mntent));
	char buffer[1024] = {0};
	
	//打开挂载文件
	FILE *mountTable = NULL;
	mountTable = setmntent("/proc/mounts", "r");
	if(mountTable == NULL)
	{
		printf("setmntent error\n");
		return -1;
	}
	
	//循环获取挂载文件名
	char *pMftDir = NULL;
	while (getmntent_r(mountTable, &mntent, buffer, 1024))
	{
		if(strcmp(pBlkDir, mntent.mnt_fsname) == 0)
		{
			pMftDir = (char *)malloc(strlen(mntent.mnt_dir) + 6);
			memset(pMftDir, 0, strlen(mntent.mnt_dir) + 6);
			
			sprintf(pMftDir, "%s/%s", mntent.mnt_dir, "$MFT");
			
			break;
		}
	}
	
	//打开MFT文件
	if((*pMftFd = open(pMftDir, O_RDONLY | O_LARGEFILE)) <= 0)
	{
		printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		//关闭
		free(pMftDir);
		pMftDir = NULL;
		endmntent(mountTable);
	
		return -1;
	}
	
	
	//关闭
	free(pMftDir);
	pMftDir = NULL;
	endmntent(mountTable);
	return 0;
}

//功能：关闭MFT文件
//参数：MftFd：MFT文件描述符
static void _NTFS_closeMft(int MftFd)
{
	close(MftFd);

	return;
}


//功能：根据分区挂载路径打开分区设备文件
//参数：pMountDir：分区挂载路径
//		blkFd：分区设备文件的描述符指针
//返回：成功0，失败-1
static int _NTFS_openBlkDev(const char *pMountDir, int *pBlkFd)
{
	struct mntent mntent;
	memset(&mntent, 0, sizeof(struct mntent));
	char buffer[1024] = {0};
	
	//打开挂载文件
	FILE *mountTable = NULL;
	mountTable = setmntent("/proc/mounts", "r");
	if(mountTable == NULL)
	{
		printf("setmntent error\n");
		return -1;
	}
	
	//循环获取挂载文件名
	while (getmntent_r(mountTable, &mntent, buffer, 1024))
	{
		//printf("mntent.mnt_dir = %s\n", mntent.mnt_dir);
		if(strcmp(pMountDir, mntent.mnt_dir) == 0)
		{
			//打开块设备
			if( (*pBlkFd = open(mntent.mnt_fsname, O_RDONLY | O_LARGEFILE)) <= 0 )
			{
				printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
				//关闭
				endmntent(mountTable);
				return -1;
			}
			
			break;
		}
	}
	
	//关闭
	endmntent(mountTable);
	return 0;
}

//功能：关闭MFT文件
//参数：blkFd：分区设备的文件描述符
static void _NTFS_closeBlkDev(int blkFd)
{
	close(blkFd);

	return;
}




//功能：读MFT项的数据
//参数：MftFd：MFT文件描述符
//		MftNum：MFT项的索引
//		pMftData：MFT项数据
//返回：成功0，失败-1
static int _NTFS_readMftItem(int MftFd, u64 MftNum, char *pMftData)
{
	if(lseek64(MftFd, 0x400 * MftNum, SEEK_SET) < 0)
	{
		printf("lseek64 in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		return -1;
	}
	if(read(MftFd, pMftData, 0x400) < 0)
	{
		printf("read in %s:%d:%s error : %s\n", __FILE__, __LINE__, __func__, strerror(errno));
		return -1;
	}
	
	return 0;
}






//功能：动态分配获取到的属性数据
//参数：blkFd：块设备描述符
//		pMftData：MFT文件描述符
//		attribute：要找的属性
//		pDbr：DBR扇区结构体
//		callback：回调函数，用于在同名属性中判断是否跳过
//		ppMftAttributeData：找到的动态分配的属性数据
//返回：成功0，失败-1，找不到属性_CANT_FIND_ATTRIBUTE
#define _CANT_FIND_ATTRIBUTE		1	//找不到属性
#define _SKIP_THIS_ATTRIBUTE		2	//跳过这个属性，找下一个同名属性
static int _NTFS_allocAttributeDataFromMftData(int blkFd, const char *pMftData, 
								u32 attribute, NTFS_DBR *pDbr, 
								int (*callback)(MFT_CommonAttributeHeader *pMftCommonAttributeHeader), 
								MFT_AttributeData **ppMftAttributeData)
{
	*ppMftAttributeData = NULL;
	
	MFT_HEADER *pMftHeader = (MFT_HEADER *)pMftData;
	if(strstr(pMftHeader->Type, "FILE") == NULL)
	{
		printf("strstr in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("pMftHeader->Type isn't FILE\n");
		return -1;
	}
	
	//循环找到属性
	MFT_CommonAttributeHeader *pMftCommonAttributeHeader = (MFT_CommonAttributeHeader *)((char *)pMftHeader + pMftHeader->AttributeOffset);
	while(pMftCommonAttributeHeader->ATTR_Type != (unsigned int)0xffffffff)
	{
		if(pMftCommonAttributeHeader->ATTR_Type == attribute)
		{
			//调用回调函数，跳过不想要的同名属性
			if(callback != NULL)
			{
				int err = callback(pMftCommonAttributeHeader);
				if(err == _SKIP_THIS_ATTRIBUTE)
				{
					pMftCommonAttributeHeader = (MFT_CommonAttributeHeader *)((char *)pMftCommonAttributeHeader 
							+ (pMftCommonAttributeHeader->ATTR_Size & 0xffff));
					continue;
				}
				else if(err < 0)
				{
					return -1;
				}
			}
				
			
			//动态分配空间保存数据
			*ppMftAttributeData = (MFT_AttributeData *)malloc(sizeof(MFT_AttributeData));
			memset(*ppMftAttributeData, 0, sizeof(MFT_AttributeData));
			
			MFT_AttributeData *pMftAttributeData = *ppMftAttributeData;
			
			//获取数据
			//如果为常驻属性
			if(pMftCommonAttributeHeader->ATTR_ResFlag == 0)
			{
				MFT_ResidentAttributeHeader *pMftResidentAttributeHeader = (MFT_ResidentAttributeHeader *)pMftCommonAttributeHeader;
				
				pMftAttributeData->pData = (char *)malloc(pMftResidentAttributeHeader->ATTR_DatSz + 8);
				memset(pMftAttributeData->pData, 0, pMftResidentAttributeHeader->ATTR_DatSz + 8);
				
				memcpy(pMftAttributeData->pData, 
					(char *)pMftResidentAttributeHeader + pMftResidentAttributeHeader->ATTR_DatOff, 
					pMftResidentAttributeHeader->ATTR_DatSz);
				
				pMftAttributeData->size = pMftResidentAttributeHeader->ATTR_DatSz;
			}
			
			
			//如果为非常驻属性
			else if(pMftCommonAttributeHeader->ATTR_ResFlag == 1)
			{
				MFT_NonResidentAttributeHeader *pMftNonResidentAttributeHeader = (MFT_NonResidentAttributeHeader *)pMftCommonAttributeHeader;
				
				u8 *pDataRuns = (u8 *)pMftNonResidentAttributeHeader + pMftNonResidentAttributeHeader->ATTR_DatOff;
				u8 low = 0, high = 0;
				u64 size = 0, offset = 0, offsetSum = 0;
				u8 *pMaxRange = (u8 *)pMftCommonAttributeHeader + pMftCommonAttributeHeader->ATTR_Size;
				while(*pDataRuns << 4 != 0 && *pDataRuns >> 4 != 0)
				{
					offset = 0;
					size = 0;
					
					//获取数据流
					low = (*pDataRuns) & 0x0f;
					high = (*pDataRuns) >> 4;
					pDataRuns++;
					
					memcpy(&size, pDataRuns, low);
					pDataRuns += low;
					memcpy(&offset, pDataRuns, high);
					pDataRuns += high;
					if(pDataRuns > pMaxRange)
						break;
					
					offsetSum += offset;
					offset = offsetSum * pDbr->secPerCluster * 0x200;
					size = size * pDbr->secPerCluster * 0x200;
					
					
					//获取数据
					MFT_AttributeData *p = pMftAttributeData;
					while(p->next != NULL)
						p = p->next;
					p->next = (MFT_AttributeData *)malloc(sizeof(MFT_AttributeData));
					memset(p->next, 0, sizeof(MFT_AttributeData));
					p = p->next;
					
					p->pData = (char *)malloc(size + 8);
					memset(p->pData, 0, size + 8);
					p->size = size;
					
					if(lseek64(blkFd, offset, SEEK_SET) < 0)
					{
						showDataInHex(pMftData, 0x400);
						
						printf("offset = %llx\n", offset);
						printf("lseek64 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
						return -1;
					}
					if(read(blkFd, p->pData, size) < 0)
					{
						printf("read in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
						return -1;
					}
				}
			}
			
			return 0;
		}
		
		pMftCommonAttributeHeader = (MFT_CommonAttributeHeader *)((char *)pMftCommonAttributeHeader 
							+ (pMftCommonAttributeHeader->ATTR_Size & 0xffff));
	}
	
	//printf("can't find attribute 0x%x\n", attribute);
	return _CANT_FIND_ATTRIBUTE;
}

//功能：释放动态分配的属性数据
//参数：ppMftAttributeData：属性数据指针
static void _NTFS_freeAttributeDataFromMftData(MFT_AttributeData **ppMftAttributeData)
{
	if(*ppMftAttributeData == NULL)
		return;
	
	MFT_AttributeData *pre = *ppMftAttributeData;
	MFT_AttributeData *next = pre->next;
	
	while(next != NULL)
	{
		free(pre->pData);
		free(pre);
		
		pre = next;
		next = next->next;
	}
	free(pre->pData);
	free(pre);
	
	*ppMftAttributeData = NULL;
	
	return;
}



//功能：动态分配获取到的属性数据
//		是_NTFS_allocAttributeDataFromMftData函数的升级版，如果找不到属性，会找20属性获得属性列表，再一次寻找属性
//参数：blkFd：块设备描述符
//		MftFd：Mft文件描述符
//		MftNum：MFT文件编号
//		attribute：要找的属性
//		pDbr：DBR扇区结构体
//		callback：回调函数，用于在同名属性中判断是否跳过
//		ppMftAttributeData：找到的动态分配的属性数据
//返回：成功0，失败-1，找不到属性_CANT_FIND_ATTRIBUTE
#define _CANT_FIND_ATTRIBUTE		1	//找不到属性
#define _SKIP_THIS_ATTRIBUTE		2	//跳过这个属性，找下一个同名属性
static int _NTFS_allocAttributeData(int blkFd, int MftFd, u64 MftNum, NTFS_DBR *pDbr, 
								u32 attribute, 
								int (*callback)(MFT_CommonAttributeHeader *pMftCommonAttributeHeader), 
								MFT_AttributeData **ppMftAttributeData)
{
	int err = 0;
	*ppMftAttributeData = NULL;
	
	//读MFT数据
	char MftData[0x400] = {0};
	if(_NTFS_readMftItem(MftFd, MftNum, MftData) < 0)
	{
		printf("_NTFS_readMftItem in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//获取属性数据
	err = _NTFS_allocAttributeDataFromMftData(blkFd, MftData, attribute, pDbr, callback, ppMftAttributeData);
	//如果找不到这个属性，找20属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeDataFromMftData(ppMftAttributeData);
		
		//获取20属性
		MFT_AttributeData *pMftAttribute20Data = NULL;
		err = _NTFS_allocAttributeDataFromMftData(blkFd, MftData, 0x20, pDbr, NULL, &pMftAttribute20Data);
		//如果20属性也找不到，返回
		if(err == _CANT_FIND_ATTRIBUTE)
		{
			_NTFS_freeAttributeDataFromMftData(&pMftAttribute20Data);
			return _CANT_FIND_ATTRIBUTE;
		}
		else if(err < 0)
		{
			_NTFS_freeAttributeDataFromMftData(&pMftAttribute20Data);
			return -1;
		}
		
		//如果找到20属性
		MFT_AttributeData *pTempMftAttribute20Data = pMftAttribute20Data;
		int findFlag = 0;
		while(pTempMftAttribute20Data != NULL && findFlag == 0)
		{
			//处理20属性
			u8 *pMaxRange = (u8 *)(pTempMftAttribute20Data->pData) + pTempMftAttribute20Data->size;
			MFT_Attribute20 *pMftAttribute20 = (MFT_Attribute20 *)pTempMftAttribute20Data->pData;
			//在属性列表中寻找属性
			while((u8 *)pMftAttribute20 < pMaxRange  && findFlag == 0)
			{
				//如果找到属性
				if(pMftAttribute20->AL_RD_Type == attribute)
				{
					//获取属性的mft数据
					char MftData20[0x400] = {0};
					if(_NTFS_readMftItem(MftFd, (pMftAttribute20->AL_RD_BaseFRS & 0xffffffffffff), MftData20) < 0)
					{
						printf("_NTFS_readMftItem in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
						_NTFS_freeAttributeDataFromMftData(&pMftAttribute20Data);
						return -1;
					}
					
					//获取属性数据
					err = _NTFS_allocAttributeDataFromMftData(blkFd, MftData20, attribute, pDbr, callback, ppMftAttributeData);
					if(err == _CANT_FIND_ATTRIBUTE)
					{
						_NTFS_freeAttributeDataFromMftData(ppMftAttributeData);
						pMftAttribute20 = (MFT_Attribute20 *)((u8 *)pMftAttribute20 + sizeof(MFT_Attribute20));
						continue;
					}
					else if(err < 0)
					{
						_NTFS_freeAttributeDataFromMftData(&pMftAttribute20Data);
						return -1;
					}
					
					//如果找到数据
					findFlag = 1;
					break;
				}
				
				pMftAttribute20 = (MFT_Attribute20 *)((u8 *)pMftAttribute20 + sizeof(MFT_Attribute20));
			}
			
			pTempMftAttribute20Data = pTempMftAttribute20Data->next;
		}
		
		_NTFS_freeAttributeDataFromMftData(&pMftAttribute20Data);
		
		//遍历了属性列表也没找到数据
		if(ppMftAttributeData == NULL)
			return _CANT_FIND_ATTRIBUTE;
	}
	else if(err < 0)
	{
		_NTFS_freeAttributeDataFromMftData(ppMftAttributeData);
		return -1;
	}
	
	//如果找到了属性，返回0
	return 0;
}

//功能：释放动态分配的属性数据
//参数：ppMftAttributeData：属性数据指针
static void _NTFS_freeAttributeData(MFT_AttributeData **ppMftAttributeData)
{
	_NTFS_freeAttributeDataFromMftData(ppMftAttributeData);
}







//功能：根据MFT编号获得对应的文件名
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		MftNum：MFT项编号
//		pDbr：DBR扇区结构体
//		pFileName：返回的文件名
//		FatherMftNum：父文件夹的MFT编号
//返回：成功0，失败-1
static int _NTFS_getFileNameFromMftNum(int blkFd, int MftFd, 
									u64 MftNum, NTFS_DBR *pDbr, 
									char *pFileName, u64 *FatherMftNum)
{
	int err = 0;
	
	//获得30属性，通过30属性获得文件名，一个MFT中可能没有30属性，如果没有就要通过20属性的属性列表来寻找
	//用于_NTFS_getFileNameFromMftNum函数，跳过命名空间为2的30属性
	int _NTFS_getFileNameFromMftNum_callback(MFT_CommonAttributeHeader *pMftCommonAttributeHeader)
	{
		//30属性默认是固定属性
		if(pMftCommonAttributeHeader->ATTR_ResFlag != 0)
		{
			printf("pMftCommonAttributeHeader->ATTR_ResFlag == 0 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
			return -1;
		}
		
		MFT_ResidentAttributeHeader *pMftResidentAttributeHeader = (MFT_ResidentAttributeHeader *)pMftCommonAttributeHeader;
		
		MFT_Attribute30 *p30Data = (MFT_Attribute30 *)((u8 *)pMftResidentAttributeHeader 
									+ pMftResidentAttributeHeader->ATTR_DatOff);
		
		//如果命名空间是2
		if(p30Data->FN_NamSpace == 2)
			return _SKIP_THIS_ATTRIBUTE;
		
		return 0;
	}
	
	
	
	//获取30属性数据
	MFT_AttributeData *pMftAttribute30Data = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, MftNum, pDbr, 0x30, 
						_NTFS_getFileNameFromMftNum_callback, &pMftAttribute30Data);
	//如果没找到属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		printf("can't find file name in %s:%d:%s MFT number = %lld\n", __FILE__, __LINE__, __func__, MftNum);
		_NTFS_freeAttributeData(&pMftAttribute30Data);
		return -1;
	}
	//如果出错
	else if(err < 0)
	{
		_NTFS_freeAttributeData(&pMftAttribute30Data);
		return -1;
	}
	
	
	
	//找到属性30，获取文件名
	MFT_Attribute30 *pMftAttribute30 = (MFT_Attribute30 *)pMftAttribute30Data->pData;
	Ucs2ToUtf8((unsigned short *)(pMftAttribute30->FN_FileName), 
				pMftAttribute30->FN_NameSz, 
				pFileName, 1024);
				
	*FatherMftNum = pMftAttribute30->FN_ParentFR & 0xffffffffffff;
	
	
	_NTFS_freeAttributeData(&pMftAttribute30Data);
	
	return 0;
}



//功能：处理索引项数据
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pIndexEndry：索引项头
//		pFileInformation：待返回的文件信息
//		callback：回调函数，用于获取文件列表
//		self：
//返回：成功0，失败-1
#define _SKIP_INDX	3	//跳过改索引项
static int _NTFS_getFileInformationFromIndexEndry(int blkFd, int MftFd, NTFS_DBR *pDbr, 
											INDX_ENTRY *pIndexEndry, 
											const char *pFatherFileName, 
											FILE_INFORMATION *pFileInformation, 
											int (*callback)(FILE_INFORMATION fileInformation, void *self), 
											void *self)
{
	//如果索引项大小比整个索引项还小，直接返回
	if(pIndexEndry->SIE_IndexEntrySize < sizeof(INDX_ENTRY))
		return _SKIP_INDX;
	
	//如果命名空间为2（DOS），直接返回
	if(pIndexEndry->SIE_FileNamespace == 2)
		return _SKIP_INDX;
	
	//结构体清0
	memset(pFileInformation, 0, sizeof(FILE_INFORMATION));
	
	//MFT编号
	pFileInformation->MftNum = pIndexEndry->SIE_MFTReferNumber & 0xffffffffffff;
	pFileInformation->fatherMftNum = pIndexEndry->SIE_FatherDirMFTReferNumber & 0xffffffffffff;
	
	//文件标志
	pFileInformation->fileFlag = pIndexEndry->SIE_FileFlag;
	
	//时间
	pFileInformation->createTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_CreatTime);
	pFileInformation->alterTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_AlterTime);
	pFileInformation->readTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_ReadTime);
	
	//大小
	pFileInformation->allocSize = pIndexEndry->SIE_FileAllocSize;
	pFileInformation->validSize = pIndexEndry->SIE_FileRealSize;
	
	//文件名
	u64 FatherMftNum = 0;
	_NTFS_getFileNameFromMftNum(blkFd, MftFd, 
								pFileInformation->MftNum, pDbr, 
								pFileInformation->fileName, &FatherMftNum);
	
	//全路径文件名
	pFileInformation->pFullFileName = (char *)malloc(strlen(pFileInformation->fileName)
										+ strlen(pFatherFileName) + 3);
	memset(pFileInformation->pFullFileName, 0, strlen(pFileInformation->fileName)
										+ strlen(pFatherFileName) + 3);
	sprintf(pFileInformation->pFullFileName, "%s/%s", pFatherFileName, pFileInformation->fileName);				
	
	
	
	if(callback != NULL)
		if(callback(*pFileInformation, self) < 0)
			return -1;
	
	return 0;
}


//功能：从MFT项获得文件列表（该MFT项为获得的文件列表的根文件）
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		MftNum：该文件的MFT编号
//		pFullFileName：该文件的文件名
//		callback：回调函数
//		self：
//返回：成功0，失败-1
static int _NTFS_getFileListFromMftItem(int blkFd, int MftFd, NTFS_DBR *pDbr, 
								u64 MftNum, const char *pFullFileName, 
								int (*callback)(FILE_INFORMATION fileInformation, void *self), 
								void *self)
{
	int err = 0;
	
	FILE_INFORMATION fileInformation;
	memset(&fileInformation, 0, sizeof(FILE_INFORMATION));
	

	//处理90属性
	//获取90属性数据
	MFT_AttributeData *pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, MftNum, pDbr, 0x90, NULL, &pMftAttributeData);
	//如果没找到属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}	
		
	//已经获得了90属性数据，开始处理
	MFT_Attribute90 *pMftAttribute90 = (MFT_Attribute90 *)pMftAttributeData->pData;
		
	u8 *pIndexEntry = pMftAttribute90->IR_IndexEntry;
	
	u8 *pIndexRange = (u8 *)&(pMftAttribute90->IH) 
			+ (pMftAttribute90->IH.IH_TalSzOfEntries & 0x0000ffff);
	
	
	//大于范围
	int flag = 0;
	while(pIndexEntry < pIndexRange)
	{
		//处理索引项，如果要跳过就跳过
		if(_NTFS_getFileInformationFromIndexEndry(blkFd, MftFd, pDbr, 
												(INDX_ENTRY *)pIndexEntry, 
												pFullFileName, 
												&fileInformation, 
												callback, self) != _SKIP_INDX)
		{
			//如果为目录，且MFT不为5，继续递归
			if((fileInformation.fileFlag & 0x10000010) != 0 
				&& fileInformation.MftNum != 5)
			{				
				err = _NTFS_getFileListFromMftItem(blkFd, MftFd, pDbr, fileInformation.MftNum, 
									fileInformation.pFullFileName, callback, self);
				if(err < 0)
				{
					_NTFS_freeAttributeData(&pMftAttributeData);
					return -1;
				}
			}
		}
		
		if(fileInformation.pFullFileName != NULL)
		{
			free(fileInformation.pFullFileName);
			
			fileInformation.pFullFileName = NULL;
		}
			
		
		pIndexEntry = (char *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	
	
	
	//处理a0属性
	pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, MftNum, pDbr, 0xa0, NULL, &pMftAttributeData);
	//如果没找到a0属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}
	
	
	MFT_AttributeData *pTempMftAttributeData = pMftAttributeData;
	//遍历所有数据
	while(pTempMftAttributeData != NULL)
	{		
		u32 *pIndxFlag = (u32 *)pTempMftAttributeData->pData;
		u32 *pDataRange = (u32 *)((u8 *)pTempMftAttributeData->pData + pTempMftAttributeData->size);
		//在数据中寻找INDX标志
		while(pIndxFlag < pDataRange)
		{
			//如果找到INDX标志
			if(*pIndxFlag == 0x58444e49)
			{
				INDX_HEADER *pIndxHeader = (INDX_HEADER *)pIndxFlag;
			
				u8 *pIndexEntry = (char *)&(pIndxHeader->SIH_IndexEntryOffset) 
								+ pIndxHeader->SIH_IndexEntryOffset;
								
				u8 *pIndexRange = (u8 *)pIndxHeader + pIndxHeader->SIH_IndexEntrySize;

				
				while(pIndexEntry < pIndexRange)
				{
					//处理索引项
					if(_NTFS_getFileInformationFromIndexEndry(blkFd, MftFd, pDbr, 
													(INDX_ENTRY *)pIndexEntry, 
													pFullFileName, 
													&fileInformation, 
													callback, self) != _SKIP_INDX)
					{
						//如果为目录，且MFT不为5，继续递归
						if((fileInformation.fileFlag & 0x10000010) != 0 
							&& fileInformation.MftNum != 5)	
						{							
							err = _NTFS_getFileListFromMftItem(blkFd, MftFd, pDbr, fileInformation.MftNum, 
												fileInformation.pFullFileName, callback, self);
							if(err < 0)
							{
								_NTFS_freeAttributeData(&pMftAttributeData);
								return -1;
							}
						}
					}
					
					if(fileInformation.pFullFileName != NULL)
					{
						free(fileInformation.pFullFileName);
						
						fileInformation.pFullFileName = NULL;
					}
					

					pIndexEntry = (u8 *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
				}
			}
			
			
			//8字节8字节来寻找INDX标志
			pIndxFlag = (u32 *)((u8 *)pIndxFlag + 8);
		}
		
		
		pTempMftAttributeData = pTempMftAttributeData->next;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	return 0;
}




//功能：获得文件列表（通过块设备文件）
//参数：pBlkDir：块设备路径
//		callback：回调函数，用于获取文件列表
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_getFileListByBlkDev(const char *pBlkDir, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self)
{
	//同步
	sync();
	
	//打开块设备
	int blkFd = open(pBlkDir, O_RDONLY | O_LARGEFILE);
	if(blkFd <= 0)
	{
		printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//获取DBR数据
	NTFS_DBR dbr;
	memset(&dbr, 0, sizeof(NTFS_DBR));
	if(_NTFS_getDbrData(blkFd, &dbr) < 0)
	{
		printf("_NTFS_getDbrData in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe isn't NTFS filesystem\n");
		
		close(blkFd);
		return -1;
	}
	
	//打开MFT文件
	int MftFd = 0;
	if(_NTFS_openMft(pBlkDir, &MftFd) < 0)
	{
		printf("_NTFS_openMft in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe haven't mounted device\n");
		
		close(blkFd);
		return -1;
	}
	

	
	//获取列表
	if(_NTFS_getFileListFromMftItem(blkFd, MftFd, &dbr, 5, "", callback, self) < 0)
	{
		printf("_NTFS_getFileListFromMftItem5 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		_NTFS_closeMft(MftFd);
		close(blkFd);
		return -1;
	}
	
	
	
	
	//关闭
	_NTFS_closeMft(MftFd);
	close(blkFd);
	
	return 0;
}


//功能：获得文件列表（通过挂载路径）
//参数：pMountDir：挂载路径
//		callback：回调函数，用于获取文件列表
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_getFileListByMountDir(const char *pMountDir, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self)
{
	//同步
	sync();
	
	//打开块设备
	int blkFd = 0;
	if( _NTFS_openBlkDev(pMountDir, &blkFd) < 0 )
	{
		printf("_NTFS_openBlkDev in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//获取DBR数据
	NTFS_DBR dbr;
	memset(&dbr, 0, sizeof(NTFS_DBR));
	if(_NTFS_getDbrData(blkFd, &dbr) < 0)
	{
		printf("_NTFS_getDbrData in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe isn't NTFS filesystem\n");
		
		_NTFS_closeBlkDev(blkFd);
		return -1;
	}
	
	int MftFd = 0;
	char *pMftDir = (char *)calloc(1, strlen(pMountDir) + strlen("$MFT") + 4);
	snprintf(pMftDir, strlen(pMountDir) + strlen("$MFT") + 4, "%s/%s", pMountDir, "$MFT");
	if( (MftFd = open(pMftDir, O_RDONLY | O_LARGEFILE)) <= 0 )
	{
		printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		free(pMftDir);
		_NTFS_closeBlkDev(blkFd);
		close(MftFd);
		return -1;
	}
	

	
	//获取列表
	if(_NTFS_getFileListFromMftItem(blkFd, MftFd, &dbr, 5, "", callback, self) < 0)
	{
		printf("_NTFS_getFileListFromMftItem5 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		free(pMftDir);
		_NTFS_closeBlkDev(blkFd);
		close(MftFd);
		return -1;
	}
	
	
	
	
	//关闭
	free(pMftDir);
	_NTFS_closeBlkDev(blkFd);
	close(MftFd);
	
	return 0;
}



/*---------------------------------搜索关键字-------------------------------*/

//功能：获取全路径中的第一个文件名
//参数：ppFullFileName：全路径
//		firstPath：获得的文件名
//返回：成功0，全路径到结尾了返回1
static int _NTFS_getFirstPath(char **ppFullFileName, char firstPath[256])
{
	char *p = *ppFullFileName;
	
	//跳过开头的/
	while(*p == '/')
		p++;
	
	//文件路径到结尾了，返回1
	if(*p == '\0')
		return 1;
	
	//拷贝文件路径
	int i = 0;
	for(; *p != '\0' && *p != '/'; i++, p++)
		firstPath[i] = *p;
	firstPath[i] = '\0';
	
	//移动全路径指针
	*ppFullFileName = p;
	
	return 0;
}


//功能：获得文件夹中的子文件信息（部分信息）
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pIndexEndry：索引项头
//		pFileInformation：子文件信息
//返回：成功0
static int _NTFS_getInformationOfChild(int blkFd, int MftFd, NTFS_DBR *pDbr, 
											INDX_ENTRY *pIndexEndry, 
											FILE_INFORMATION *pFileInformation)
{
	//如果索引项大小比整个索引项还小，直接返回
	if(pIndexEndry->SIE_IndexEntrySize < sizeof(INDX_ENTRY))
		return 3;
	
	//如果命名空间为2（DOS），直接返回
	if(pIndexEndry->SIE_FileNamespace == 2)
		return 3;
	
	//结构体清0
	memset(pFileInformation, 0, sizeof(FILE_INFORMATION));
	
	//MFT编号
	pFileInformation->MftNum = pIndexEndry->SIE_MFTReferNumber & 0xffffffffffff;
	pFileInformation->fatherMftNum = pIndexEndry->SIE_FatherDirMFTReferNumber & 0xffffffffffff;
	
	//文件标志
	pFileInformation->fileFlag = pIndexEndry->SIE_FileFlag;
	
	//时间
	pFileInformation->createTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_CreatTime);
	pFileInformation->alterTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_AlterTime);
	pFileInformation->readTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_ReadTime);
	
	//大小
	pFileInformation->allocSize = pIndexEndry->SIE_FileAllocSize;
	pFileInformation->validSize = pIndexEndry->SIE_FileRealSize;
	
	//文件名
	u64 FatherMftNum = 0;
	_NTFS_getFileNameFromMftNum(blkFd, MftFd, 
								pFileInformation->MftNum, pDbr, 
								pFileInformation->fileName, &FatherMftNum);
	
	return 0;
}


//功能：获得指定文件的MFT编号
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		fatherMftNum：父文件夹的MFT编号
//		pFirstPath：要找的文件名
//		pChildMftNum：返回的子文件夹的MFT编号
//返回：成功0，失败-1
static int _NTFS_getMftNumOfFirstPath(int blkFd, int MftFd, NTFS_DBR *pDbr, 
								u64 fatherMftNum, const char *pFirstPath, u64 *pChildMftNum)
{
	int err = 0;
	int flag = 0;
	
	FILE_INFORMATION fileInformation;
	memset(&fileInformation, 0, sizeof(FILE_INFORMATION));
	

	//处理90属性
	//获取90属性数据
	MFT_AttributeData *pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, fatherMftNum, pDbr, 0x90, NULL, &pMftAttributeData);
	//如果没找到属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}	
		
	//已经获得了90属性数据，开始处理
	MFT_Attribute90 *pMftAttribute90 = (MFT_Attribute90 *)pMftAttributeData->pData;
		
	u8 *pIndexEntry = pMftAttribute90->IR_IndexEntry;
	
	u8 *pIndexRange = (u8 *)&(pMftAttribute90->IH) 
			+ (pMftAttribute90->IH.IH_TalSzOfEntries & 0x0000ffff);
	
	
	//大于范围
	while(pIndexEntry < pIndexRange && flag == 0)
	{
		//处理索引项，如果要跳过就跳过
		if(_NTFS_getInformationOfChild(blkFd, MftFd, pDbr, 
									(INDX_ENTRY *)pIndexEntry, 
									&fileInformation) != _SKIP_INDX)
		{
			//找到pFirstPath
			if(strcmp(fileInformation.fileName, pFirstPath) == 0)
			{
				flag = 1;
				*pChildMftNum = fileInformation.MftNum;
			}
		}
		
		pIndexEntry = (char *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	
	
	//处理a0属性
	pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, fatherMftNum, pDbr, 0xa0, NULL, &pMftAttributeData);
	//如果没找到a0属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}
	
	
	MFT_AttributeData *pTempMftAttributeData = pMftAttributeData;
	//遍历所有数据
	while(pTempMftAttributeData != NULL && flag == 0)
	{		
		u32 *pIndxFlag = (u32 *)pTempMftAttributeData->pData;
		u32 *pDataRange = (u32 *)((u8 *)pTempMftAttributeData->pData + pTempMftAttributeData->size);
		//在数据中寻找INDX标志
		while(pIndxFlag < pDataRange && flag == 0)
		{
			//如果找到INDX标志
			if(*pIndxFlag == 0x58444e49)
			{
				INDX_HEADER *pIndxHeader = (INDX_HEADER *)pIndxFlag;
			
				u8 *pIndexEntry = (char *)&(pIndxHeader->SIH_IndexEntryOffset) 
								+ pIndxHeader->SIH_IndexEntryOffset;
								
				u8 *pIndexRange = (u8 *)pIndxHeader + pIndxHeader->SIH_IndexEntrySize;

				
				while(pIndexEntry < pIndexRange && flag == 0)
				{
					//处理索引项
					if(_NTFS_getInformationOfChild(blkFd, MftFd, pDbr, 
									(INDX_ENTRY *)pIndexEntry, 
									&fileInformation) != _SKIP_INDX)
					{
						//找到pFirstPath
						if(strcmp(fileInformation.fileName, pFirstPath) == 0)
						{
							flag = 1;
							*pChildMftNum = fileInformation.MftNum;
						}
					}

					pIndexEntry = (u8 *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
				}
			}
			
			
			//8字节8字节来寻找INDX标志
			pIndxFlag = (u32 *)((u8 *)pIndxFlag + 8);
		}
		
		
		pTempMftAttributeData = pTempMftAttributeData->next;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	if(flag == 0)
	{
		printf("can't find file : %s\n", pFirstPath);
		return -1;
	}
		
	
	return 0;
}


//功能：获得指定文件路径的MFT编号
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pFullFileName：全路径
//		MftNum：返回的文件MFT编号
//返回：成功0，失败-1
static int _NTFS_getSpecificDirMftNum(int blkFd, int MftFd, NTFS_DBR *pDbr, 
								const char *pFullFileName, u64* MftNum)
{
	int ret = 0;
	char *pTempFullFileName = (char *)pFullFileName;
	char firstPath[256] = {0};
	
	u64 fatherMftNum = 5;
	u64 childMftNum = 0;
	
	//获得全路径的第一个路径
	do
	{
		ret = _NTFS_getFirstPath(&pTempFullFileName, firstPath);
		if(ret == 1)
			break;
		
		//返回路径名相同的文件的MFT
		ret = _NTFS_getMftNumOfFirstPath(blkFd, MftFd, pDbr, fatherMftNum, firstPath, &childMftNum);
		if(ret != 0)
		{
			printf("_NTFS_getMftNumOfFirstPath in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
			return -1;
		}
		
		fatherMftNum = childMftNum;
		childMftNum = 0;
	}
	while(ret == 0);
	
	*MftNum = fatherMftNum;
	
	return 0;
}


//功能：判断关键字和文件名是否匹配
//参数：pFileName：待判断的文件名
//		ppSearchKeyList：要搜索的关键字列表
//		searchKeyCount：关键字数量
//返回：匹配返回0，不匹配返回1，错误返回-1
static int _NTFS_matchFileByKey(char *pFileName, char **ppSearchKeyList, int searchKeyCount)
{
	int res = 1;
	char *find = NULL;
	char **key = ppSearchKeyList;
	int i = 0;
	
	if(key == NULL)
		return -1;
	
	while(*key != NULL && i < searchKeyCount)
	{
		find = NULL;
		find = strstr(pFileName, *key);
		if(find != NULL)
		{
			res = 0;
			break;
		}
		
		key++;
		i++;
	}
	
	return res;
}


//功能：处理索引项数据
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pIndexEndry：索引项头
//		pFileInformation：待返回的文件信息
//		callback：回调函数，用于获取文件列表
//		self：
//返回：成功0，失败-1
#define _SKIP_INDX	3	//跳过改索引项
static int _NTFS_getSearchFileInformationFromIndexEndry(int blkFd, int MftFd, NTFS_DBR *pDbr, 
											INDX_ENTRY *pIndexEndry, 
											const char *pFatherFileName, 
											FILE_INFORMATION *pFileInformation, 
											char **ppSearchKeyList, int searchKeyCount, 
											int (*callback)(FILE_INFORMATION fileInformation, void *self), 
											void *self)
{
	//如果索引项大小比整个索引项还小，直接返回
	if(pIndexEndry->SIE_IndexEntrySize < sizeof(INDX_ENTRY))
		return 3;
	
	//如果命名空间为2（DOS），直接返回
	if(pIndexEndry->SIE_FileNamespace == 2)
		return 3;
	
	//结构体清0
	memset(pFileInformation, 0, sizeof(FILE_INFORMATION));
	
	//MFT编号
	pFileInformation->MftNum = pIndexEndry->SIE_MFTReferNumber & 0xffffffffffff;
	pFileInformation->fatherMftNum = pIndexEndry->SIE_FatherDirMFTReferNumber & 0xffffffffffff;
	
	//文件标志
	pFileInformation->fileFlag = pIndexEndry->SIE_FileFlag;
	
	//时间
	pFileInformation->createTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_CreatTime);
	pFileInformation->alterTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_AlterTime);
	pFileInformation->readTime = _NTFS_Filetime2Unixtime(pIndexEndry->SIE_ReadTime);
	
	//大小
	pFileInformation->allocSize = pIndexEndry->SIE_FileAllocSize;
	pFileInformation->validSize = pIndexEndry->SIE_FileRealSize;
	
	//文件名
	u64 FatherMftNum = 0;
	_NTFS_getFileNameFromMftNum(blkFd, MftFd, 
								pFileInformation->MftNum, pDbr, 
								pFileInformation->fileName, &FatherMftNum);
	
	//全路径文件名
	pFileInformation->pFullFileName = (char *)malloc(strlen(pFileInformation->fileName)
										+ strlen(pFatherFileName) + 3);
	memset(pFileInformation->pFullFileName, 0, strlen(pFileInformation->fileName)
										+ strlen(pFatherFileName) + 3);
	sprintf(pFileInformation->pFullFileName, "%s/%s", pFatherFileName, pFileInformation->fileName);				
	
	
	
	//判断是否匹配，匹配就调用回调函数
	if(_NTFS_matchFileByKey(pFileInformation->fileName, ppSearchKeyList, searchKeyCount) == 0
		&& *(pFileInformation->fileName) != '.')
	{
		if(callback != NULL)
			if(callback(*pFileInformation, self) < 0)
				return -1;
	}
	
	return 0;
}



//功能：从MFT项获得搜索匹配的文件列表
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		MftNum：该文件的MFT编号
//		pFullFileName：该文件的文件名
//		callback：回调函数
//		self：
//返回：成功0，失败-1
static int _NTFS_getSearchFileListFromMftItem(int blkFd, int MftFd, NTFS_DBR *pDbr, 
								u64 MftNum, const char *pFullFileName, 
								char **ppSearchKeyList, int searchKeyCount, 
								int (*callback)(FILE_INFORMATION fileInformation, void *self), 
								void *self)
{
	int err = 0;
	
	FILE_INFORMATION fileInformation;
	memset(&fileInformation, 0, sizeof(FILE_INFORMATION));
	

	//处理90属性
	//获取90属性数据
	MFT_AttributeData *pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, MftNum, pDbr, 0x90, NULL, &pMftAttributeData);
	//如果没找到属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}	
		
	//已经获得了90属性数据，开始处理
	MFT_Attribute90 *pMftAttribute90 = (MFT_Attribute90 *)pMftAttributeData->pData;
		
	u8 *pIndexEntry = pMftAttribute90->IR_IndexEntry;
	
	u8 *pIndexRange = (u8 *)&(pMftAttribute90->IH) 
			+ (pMftAttribute90->IH.IH_TalSzOfEntries & 0x0000ffff);
	
	
	//大于范围
	int flag = 0;
	while(pIndexEntry < pIndexRange)
	{
		//处理索引项，如果要跳过就跳过
		if(_NTFS_getSearchFileInformationFromIndexEndry(blkFd, MftFd, pDbr, 
												(INDX_ENTRY *)pIndexEntry, 
												pFullFileName, 
												&fileInformation, 
												ppSearchKeyList, searchKeyCount, 
												callback, self) != _SKIP_INDX)
		{
			//如果为目录，且MFT不为5，继续递归
			if((fileInformation.fileFlag & 0x10000010) != 0 
				&& fileInformation.MftNum != 5)
			{				
				err = _NTFS_getSearchFileListFromMftItem(blkFd, MftFd, pDbr, fileInformation.MftNum, 
									fileInformation.pFullFileName, ppSearchKeyList, searchKeyCount, callback, self);
				if(err < 0)
				{
					_NTFS_freeAttributeData(&pMftAttributeData);
					return -1;
				}
			}
		}
		
		if(fileInformation.pFullFileName != NULL)
		{
			free(fileInformation.pFullFileName);
			
			fileInformation.pFullFileName = NULL;
		}
			
		
		pIndexEntry = (char *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	
	
	
	//处理a0属性
	pMftAttributeData = NULL;
	err = _NTFS_allocAttributeData(blkFd, MftFd, MftNum, pDbr, 0xa0, NULL, &pMftAttributeData);
	//如果没找到a0属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return 0;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeData(&pMftAttributeData);
		return -1;
	}
	
	
	MFT_AttributeData *pTempMftAttributeData = pMftAttributeData;
	//遍历所有数据
	while(pTempMftAttributeData != NULL)
	{		
		u32 *pIndxFlag = (u32 *)pTempMftAttributeData->pData;
		u32 *pDataRange = (u32 *)((u8 *)pTempMftAttributeData->pData + pTempMftAttributeData->size);
		//在数据中寻找INDX标志
		while(pIndxFlag < pDataRange)
		{
			//如果找到INDX标志
			if(*pIndxFlag == 0x58444e49)
			{
				INDX_HEADER *pIndxHeader = (INDX_HEADER *)pIndxFlag;
			
				u8 *pIndexEntry = (char *)&(pIndxHeader->SIH_IndexEntryOffset) 
								+ pIndxHeader->SIH_IndexEntryOffset;
								
				u8 *pIndexRange = (u8 *)pIndxHeader + pIndxHeader->SIH_IndexEntrySize;

				
				while(pIndexEntry < pIndexRange)
				{
					//处理索引项
					if(_NTFS_getSearchFileInformationFromIndexEndry(blkFd, MftFd, pDbr, 
													(INDX_ENTRY *)pIndexEntry, 
													pFullFileName, 
													&fileInformation, 
													ppSearchKeyList, searchKeyCount, 
													callback, self) != _SKIP_INDX)
					{
						//如果为目录，且MFT不为5，继续递归
						if((fileInformation.fileFlag & 0x10000010) != 0 
							&& fileInformation.MftNum != 5)	
						{							
							err = _NTFS_getSearchFileListFromMftItem(blkFd, MftFd, pDbr, fileInformation.MftNum, 
												fileInformation.pFullFileName, ppSearchKeyList, searchKeyCount, callback, self);
							if(err < 0)
							{
								_NTFS_freeAttributeData(&pMftAttributeData);
								return -1;
							}
						}
					}
					
					if(fileInformation.pFullFileName != NULL)
					{
						free(fileInformation.pFullFileName);
						
						fileInformation.pFullFileName = NULL;
					}
					

					pIndexEntry = (u8 *)pIndexEntry + ((INDX_ENTRY *)pIndexEntry)->SIE_IndexEntrySize;
				}
			}
			
			
			//8字节8字节来寻找INDX标志
			pIndxFlag = (u32 *)((u8 *)pIndxFlag + 8);
		}
		
		
		pTempMftAttributeData = pTempMftAttributeData->next;
	}
	
	_NTFS_freeAttributeData(&pMftAttributeData);
	
	return 0;
}

//功能：去除字符串头和尾的/
//参数：pSlash：带/的字符串
//		Result：结果
//返回：成功0，失败-1
static int _NTFS_deleteSlash(const char *pSlash, char **ppResult)
{
	if(pSlash == NULL || ppResult == NULL)
		return -1;
	
	*ppResult = (char *)calloc(1, strlen(pSlash) + 1);
	char *pResult = *ppResult;
	
	const char *p = pSlash;
	while(*p == '/')
		p++;
	
	int i = 0;
	for(; *p != '\0' && *p != '/'; p++, i++)
		pResult[i] = *p;
	pResult[i] = '\0';
	
	
	return 0;	
}

//功能：搜索关键字
//参数：pMountDir：分区的挂载路径（如：/tmp/mnt/XXXXXX）
//		pSearchDir：要搜索的文件路径（不包含分区的挂载路径）
//		callback：回调函数，用于获取文件列表
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_searchKeys(const char *pMountDir, const char *pSearchDir, 
				char **ppSearchKeyList, int searchKeyCount, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self)
{
	//同步
	sync();
	
	//打开块设备
	int blkFd = 0;
	if( _NTFS_openBlkDev(pMountDir, &blkFd) < 0 )
	{
		printf("_NTFS_openBlkDev in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//获取DBR数据
	NTFS_DBR dbr;
	memset(&dbr, 0, sizeof(NTFS_DBR));
	if(_NTFS_getDbrData(blkFd, &dbr) < 0)
	{
		printf("_NTFS_getDbrData in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe isn't NTFS filesystem\n");
		
		_NTFS_closeBlkDev(blkFd);
		return -1;
	}
	
	//打开MFT文件
	int MftFd = 0;
	char *pMftDir = (char *)calloc(1, strlen(pMountDir) + strlen("$MFT") + 4);
	snprintf(pMftDir, strlen(pMountDir) + strlen("$MFT") + 4, "%s/%s", pMountDir, "$MFT");
	if( (MftFd = open(pMftDir, O_RDONLY | O_LARGEFILE)) <= 0 )
	{
		printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		free(pMftDir);
		return -1;
	}
	

	//获取指定路径的MftNum
	u64 MftNum = 0;
	if(_NTFS_getSpecificDirMftNum(blkFd, MftFd, &dbr, pSearchDir, &MftNum) < 0)
	{
		printf("_NTFS_getSpecificDirMft in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		free(pMftDir);
		close(MftFd);
		return -1;
	}
	printf("MftNum = %lld\n", MftNum);

	
	//获取列表
	char *pFullPath = NULL;
	char *pTempSearchDir = NULL;
	_NTFS_deleteSlash(pSearchDir, &pTempSearchDir);
	pFullPath = (char *)calloc(1, strlen(pMountDir) + strlen(pTempSearchDir) + 4);
	if(strlen(pTempSearchDir) == 0)
		snprintf(pFullPath, strlen(pMountDir) + strlen(pTempSearchDir) + 4, "%s%s", pMountDir, pTempSearchDir);
	else
		snprintf(pFullPath, strlen(pMountDir) + strlen(pTempSearchDir) + 4, "%s/%s", pMountDir, pTempSearchDir);		
	free(pTempSearchDir);
	
	if(_NTFS_getSearchFileListFromMftItem(blkFd, MftFd, &dbr, MftNum, pFullPath, ppSearchKeyList, searchKeyCount, callback, self) < 0)
	{
		printf("_NTFS_getSearchFileListFromMftItem in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		_NTFS_closeBlkDev(blkFd);
		if(pFullPath)
			free(pFullPath);
		close(MftFd);
		free(pMftDir);
		return -1;
	}
	
	
	
	
	//关闭
	if(pFullPath)
		free(pFullPath);
	_NTFS_closeBlkDev(blkFd);
	close(MftFd);
	free(pMftDir);
	
	return 0;
}











/*--------------------------------------USN-----------------------------------*/

//USN记录结构体
typedef struct _USN_RECORD
{
	unsigned int        RecordLength;				//记录长度
	unsigned short      MajorVersion;				//主版本
	unsigned short      MinorVersion;				//次版本
	unsigned long long  FileReferenceNumber;		//文件引用数，前六位为文件的MFT编号
	unsigned long long  ParentFileReferenceNumber;	//父目录引用数，前六位为文件所在目录的MFT编号
	unsigned long long  Usn;						//USN，记录该USN的偏移
	unsigned long long 	TimeStamp;					//时间戳
	unsigned int        Reason;						//原因
	unsigned int        SourceInfo;					//源信息
	unsigned int        SecurityId;					//安全
	unsigned int        FileAttributes;				//文件属性
	unsigned short      FileNameLength;				//文件长度
	unsigned short      FileNameOffset;				//文件名偏移
	unsigned char       FileName[];					//文件名
} __attribute__((packed)) USN_RECORD;


//功能：获取USN文件的MFT编号
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pMftNum：返回的USN的文件MFT编号
//返回：成功0，失败-1
static int _NTFS_getUsnMft(int blkFd, int MftFd, NTFS_DBR *pDbr, u64 *pMftNum)
{
	//定义回调函数，找到$Extend/$UsnJrnl文件就返回
	int __FindUsnCallback(FILE_INFORMATION fileInformation, void *self)
	{
		u64 *pMftNum = (u64 *)self;
		
		//如果找到了USN文件，记录其MFT编号
		if(strcmp(fileInformation.pFullFileName, "$Extend/$UsnJrnl") == 0)
		{
			*pMftNum = fileInformation.MftNum;
		}
		
		return 0;
	}
	
	if(_NTFS_getFileListFromMftItem(blkFd, MftFd, pDbr, 11, "$Extend", __FindUsnCallback, pMftNum) < 0)
	{
		printf("_NTFS_getFileListFromMftItem5 in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		_NTFS_closeMft(MftFd);
		close(blkFd);
		return -1;
	}
	
	return 0;
}


//功能：根据MFT编号获得对应的全路径名
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		MftNum：MFT文件编号
//		pDbr：DBR扇区结构体
//		ppFullFileName：返回的全路径指针
//返回：成功0，失败-1
static int _NTFS_allocFullFileNameFromMftNum(int blkFd, int MftFd, 
									u64 MftNum, NTFS_DBR *pDbr, 
									char **ppFullFileName)
{
	//根据MFT编号获得对应的文件名和父文件MFT编号
	u64 FatherMftNum = 0;
	char fileName[1024] = {0};
	if(_NTFS_getFileNameFromMftNum(blkFd, MftFd, MftNum, pDbr, fileName, &FatherMftNum) < 0)
	{
		printf("_NTFS_getFileNameFromMftNum in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	*ppFullFileName = (char *)malloc(strlen(fileName) + 2);
	memset(*ppFullFileName, 0, strlen(fileName) + 2);
	strcat(*ppFullFileName, fileName);
	memset(fileName, 0, 1024);
	
	
	while(FatherMftNum != 5)
	{
		if(_NTFS_getFileNameFromMftNum(blkFd, MftFd, FatherMftNum, pDbr, fileName, &FatherMftNum) < 0)
		{
			printf("_NTFS_getFileNameFromMftNum in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
			return -1;
		}
		
		
		char *tempFullFileName = *ppFullFileName;
		*ppFullFileName = (char *)malloc(strlen(tempFullFileName) + strlen(fileName) + 8);
		memset(*ppFullFileName, 0, strlen(tempFullFileName) + strlen(fileName) + 8);
		sprintf(*ppFullFileName, "%s/%s", fileName, tempFullFileName);
		
		memset(fileName, 0, 1024);
		free(tempFullFileName);
	}
	
	//printf("*ppFullFileName = %s\n", *ppFullFileName);
	
	return 0;
}

//功能：释放动态分配的全路径
//参数：ppFullFileName：返回的全路径指针
static void _NTFS_freeFullFileName(char **ppFullFileName)
{
	if(*ppFullFileName != NULL)
		free(*ppFullFileName);
	
	ppFullFileName = NULL;
}



//功能：分析Usn数据
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		pDbr：DBR扇区结构体
//		pUsnRecord：USN记录结构体的数据指针
//		pUsnInformation：返回的USN记录
//		callback：回调函数
//		self：
//返回：成功0，失败-1
static int _NTFS_parseUsnRecord(int blkFd, int MftFd, NTFS_DBR *pDbr,
							USN_RECORD *pUsnRecord, 
							USN_INFORMATION *pUsnInformation, 
							int (*callback)(USN_INFORMATION UsnInformation, void *self), 
							void *self)
{
	//如果版本不是2.0，报错
	if(pUsnRecord->MajorVersion != 2 || pUsnRecord->MinorVersion != 0)
	{
		printf("USN Version isn't 2.0\n");
		return -1;
	}
	
	//获取文件的MFT编号和父文件夹的MFT编号
	pUsnInformation->MFTnum = pUsnRecord->FileReferenceNumber & 0xffffffffffff;
	pUsnInformation->FatherMFTnum = pUsnRecord->ParentFileReferenceNumber & 0xffffffffffff;
	
	//获取文件改变的时间戳
	pUsnInformation->TimeStamp = _NTFS_Filetime2Unixtime(pUsnRecord->TimeStamp);
	
	//获取文件改变的原因
	pUsnInformation->Reason = pUsnRecord->Reason;
	
	//获取文件的文件属性
	pUsnInformation->FileAttributes = pUsnRecord->Reason;
	
	//获取文件名
	char *pFileName = (char *)pUsnRecord + pUsnRecord->FileNameOffset;
	Ucs2ToUtf8((unsigned short *)pFileName, 
				pUsnRecord->FileNameLength / 2, 
				pUsnInformation->fileName, 1024);
	
	//获取全路径
	if(_NTFS_allocFullFileNameFromMftNum(blkFd, MftFd, pUsnInformation->MFTnum, pDbr, 
										&(pUsnInformation->pFullFileName)) < 0)
	{
		printf("_NTFS_allocFullFileNameFromMftNum in %s:%d error \n", __FILE__, __LINE__);
		
		_NTFS_freeFullFileName(&(pUsnInformation->pFullFileName));
		return -1;
	}
		
	//调用回调函数
	if(callback != NULL)
	{
		if(callback(*pUsnInformation, self) < 0)
		{
			printf("callback in %s:%d error \n", __FILE__, __LINE__);
			
			_NTFS_freeFullFileName(&(pUsnInformation->pFullFileName));
			return -1;
		}
	}
	
	_NTFS_freeFullFileName(&(pUsnInformation->pFullFileName));
	return 0;
}

//功能：获取并分析USN数据
//参数：blkFd：块设备描述符
//		MftFd：MFT文件描述符
//		MftNum：USN文件的MFT编号
//		pDbr：DBR扇区结构体
//		callback：回调函数
//		self：
//返回：成功0，失败-1
static int _NTFS_getAndParseUsnData(int blkFd, int MftFd, int MftNum, NTFS_DBR *pDbr, 
							int (*callback)(USN_INFORMATION UsnInformation, void *self), 
							void *self)
{
	int err = 0;
	USN_INFORMATION UsnInformation;
	memset(&UsnInformation, 0, sizeof(USN_INFORMATION));
	
	//读USN的MFT，找到$J（J流）数据
	//读MFT
	char MftData[0x400] = {0};
	if(_NTFS_readMftItem(MftFd, MftNum, MftData) < 0)
	{
		printf("_NTFS_readMftItem in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//找到80属性
	//定义回调函数
	int __FindJStreamCallback(MFT_CommonAttributeHeader *pMftCommonAttributeHeader)
	{
		u8 attributeName[1024] = {0};		

		char *pStreamName = (char *)pMftCommonAttributeHeader + pMftCommonAttributeHeader->ATTR_NamOff;
		Ucs2ToUtf8((unsigned short *)pStreamName, 
					pMftCommonAttributeHeader->ATTR_NamSz, 
					attributeName, 1024);
			
		printf("attributeName = %s\n", attributeName);	
			
		//如果不是J流数据，跳过
		if(strcmp(attributeName, "$J") != 0)
			return _SKIP_THIS_ATTRIBUTE;
		
		return 0;
	}
	
	MFT_AttributeData *pMftAttributeJStreamData = NULL;
	err = _NTFS_allocAttributeDataFromMftData(blkFd, MftData, 0x80, pDbr, __FindJStreamCallback, &pMftAttributeJStreamData);
	//如果没找到属性
	if(err == _CANT_FIND_ATTRIBUTE)
	{
		printf("can't find $J stream\n");
		
		_NTFS_freeAttributeDataFromMftData(&pMftAttributeJStreamData);
		return -1;
	}
	//如果出错
	else if(err == -1)
	{
		_NTFS_freeAttributeDataFromMftData(&pMftAttributeJStreamData);
		return -1;
	}	
	
	//找到了$J流数据，分析数据
	MFT_AttributeData *p = pMftAttributeJStreamData;
	while(p != NULL)
	{
		USN_RECORD *pUsnRecord = (USN_RECORD *)p->pData;
		char *pMaxRange = p->pData + p->size;
		while((char *)pUsnRecord < pMaxRange && pUsnRecord->RecordLength != 0)
		{
			memset(&UsnInformation, 0, sizeof(USN_INFORMATION));
			
			//分析Usn数据
			if(_NTFS_parseUsnRecord(blkFd, MftFd, pDbr, pUsnRecord, &UsnInformation, 
							callback, self) < 0)
			{
				_NTFS_freeAttributeDataFromMftData(&pMftAttributeJStreamData);
				return -1;
			}
			
			pUsnRecord = (USN_RECORD *)((char *)pUsnRecord + pUsnRecord->RecordLength);
		}
		
		p = p->next;
	}
	
	//返回
	_NTFS_freeAttributeDataFromMftData(&pMftAttributeJStreamData);
	return 0;
}


//功能：获得文件列表
//参数：pBlkDir：块设备路径
//		callback：回调函数
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_getUSNjournalInformation(char *pBlkDir, 
							int (*callback)(USN_INFORMATION UsnInformation, void *self), 
							void *self)
{
	//同步
	sync();
	
	//打开块设备
	int blkFd = open(pBlkDir, O_RDONLY | O_LARGEFILE);
	if(blkFd <= 0)
	{
		printf("open in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		return -1;
	}
	
	//获取DBR数据
	NTFS_DBR dbr;
	memset(&dbr, 0, sizeof(NTFS_DBR));
	if(_NTFS_getDbrData(blkFd, &dbr) < 0)
	{
		printf("_NTFS_getDbrData in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe isn't NTFS filesystem\n");
		
		close(blkFd);
		return -1;
	}
	
	//打开MFT文件
	int MftFd = 0;
	if(_NTFS_openMft(pBlkDir, &MftFd) < 0)
	{
		printf("_NTFS_openMft in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		printf("maybe haven't mounted device\n");
		
		close(blkFd);
		return -1;
	}
	
	
	
	//获取USN的MFT编号
	u64 MftNum = 0;
	if(_NTFS_getUsnMft(blkFd, MftFd, &dbr, &MftNum) < 0)
	{
		printf("_NTFS_getUsnMft in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		_NTFS_closeMft(MftFd);
		close(blkFd);
		return -1;
	}
	
	
	
	
	//分析USN数据
	if(_NTFS_getAndParseUsnData(blkFd, MftFd, MftNum, &dbr, callback, self) < 0)
	{
		printf("_NTFS_getAndParseUsnData in %s:%d:%s error\n", __FILE__, __LINE__, __func__);
		
		_NTFS_closeMft(MftFd);
		close(blkFd);
		return -1;
	}
	
	
	//关闭
	_NTFS_closeMft(MftFd);
	close(blkFd);
	
	return 0;
}


















