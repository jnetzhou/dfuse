#ifndef ADB_BRIDGE_H_
#define ADB_BRIDGE_H_

typedef void (*sync_ls_cb)(unsigned mode, unsigned size, unsigned time,
		const char *name, void *cookie);

extern int sync_ls(int fd, const char *path, sync_ls_cb func, void *cookie);
extern void sync_quit(int fd);

#endif /* ADB_BRIDGE_H_ */

