// $Id$
//
//  PLP - An implementation of the PSION link protocol
//
//  Copyright (C) 1999  Philip Proudman
//  Modifications for plptools:
//    Copyright (C) 1999 Fritz Elfert <felfert@to.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  e-mail philip.proudman@btinternet.com

#include <sys/types.h>
#include <dirent.h>
#include <stream.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <iomanip.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "defs.h"
#include "ftp.h"
#include "rfsv32.h"
#include "bufferarray.h"
#include "bufferstore.h"

#if HAVE_LIBREADLINE
extern "C"  {
#include <stdio.h>
#include <readline/readline.h>
#if HAVE_LIBHISTORY
#include <readline/history.h>
#endif
}
#endif

void ftp::
resetUnixPwd()
{
	getcwd(localDir, 500);
	strcat(localDir, "/");
}

ftp::ftp()
{
	resetUnixPwd();
	strcpy(psionDir, DEFAULT_DRIVE);
	strcat(psionDir, DEFAULT_BASE_DIRECTORY);
}

ftp::~ftp()
{
}

void ftp::usage() {
	cerr << "Unknown command" << endl;
	cerr << "  pwd" << endl;
	cerr << "  ren <oldname> <newname>" << endl;
	cerr << "  touch <psionfile>" << endl;
	cerr << "  gtime <psionfile>" << endl;
	cerr << "  test <psionfile>" << endl;
	cerr << "  gattr <psionfile>" << endl;
	cerr << "  sattr [[-|+]rhsa] <psionfile>" << endl;
	cerr << "  devs" << endl;
	cerr << "  dir|ls" << endl;
	cerr << "  dircnt" << endl;
	cerr << "  cd <dir>" << endl;
	cerr << "  lcd <dir>" << endl;
	cerr << "  !<system command>" << endl;
	cerr << "  get <psionfile>" << endl;
	cerr << "  put <unixfile>" << endl;
	cerr << "  mget <shellpattern>" << endl;
	cerr << "  mput <shellpattern>" << endl;
	cerr << "  del|rm <psionfile>" << endl;
	cerr << "  mkdir <psiondir>" << endl;
	cerr << "  rmdir <psiondir>" << endl;
	cerr << "  prompt" << endl;
	cerr << "  bye" << endl;
}

static int Wildmat(const char *s, char *p);

static int
Star(const char *s, char *p)
{
	while (Wildmat(s, p) == 0)
		if (*++s == '\0')
			return 0;
        return 1;
}

static int
Wildmat(const char *s, char *p)
{
	register int last;
	register int matched;
	register int reverse;

	for (; *p; s++, p++)
		switch (*p) {
			case '\\':
				/*
				 * Literal match with following character,
				 * fall through.
				 */
				p++;
			default:
				if (*s != *p)
					return (0);
				continue;
			case '?':
				/* Match anything. */
				if (*s == '\0')
					return (0);
				continue;
			case '*':
				/* Trailing star matches everything. */
				return (*++p ? Star(s, p) : 1);
			case '[':
				/* [^....] means inverse character class. */
				if ((reverse = (p[1] == '^')))
					p++;
				for (last = 0, matched = 0; *++p && (*p != ']'); last = *p)
					/* This next line requires a good C compiler. */
					if (*p == '-' ? *s <= *++p && *s >= last : *s == *p)
						matched = 1;
				if (matched == reverse)
					return (0);
				continue;
		}
	return (*s == '\0');
}

