typedef enum {
	WRITE,
	TRUNCATE,
	READ_LAYOUT,
	CLONE,
	GET_INODE,
	GET_INODE_WHOLE,
	SET_INODE,
	CREATE_INODE,
	DELETE_INODE,
	BACKUP_AND_STOP
} INS_COM;

char *comstr[] = {
	"WRITE",
	"TRUNCATE",
	"READ_LAYOUT",
	"CLONE",
	"GET_INODE",
	"GET_INODE_WHOLE",
	"SET_INODE",
	"CREATE_INODE",
	"DELETE_INODE",
	"BACKUP_AND_STOP"
};

typedef enum {
	SERVER_BUSY,
	QUEUED,
	SUCCESS,
	FAILED	
} INS_RET;

char *retstr[] = {
	"SERVER_BUSY",
	"QUEUED",
	"SUCCESS",
	"FAILED"	
};
