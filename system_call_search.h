#ifndef __SYSTEM_CALL_SEARCH_H
#define __SYSTEM_CALL_SEARCH_H





int systemCallSearch(const char *pDirPath, char **search_key_list, int search_key_count, int currentOnly, 
		int (*callback)(FILE_INFORMATION fileInformation, void *self), void *self);

























#endif /* __SYSTEM_CALL_SEARCH_H */


