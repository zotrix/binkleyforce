

#define NS_NOTINIT	(0)
#define	NS_UNCONF	(1)
#define	NS_ERROR	(2)
#define	NS_READY	(3)
#define NS_RECEIVING	(4)
#define NS_RECVFILE	(5)
#define NS_CLOSED	(6)

#define STRBUF (1024)

typedef struct {
	int	state;
	char	*error;
	int	socket;
	int	filename[STRBUF];
	unsigned long long length;
} s_netspool_state;

