/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <fnmatch.h>
#include <Evas.h>

/* local subsystem functions */
static int auth_action_ok(char *a);
static int auth_etc_enlightenment_sysactions(char *a, char *u, char *g);
static char *get_word(char *s, char *d);

/* local subsystem globals */
Evas_Hash *actions = NULL;

/* externally accessible functions */
int
main(int argc, char **argv)
{
   int i;
   int test = 0;
   char *action, *cmd;

   for (i = 1; i < argc; i++)
     {
	if ((!strcmp(argv[i], "-h")) ||
	    (!strcmp(argv[i], "-help")) ||
	    (!strcmp(argv[i], "--help")))
	  {
	     printf(
		    "This is an internal tool for Enlightenment.\n"
		    "do not use it.\n"
		    );
	     exit(0);
	  }
     }
   if (argc == 3)
     {
	if (!strcmp(argv[1], "-t")) test = 1;
	action = argv[2];
     }
   else if (argc == 2)
     {
	action = argv[1];
     }
   else
     {
	exit(-1);
     }
   
   evas_init();

   if (!auth_action_ok(action))
     {
	printf("ERROR: ACTION NOT ALLOWED: %s\n", action);
	exit(10);
     }
   /* we can add more levels of auth here */
   
   cmd = evas_hash_find(actions, action);
   if (!cmd)
     {
	printf("ERROR: UNDEFINED ACTION: %s\n", action);
	exit(20);
     }
   if (!test) return system(cmd);
   
   evas_shutdown();
   
   return 0;
}

/* local subsystem functions */
static int
auth_action_ok(char *a)
{
   struct passwd *pw;
   struct group *gp;
   char *usr = NULL, *grp;
   int ret;

   pw = getpwuid(getuid());
   if (!pw) return 0;
   usr = pw->pw_name;
   if (!usr) return 0;
   gp = getgrgid(getgid());
   if (gp) grp = gp->gr_name;
   /* first stage - check:
    * PREFIX/etc/enlightenment/sysactions.conf
    */
   ret = auth_etc_enlightenment_sysactions(a, usr, grp);
   if (ret == 1) return 1;
   else if (ret == -1) return 0;
   /* the DEFAULT - allow */
   return 1;
}

static int
auth_etc_enlightenment_sysactions(char *a, char *u, char *g)
{
   FILE *f;
   char file[4096], buf[4096], id[4096], ugname[4096], perm[4096], act[4096];
   char *p, *pp, *s;
   int len, line = 0, ok = 0;
   int allow = 0;
   int deny = 0;
   
   snprintf(file, sizeof(file), "/etc/enlightenment/sysactions.conf");
   f = fopen(file, "r");
   if (!f)
     {
	snprintf(file, sizeof(file), PACKAGE_SYSCONF_DIR"/enlightenment/sysactions.conf");
	f = fopen(file, "r");
	if (!f) return 0;
     }
   while (fgets(buf, sizeof(buf), f))
     {
	line++;
	len = strlen(buf);
	if (len < 1) continue;
	if (buf[len - 1] == '\n') buf[len - 1] = 0;
	/* format:
	 * 
	 * # comment
	 * user:  username  [allow:|deny:] halt reboot ...
	 * group: groupname [allow:|deny:] suspend ...
	 */
	if (buf[0] == '#') continue;
	p = buf;
	p = get_word(p, id);
	p = get_word(p, ugname);
	pp = p;
	p = get_word(p, perm);
	allow = 0;
	deny = 0;
	if (!strcmp(id, "user:"))
	  {
	     if (!fnmatch(u, ugname, 0))
	       {
		  if (!strcmp(perm, "allow:")) allow = 1;
		  else if (!strcmp(perm, "deny:")) deny = 1;
		  else
		    goto malformed;
	       }
	     else
	       continue;
	  }
	else if (!strcmp(id, "group:"))
	  {
	     if (!fnmatch(u, ugname, 0))
	       {
		  if (!strcmp(perm, "allow:")) allow = 1;
		  else if (!strcmp(perm, "deny:")) deny = 1;
		  else
		    goto malformed;
	       }
	     else
	       continue;
	  }
	else if (!strcmp(id, "action:"))
	  {
	     while ((*pp) && (isspace(*pp))) pp++;
	     s = evas_hash_find(actions, ugname);
	     if (s)
	       {
		  actions = evas_hash_del(actions, ugname, s);
		  free(s);
	       }
	     actions = evas_hash_add(actions, ugname, strdup(pp));
	     continue;
	  }
	else if (id[0] == 0)
	  continue;
	else
	  goto malformed;
	
	for (;;)
	  {
	     p = get_word(p, act);
	     if (act[0] == 0) break;
	     if (!fnmatch(act, a, 0))
	       {
		  if (allow) ok = 1;
		  else if (deny) ok = -1;
		  goto done;
	       }
	  }
	
	continue;
	malformed:
	printf("WARNING: %s:%i\n"
	       "LINE: '%s'\n"
	       "MALFORMED LINE. SKIPPED.\n",
	       file, line, buf);
     }
   done:
   fclose(f);
   return ok;
}

static char *
get_word(char *s, char *d)
{
   char *p1, *p2;
   
   p1 = s;
   p2 = d;
   while (*p1)
     {
	if (p2 == d)
	  {
	     if (isspace(*p1))
	       {
		  p1++;
		  continue;
	       }
	  }
	if (isspace(*p1)) break;
	*p2 = *p1;
	p1++;
	p2++;
     }
   *p2 = 0;
   return p1;
}

