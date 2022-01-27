#ifndef __NTFS_H
#define __NTFS_H

//宏定义文件类型
#define FILE_ATTRIBUTE_READONLY				0x00000001	//只读
#define FILE_ATTRIBUTE_HIDDEN				0x00000002	//隐藏
#define FILE_ATTRIBUTE_SYSTEM				0x00000004	//系统文件或目录
#define FILE_ATTRIBUTE_VOLUME_LABEL			0x00000008	//卷标记（NTFS不使用）
#define FILE_ATTRIBUTE_DIRECTORY			0x00000010	//文件夹
#define FILE_ATTRIBUTE_ARCHIVE				0x00000020	//包
#define	FILE_ATTRIBUTE_DEVICE				0x00000040	//设备
#define	FILE_ATTRIBUTE_NORMAL				0x00000080	//普通
#define	FILE_ATTRIBUTE_TEMPORARY			0x00000100	//临时
#define	FILE_ATTRIBUTE_SPARSE_FILE			0x00000200	//稀疏文件
#define	FILE_ATTRIBUTE_REPARSE_POINT		0x00000400	//重解析点或符号链接
#define	FILE_ATTRIBUTE_COMPRESSED			0x00000800	//压缩
#define	FILE_ATTRIBUTE_OFFLINE				0x00001000	//离线，文件数据存储在离线设备
#define	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	0x00002000	//不能索引
#define FILE_ATTRIBUTE_ENCRYPTED			0x00004000	//加密
#define FILE_ATTRIBUTE_UNKNOWN1				0x00008000	//母鸡
#define FILE_ATTRIBUTE_VIRTUAL				0x00010000	//虚拟
#define FILE_ATTRIBUTE_DIRECTORY2			0x10000000	//也用于表示目录
#define	FILE_ATTRIBUTE_INDEX_VIEW			0x20000000	//索引视图




typedef struct _FILE_INFORMATION		//文件信息结构体
{
	unsigned long long MftNum;			//文件的MFT编号
	unsigned long long fatherMftNum;	//父文件夹的MFT编号
	
	unsigned long long fileFlag;		//文件类型
	
	unsigned long long createTime;		//文件的创建时间
	unsigned long long alterTime;		//文件最后修改时间
	unsigned long long readTime;		//文件最后访问时间
	
	unsigned long long allocSize;		//文件分配大小
	unsigned long long validSize;		//文件的真实尺寸
	
	char fileName[1024];				//文件名
	char *pFullFileName;				//全文件名
} FILE_INFORMATION;


//宏定义原因
#define USN_REASON_BASIC_INFO_CHANGE 		0x00008000	//文件属性改变
#define USN_REASON_CLOSE 					0x80000000	//文件关闭
#define USN_REASON_COMPRESSION_CHANGE		0x00020000	//文件压缩状态改变
#define USN_REASON_DATA_EXTEND				0x00000002	//文件增加了数据
#define USN_REASON_DATA_OVERWRITE			0x00000001	//文件数据重写了
#define USN_REASON_DATA_TRUNCATION			0x00000004	//文件数据流被截断了
#define USN_REASON_EA_CHANGE				0x00000400	//文件扩展属性改变了
#define USN_REASON_ENCRYPTION_CHANGE		0x00040000	//文件被加密或解密了
#define USN_REASON_FILE_CREATE				0x00000100	//文件创建
#define USN_REASON_FILE_DELETE				0x00000200	//文件删除
#define USN_REASON_HARD_LINK_CHANGE			0x00010000	//NTFS硬链接改变了
#define USN_REASON_INDEXABLE_CHANGE			0x00004000	
#define USN_REASON_INTEGRITY_CHANGE			0x00800000
#define USN_REASON_NAMED_DATA_EXTEND		0x00000020	//文件名数据增加
#define USN_REASON_NAMED_DATA_OVERWRITE		0x00000010	//文件名数据重写了
#define USN_REASON_NAMED_DATA_TRUNCATION	0x00000040	//文件名数据截断了
#define USN_REASON_OBJECT_ID_CHANGE			0x00080000	//文件对象标识符改变了
#define USN_REASON_RENAME_NEW_NAME			0x00002000	//文件目录重命名了，新的名字
#define USN_REASON_RENAME_OLD_NAME			0x00001000	//文件目录重命名了，旧的名字
#define	USN_REASON_REPARSE_POINT_CHANGE		0x00100000	//文件目录多分点属性改变
#define USN_REASON_SECURITY_CHANGE			0x00000800	//访问权限改变了
#define USN_REASON_STREAM_CHANGE			0x00200000	//流改变
#define USN_REASON_TRANSACTED_CHANGE		0x00400000


//USN信息结构体
typedef struct _USN_INFORMATION
{
	unsigned long long MFTnum;
	unsigned long long FatherMFTnum;
	unsigned long long TimeStamp;
	unsigned int Reason;
	unsigned int FileAttributes;
	char fileName[1024];
	char *pFullFileName;
} USN_INFORMATION;





//功能：获得文件列表
//参数：pBlkDir：块设备路径
//		callback：回调函数
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_getFileListByBlkDev(const char *pBlkDir, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self);

//功能：获得文件列表（通过挂载路径）
//参数：pMountDir：挂载路径
//		callback：回调函数，用于获取文件列表
//		self：回调函数参数
//返回：成功0，失败-1	
int NTFS_getFileListByMountDir(const char *pMountDir, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self);

//功能：搜索关键字
//参数：pMountDir：分区的挂载路径（如：/tmp/mnt/XXXXXX）
//		pSearchDir：要搜索的文件路径（不包含分区的挂载路径）
//		callback：回调函数，用于获取文件列表
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_searchKeys(const char *pMountDir, const char *pSearchDir, 
				char **ppSearchKeyList, int searchKeyCount, 
				int (*callback)(FILE_INFORMATION fileInformation, void *self), 
				void *self);

//功能：获得文件列表
//参数：pBlkDir：块设备路径
//		callback：回调函数
//		self：回调函数参数
//返回：成功0，失败-1
int NTFS_getUSNjournalInformation(char *pBlkDir, 
							int (*callback)(USN_INFORMATION USNInformation, void *self), 
							void *self);





#endif /* __NTFS_H */


