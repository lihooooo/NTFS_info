# NTFS_info

读取NTFS文件系统的信息，获取分区下所有文件的信息
程序直接读取块设备文件进行信息提取，在文件系统没有缓存进内存的情况下，比通过调用`opendir`和`readdir`获取文件信息快一倍以上。