int ftp::
session(rfsv32 & a, int xargc, char **xargv)
{
	int argc;
	char *argv[10];
	char f1[256];
	char f2[256];
	long res;
	bool prompt = true;
	bool once = false;

	if (xargc > 1) {
		once = true;
		argc = (xargc<10)?xargc:10;
		for (int i = 0; i < argc; i++)
			argv[i] = xargv[i+1];
	}
	do {
		if (!once)
			getCommand(argc, argv);

		if (!strcmp(argv[0], "prompt")) {
			prompt = !prompt;
			cout << "Prompting now " << (prompt?"on":"off") << endl;
			continue;
		}
		if (!strcmp(argv[0], "pwd")) {
			cout << "Local dir: \"" << localDir << "\"" << endl;
			cout << "Psion dir: \"" << psionDir << "\"" << endl;
			continue;
		}
		if (!strcmp(argv[0], "ren") && (argc == 3)) {
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			strcpy(f2, psionDir);
			strcat(f2, argv[2]);
			if ((res = a.rename(f1, f2)) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "touch") && (argc == 2)) {
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.fsetmtime(f1, time(0))) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "test") && (argc == 2)) {
			long attr, size, time;
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.fgeteattr(f1, &attr, &size, &time)) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "gattr") && (argc == 2)) {
			long attr;
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.fgetattr(f1, &attr)) != 0)
				errprint(res, a);
			else
				cout << hex << setw(4) << setfill('0') << attr << endl;
			continue;
		}
		if (!strcmp(argv[0], "gtime")) {
			long mtime;
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.fgetmtime(f1, &mtime)) != 0)
				errprint(res, a);
			else {
				char dateBuff[100];
				struct tm *t;
				t = localtime(&mtime);
				strftime(dateBuff, 100, "%c %Z", t);
				cout << dateBuff << endl;
			}
			continue;
		}
		if (!strcmp(argv[0], "sattr")) {
			long attr[2];
			int aidx = 0;
			char *p = argv[1];

			strcpy(f1, psionDir);
			strcat(f1, argv[2]);

			attr[0] = attr[1] = 0;
			while (*p) {
				switch (*p) {
					case '+':
						aidx = 0;
						break;
					case '-':
						aidx = 1;
						break;
					case 'r':
						attr[aidx] |= 0x01;
						attr[1 - aidx] &= ~0x01;
						break;
					case 'h':
						attr[aidx] |= 0x02;
						attr[1 - aidx] &= ~0x02;
						break;
					case 's':
						attr[aidx] |= 0x04;
						attr[1 - aidx] &= ~0x04;
						break;
					case 'a':
						attr[aidx] |= 0x20;
						attr[1 - aidx] &= ~0x20;
						break;
				}
				p++;
			}
			if ((res = a.fsetattr(f1, attr[0], attr[1])) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "dircnt")) {
			long cnt;
			if ((res = a.dircount(psionDir, &cnt)) != 0)
				errprint(res, a);
			else
				cout << cnt << " Entries" << endl;
			continue;
		}
		if (!strcmp(argv[0], "devs")) {
			long devbits;
			if ((res = a.devlist(&devbits)) == 0) {
				cout << "Drive Type Volname     Total     Free      UniqueID" << endl;
				for (int i = 0; i < 26; i++) {
					char *vname;
					long vtotal, vfree, vattr, vuniqueid;

					if ((devbits & 1) != 0) {
						vname = a.devinfo(i, &vfree, &vtotal, &vattr, &vuniqueid);
						if (vname != NULL) {
							cout << (char) ('A' + i) << "     " <<
							    hex << setw(4) << setfill('0') << vattr << " " <<
							    setw(12) << setfill(' ') << setiosflags(ios::left) << vname << resetiosflags(ios::left) << dec << setw(9) <<
							    vtotal << setw(9) << vfree << setw(10) << vuniqueid << endl;
							free(vname);
						}
					}
					devbits >>= 1;
				}
			} else
				errprint(res, a);
			continue;
		}
		if ((!strcmp(argv[0], "ls")) || (!strcmp(argv[0], "dir"))) {
			bufferArray files;
			if ((res = a.dir(psionDir, &files)) != 0)
				errprint(res, a);
			else
				while (!files.empty()) {
					bufferStore s;
					s = files.popBuffer();
					long date = s.getDWord(0);
					long size = s.getDWord(4);
					long attr = s.getDWord(8);
					char dateBuff[100];
                                	struct tm *t;
                                	t = localtime(&date);
                                	strftime(dateBuff, 100, "%c", t);
                                	cout << ((attr & rfsv32::PSI_ATTR_DIRECTORY) ? "d" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_RONLY) ? "-" : "w");
                                	cout << ((attr & rfsv32::PSI_ATTR_HIDDEN) ? "h" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_SYSTEM) ? "s" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_ARCHIVE) ? "a" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_VOLUME) ? "v" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_NORMAL) ? "n" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_TEMPORARY) ? "t" : "-");
                                	cout << ((attr & rfsv32::PSI_ATTR_COMPRESSED) ? "c" : "-");
                                	cout << " " << dec << setw(10) << setfill(' ') << size;
                                	cout << " " << dateBuff;
					cout << " " << s.getString(12) << endl;
				}
			continue;
		}
		if (!strcmp(argv[0], "lcd")) {
			if (argc == 1)
				resetUnixPwd();
			else {
				if (chdir(argv[1]) == 0)
					strcpy(localDir, argv[1]);
				else
					cerr << "No such directory" << endl
					    << "Keeping original directory \"" << localDir << "\"" << endl;
			}
			continue;
		}
		if (!strcmp(argv[0], "cd")) {
			if (argc == 1) {
				strcpy(psionDir, DEFAULT_DRIVE);
				strcat(psionDir, DEFAULT_BASE_DIRECTORY);
			} else {
				long tmp;
				if (!strcmp(argv[1], "..")) {
					strcpy(f1, psionDir);
					char *p = f1 + strlen(f1);
					if (p > f1)
						p--;
					*p = '\0';
					while ((p > f1) && (*p != '/') && (*p != '\\'))
						p--;
					*(++p) = '\0';
					if (strlen(f1) < 3) {
						strcpy(f1, psionDir);
						f1[3] = '\0';
					}
				} else {
					if ((argv[1][0] != '/') && (argv[1][0] != '\\') &&
					    (argv[1][1] != ':')) {
						strcpy(f1, psionDir);
						strcat(f1, argv[1]);
					} else
						strcpy(f1, argv[1]);
				}
				if ((f1[strlen(f1) -1] != '/') && (f1[strlen(f1) -1] != '\\'))
					strcat(f1,"\\");
				if ((res = a.dircount(f1, &tmp)) == 0)
					strcpy(psionDir, f1);
				else
					cerr << "Keeping original directory \"" << psionDir << "\"" << endl;
			}
			continue;
		}
		if ((!strcmp(argv[0], "get")) && (argc > 1)) {
			struct timeval stime;
			struct timeval etime;
			struct stat stbuf;

			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			strcpy(f2, localDir);
			if (argc == 2)
				strcat(f2, argv[1]);
			else
				strcat(f2, argv[2]);
			gettimeofday(&stime, 0L);
			if ((res = a.copyFromPsion(f1, f2)) != 0)
				errprint(res, a);
			else {
				gettimeofday(&etime, 0L);
				long dsec = etime.tv_sec - stime.tv_sec;
				long dhse = (etime.tv_usec / 10000) -
					(stime.tv_usec /10000);
				if (dhse < 0) {
					dsec--;
					dhse = 100 + dhse;
				}
				float dt = dhse;
				dt /= 100.0;
				dt += dsec;
				stat(f2, &stbuf);
				float cps = (float)(stbuf.st_size) / dt;
				cout << "Transfer complete, (" << stbuf.st_size
					<< " bytes in " << dsec << "."
					<< dhse << " secs = " << cps << " cps)\n";
			}
			continue;
		} else if ((!strcmp(argv[0], "mget")) && (argc == 2)) {
			char *pattern = argv[1];
			bufferArray files;
			if ((res = a.dir(psionDir, &files)) != 0) {
				errprint(res, a);
				continue;
			}
			while (!files.empty()) {
				bufferStore s;
				s = files.popBuffer();
				char temp[100];
				long attr = s.getDWord(8);

				if (attr & 0x10)
					continue;
				if (!Wildmat(s.getString(12), pattern))
					continue;
				do {
					cout << "Get \"" << s.getString(12) << "\" (y,n): ";
					if (prompt) {
						cout.flush();
						cin.getline(temp, 100);
					} else {
						temp[0] = 'y';
						temp[1] = 0;
						cout << "y ";
						cout.flush();
					}
				} while (temp[1] != 0 || (temp[0] != 'y' && temp[0] != 'n'));
				if (temp[0] != 'n') {
					strcpy(f1, psionDir);
					strcat(f1, s.getString());
					strcpy(f2, localDir);
					strcat(f2, s.getString());
					if (temp[0] == 'l') {
						for (char *p = f2; *p; p++)
							*p = tolower(*p);
					}
					if ((res = a.copyFromPsion(f1, f2)) != 0) {
						errprint(res, a);
						break;
					} else
						cout << "Transfer complete\n";
				}
			}
			continue;
		}
		if (!strcmp(argv[0], "put")) {
			struct timeval stime;
			struct timeval etime;
			struct stat stbuf;

			strcpy(f1, localDir);
			strcat(f1, argv[1]);
			strcpy(f2, psionDir);
			if (argc == 2)
				strcat(f2, argv[1]);
			else
				strcat(f2, argv[2]);
			gettimeofday(&stime, 0L);
			if ((res = a.copyToPsion(f1, f2)) != 0)
				errprint(res, a);
			else {
				gettimeofday(&etime, 0L);
				long dsec = etime.tv_sec - stime.tv_sec;
				long dhse = (etime.tv_usec / 10000) -
					(stime.tv_usec /10000);
				if (dhse < 0) {
					dsec--;
					dhse = 100 + dhse;
				}
				float dt = dhse;
				dt /= 100.0;
				dt += dsec;
				stat(f1, &stbuf);
				float cps = (float)(stbuf.st_size) / dt;
				cout << "Transfer complete, (" << stbuf.st_size
					<< " bytes in " << dsec << "."
					<< dhse << " secs = " << cps << " cps)\n";
			}
			continue;
		}
		if ((!strcmp(argv[0], "mput")) && (argc == 2)) {
			char *pattern = argv[1];
			DIR *d = opendir(localDir);
			if (d) {
				struct dirent *de;
				do {
					de = readdir(d);
					if (de) {
						char temp[100];
						struct stat st;

						if (!Wildmat(de->d_name, pattern))
							continue;
						strcpy(f1, localDir);
						strcat(f1, de->d_name);
						if (stat(f1, &st) != 0)
							continue;
						if (!S_ISREG(st.st_mode))
							continue;
						do {
							cout << "Put \"" << de->d_name << "\" y,n: ";
							if (prompt) {
								cout.flush();
								cin.getline(temp, 100);
							} else {
								temp[0] = 'y';
								temp[1] = 0;
								cout << "y ";
								cout.flush();
							}
						} while (temp[1] != 0 || (temp[0] != 'y' && temp[0] != 'n'));
						if (temp[0] == 'y') {
							strcpy(f2, psionDir);
							strcat(f2, de->d_name);
							if ((res = a.copyToPsion(f1, f2)) != 0) {
								errprint(res, a);
								break;
							} else
								cout << "Transfer complete\n";
						}
					}
				} while (de);
				closedir(d);
			} else
				cerr << "Error in directory name \"" << localDir << "\"\n";
			continue;
		}
		if (!strcmp(argv[0], "del") ||
		    !strcmp(argv[0], "rm")) {
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.remove(f1)) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "mkdir")) {
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.mkdir(f1)) != 0)
				errprint(res, a);
			continue;
		}
		if (!strcmp(argv[0], "rmdir")) {
			strcpy(f1, psionDir);
			strcat(f1, argv[1]);
			if ((res = a.rmdir(f1)) != 0)
				errprint(res, a);
			continue;
		}
		if (argv[0][0] == '!') {
			char cmd[1024];
			strcpy(cmd, &argv[0][1]);
			for (int i=1; i<argc; i++) {
				strcat(cmd, " ");
				strcat(cmd, argv[i]);
			}
			system(cmd);
			continue;
		}
		if (strcmp(argv[0], "bye") && strcmp(argv[0], "quit"))
			usage();
	} while (strcmp(argv[0], "bye") && strcmp(argv[0], "quit") && !once);
	return a.getStatus();
}

