# include <sys/types.h>
# include <pwd.h>
# include <stdio.h>
# include <sysexits.h>
# include <ctype.h>

static char	SccsId[] =	"@(#)vacation.c	4.2		%G%";

/*
**  VACATION -- return a message to the sender when on vacation.
**
**	This program could be invoked as a message receiver
**	when someone is on vacation.  It returns a message
**	specified by the user to whoever sent the mail, taking
**	care not to return a message too often to prevent
**	"I am on vacation" loops.
**
**	Positional Parameters:
**		the user to collect the vacation message from.
**
**	Flag Parameters:
**		-I	initialize the database.
**
**	Side Effects:
**		A message is sent back to the sender.
**
**	Author:
**		Eric Allman
**		UCB/INGRES
*/

typedef int	bool;

# define TRUE		1
# define FALSE		0

# define MAXLINE	256	/* max size of a line */
# define MAXNAME	128	/* max size of one name */

# define ONEWEEK	(60L*60L*24L*7L)

time_t	Timeout = ONEWEEK;	/* timeout between notices per user */

struct dbrec
{
	long	sentdate;
};

typedef struct
{
	char	*dptr;
	int	dsize;
} DATUM;

extern DATUM fetch();

main(argc, argv)
	char **argv;
{
	char *from;
	register char *p;
	struct passwd *pw;
	char *homedir;
	char *myname;
	char buf[MAXLINE];
	extern struct passwd *getpwnam();
	extern char *newstr();
	extern char *getfrom();
	extern bool knows();
	extern time_t convtime();

	/* process arguments */
	while (--argc > 0 && (p = *++argv) != NULL && *p == '-')
	{
		switch (*++p)
		{
		  case 'I':	/* initialize */
			initialize();
			exit(EX_OK);

		  default:
			usrerr("Unknown flag -%s", p);
			exit(EX_USAGE);
		}
	}

	/* verify recipient argument */
	if (argc != 1)
	{
		usrerr("Usage: vacation username (or) vacation -I");
		exit(EX_USAGE);
	}

	myname = p;

	/* find user's home directory */
	pw = getpwnam(myname);
	if (pw == NULL)
	{
		usrerr("Unknown user %s", myname);
		exit(EX_NOUSER);
	}
	homedir = newstr(pw->pw_dir);
	(void) strcpy(buf, homedir);
	(void) strcat(buf, "/.vacation");
	dbminit(buf);

	/* read message from standard input (just from line) */
	from = getfrom();

	/* check if this person is already informed */
	if (!knows(from))
	{
		/* mark this person as knowing */
		setknows(from);

		/* send the message back */
		(void) strcpy(buf, homedir);
		(void) strcat(buf, "/.vacation.msg");
		sendmessage(buf, from, myname);
		/* never returns */
	}
	exit (EX_OK);
}
/*
**  GETFROM -- read message from standard input and return sender
**
**	Parameters:
**		none.
**
**	Returns:
**		pointer to the sender address.
**
**	Side Effects:
**		Reads first line from standard input.
*/

char *
getfrom()
{
	static char line[MAXLINE];
	register char *p;
	extern char *index();

	/* read the from line */
	if (fgets(line, sizeof line, stdin) == NULL ||
	    strncmp(line, "From ", 5) != NULL)
	{
		usrerr("No initial From line");
		exit(EX_USAGE);
	}

	/* find the end of the sender address and terminate it */
	p = index(&line[5], ' ');
	if (p == NULL)
	{
		usrerr("Funny From line '%s'", line);
		exit(EX_USAGE);
	}
	*p = '\0';

	/* return the sender address */
	return (&line[5]);
}
/*
**  KNOWS -- predicate telling if user has already been informed.
**
**	Parameters:
**		user -- the user who sent this message.
**
**	Returns:
**		TRUE if 'user' has already been informed that the
**			recipient is on vacation.
**		FALSE otherwise.
**
**	Side Effects:
**		none.
*/

bool
knows(user)
	char *user;
{
	DATUM k, d;
	long now;

	time(&now);
	k.dptr = user;
	k.dsize = strlen(user) + 1;
	d = fetch(k);
	if (d.dptr == NULL || ((struct dbrec *) d.dptr)->sentdate + Timeout < now)
		return (FALSE);
	return (TRUE);
}
/*
**  SETKNOWS -- set that this user knows about the vacation.
**
**	Parameters:
**		user -- the user who should be marked.
**
**	Returns:
**		none.
**
**	Side Effects:
**		The dbm file is updated as appropriate.
*/

setknows(user)
	char *user;
{
	DATUM k, d;
	struct dbrec xrec;

	k.dptr = user;
	k.dsize = strlen(user) + 1;
	time(&xrec.sentdate);
	d.dptr = (char *) &xrec;
	d.dsize = sizeof xrec;
	store(k, d);
}
/*
**  SENDMESSAGE -- send a message to a particular user.
**
**	Parameters:
**		msgf -- filename containing the message.
**		user -- user who should receive it.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sends mail to 'user' using /usr/lib/sendmail.
*/

sendmessage(msgf, user, myname)
	char *msgf;
	char *user;
	char *myname;
{
	FILE *f;

	/* find the message to send */
	f = freopen(msgf, "r", stdin);
	if (f == NULL)
	{
		f = freopen("/usr/lib/vacation.def", "r", stdin);
		if (f == NULL)
			syserr("No message to send");
	}

	execl("/usr/lib/sendmail", "sendmail", "-f", myname, user, NULL);
	syserr("Cannot exec /usr/lib/sendmail");
}
/*
**  INITIALIZE -- initialize the database before leaving for vacation
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes the files .vacation.{pag,dir} in the
**		caller's home directory.
*/

initialize()
{
	char *homedir;
	char buf[MAXLINE];
	extern char *getenv();

	setgid(getgid());
	setuid(getuid());
	homedir = getenv("HOME");
	if (homedir == NULL)
		syserr("No home!");
	(void) strcpy(buf, homedir);
	(void) strcat(buf, "/.vacation.dir");
	if (close(creat(buf, 0644)) < 0)
		syserr("Cannot create %s", buf);
	(void) strcpy(buf, homedir);
	(void) strcat(buf, "/.vacation.pag");
	if (close(creat(buf, 0644)) < 0)
		syserr("Cannot create %s", buf);
}
/*
**  USRERR -- print user error
**
**	Parameters:
**		f -- format.
**		p -- first parameter.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

usrerr(f, p)
	char *f;
	char *p;
{
	fprintf(stderr, "vacation: ");
	_doprnt(f, &p, stderr);
	fprintf(stderr, "\n");
}
/*
**  SYSERR -- print system error
**
**	Parameters:
**		f -- format.
**		p -- first parameter.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

syserr(f, p)
	char *f;
	char *p;
{
	fprintf(stderr, "vacation: ");
	_doprnt(f, &p, stderr);
	fprintf(stderr, "\n");
	exit(EX_USAGE);
}
/*
**  NEWSTR -- copy a string
**
**	Parameters:
**		s -- the string to copy.
**
**	Returns:
**		A copy of the string.
**
**	Side Effects:
**		none.
*/

char *
newstr(s)
	char *s;
{
	char *p;
	extern char *malloc();

	p = malloc(strlen(s) + 1);
	if (p == NULL)
	{
		syserr("newstr: cannot alloc memory");
		exit(EX_OSERR);
	}
	strcpy(p, s);
	return (p);
}