void ftp::
errprint(long errcode, rfsv32 & a) {
	cerr << "Error: " << a.opErr(errcode) << endl;
}

int ftp::
convertName(const char *orig, char *retVal)
{
	int len = strlen(orig);
	char *temp = new char[len + 1];

	for (int i = 0; i <= len; i++) {
		if (orig[i] == '/')
			temp[i] = '\\';
		else
			temp[i] = orig[i];
	}
	if (len == 0 || temp[1] != ':') {
		// We can automatically supply a drive letter
		strcpy(retVal, DEFAULT_DRIVE);

		if (len == 0 || temp[0] != '\\') {
			strcat(retVal, DEFAULT_BASE_DIRECTORY);
		} else {
			retVal[strlen(retVal) - 1] = 0;
		}

		strcat(retVal, temp);
	} else {
		strcpy(retVal, temp);
	}
	delete[]temp;
	return 0;
}

void ftp::
getCommand(int &argc, char **argv)
{
	int ws;
	char *bp;

	static char buf[1024];

	buf[0] = 0; argc = 0;
	while (!strlen(buf)) {
		bp = readline("> ");
		if (!bp) {
			strcpy(buf, "bye");
			cout << buf << endl;
		} else {
			strcpy(buf, bp);
			add_history(buf);
			free(bp);
		}
	}
	ws = 1;
	for (char *p = buf; *p; p++)
		if ((*p == ' ') || (*p == '\t')) {
			ws = 1;
			*p = 0;
		} else {
			if (ws)
				argv[argc++] = p;
			ws = 0;
		}
}

  // Unix utilities
bool ftp::
unixDirExists(const char *dir)
{
	return false;
}

void ftp::
getUnixDir(bufferArray & files)
{
}

void ftp::
cd(const char *source, const char *cdto, char *dest)
{
	if (cdto[0] == '/' || cdto[0] == '\\' || cdto[1] == ':') {
		strcpy(dest, cdto);
		char cc = dest[strlen(dest) - 1];
		if (cc != '/' && cc != '\\')
			strcat(dest, "/");
	} else {
		char start[200];
		strcpy(start, source);

		while (*cdto) {
			char bit[200];
			int j;
			for (j = 0; cdto[j] && cdto[j] != '/' && cdto[j] != '\\'; j++)
				bit[j] = cdto[j];
			bit[j] = 0;
			cdto += j;
			if (*cdto)
				cdto++;

			if (!strcmp(bit, "..")) {
				strcpy(dest, start);
				int i;
				for (i = strlen(dest) - 2; i >= 0; i--) {
					if (dest[i] == '/' || dest[i] == '\\') {
						dest[i + 1] = 0;
						break;
					}
				}
			} else {
				strcpy(dest, start);
				strcat(dest, bit);
				strcat(dest, "/");
			}
			strcpy(start, dest);
		}
	}
}
