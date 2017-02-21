/*

Configuration File Management
Read/Write .ini files

version 0.2 2001/10/30 13:20:00
Author: 陈志强 (czhiqiang@163.net)，modified from iodbc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef _MAC
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#endif
#include <unistd.h>
#include <ctype.h>

#include "inifile.h"


static PCFGENTRY _cfg_poolalloc (PCONFIG p, unsigned int count);
static int _cfg_parse (PCONFIG pconfig);

/*** READ MODULE ****/

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef _MAC
static int
strcasecmp (const char *s1, const char *s2)
{
  int cmp;

  while (*s1)
    {
      if ((cmp = toupper (*s1) - toupper (*s2)) != 0)
	return cmp;
      s1++;
      s2++;
    }
  return (*s2) ? -1 : 0;
}
#endif

char *
remove_quotes(const char *szString)
{
  char *szWork, *szPtr;

  while (*szString == '\'' || *szString == '\"')
    szString += 1;

  if (!*szString)
    return NULL;
  szWork = strdup (szString);
  szPtr = strchr (szWork, '\'');
  if (szPtr)
    *szPtr = 0;
  szPtr = strchr (szWork, '\"');
  if (szPtr)
    *szPtr = 0;

  return szWork;
}

int
cfg_file_exist (const char *filename)
{
  return access(filename, 0);
}


/*
 *  Initialize a configuration
 */
int
cfg_init (PCONFIG *ppconf, const char *filename, int doCreate)
{
  PCONFIG pconfig;

  *ppconf = NULL;

  if (!filename)
    return -1;

  if ((pconfig = (PCONFIG) calloc (1, sizeof (TCONFIG))) == NULL)
    return -1;

  //strdup:字符串复制，strdup把动态分配内存的事实隐藏在自己内部
  //释放strdup内部动态分配的内存需要由调用者去做.
  pconfig->fileName = strdup (filename);
  if (pconfig->fileName == NULL)
    {
      cfg_done (pconfig);
      return -1;
    }

  /* If the file does not exist, try to create it */
  if (doCreate && access (pconfig->fileName, 0) == -1)
    {
      int fd;

      fd = creat (filename, 0644);
      if (fd)
	close (fd);
    }

  if (cfg_refresh (pconfig) == -1)
    {
      cfg_done (pconfig);
      return -1;
    }
  *ppconf = pconfig;

  return 0;
}


/*
 *  Free all data associated with a configuration
 */
int
cfg_done (PCONFIG pconfig)
{
  if (pconfig)
    {
      cfg_freeimage (pconfig);
      if (pconfig->fileName)
	free (pconfig->fileName);
      free (pconfig);
    }

  return 0;
}


/*
 *  Free the content specific data of a configuration
 */
int
cfg_freeimage (PCONFIG pconfig)
{
  char *saveName;
  PCFGENTRY e;
  unsigned int i;

  if (pconfig->image)
    free (pconfig->image);
  if (pconfig->entries)
    {
      e = pconfig->entries;
      for (i = 0; i < pconfig->numEntries; i++, e++)
	{
	  if (e->flags & CFE_MUST_FREE_SECTION)
	    free (e->section);
	  if (e->flags & CFE_MUST_FREE_ID)
	    free (e->id);
	  if (e->flags & CFE_MUST_FREE_VALUE)
	    free (e->value);
	  if (e->flags & CFE_MUST_FREE_COMMENT)
	    free (e->comment);
	}
      free (pconfig->entries);
    }

  saveName = pconfig->fileName;
  memset (pconfig, 0, sizeof (TCONFIG));
  pconfig->fileName = saveName;

  return 0;
}


/*
 *  This procedure reads an copy of the file into memory
 *  caching the content based on stat
 */
int
cfg_refresh (PCONFIG pconfig)
{
  //sb : stat buf
  struct stat sb;
  char *mem;
  int fd;

  //stat()用来将参数fileName 所指的文件状态, 复制到参数sb 所指的结构中
  if (pconfig == NULL || stat (pconfig->fileName, &sb) == -1)
    return -1;

  /*
   *  If our image is dirty, ignore all local changes
   *  and force a reread of the image, thus ignoring all mods
   */
  if (pconfig->dirty)
    cfg_freeimage (pconfig);

  /*
   *  Check to see if our incore image is still valid
   */
  if (pconfig->image && sb.st_size == pconfig->size
      && sb.st_mtime == pconfig->mtime)
    return 0;

  /*
   *  Now read the full image
   */
  if ((fd = open (pconfig->fileName, O_RDONLY | O_BINARY)) == -1)
    return -1;

  mem = (char *) malloc (sb.st_size + 1);
  //read() : 从打开的设备或文件(fd)中读取数据到mem.
  if (mem == NULL || read (fd, mem, sb.st_size) != sb.st_size)
    {
      free (mem);
      close (fd);
      return -1;
    }
  mem[sb.st_size] = 0;

  close (fd);

  /*
   *  Store the new copy
   */
  cfg_freeimage (pconfig);
  pconfig->image = mem;
  pconfig->size = sb.st_size;
  pconfig->mtime = sb.st_mtime;

  if (_cfg_parse (pconfig) == -1)
    {
      cfg_freeimage (pconfig);
      return -1;
    }

  return 1;
}


#define iseolchar(C) (strchr ("\n\r\x1a", C) != NULL)
#define iswhite(C) (strchr ("\f\t ", C) != NULL)


static char *
_cfg_skipwhite (char *s)
{
  while (*s && iswhite (*s))
    s++;
  return s;
}


static int
_cfg_getline (char **pCp, char **pLinePtr)
{
  char *start;
  char *cp = *pCp;

  while (*cp && iseolchar (*cp))
    cp++;
  start = cp;
  if (pLinePtr)
    *pLinePtr = cp;

  while (*cp && !iseolchar (*cp))
    cp++;
  if (*cp)
    {
      *cp++ = 0;
      *pCp = cp;

      while (--cp >= start && iswhite (*cp));
      cp[1] = 0;
    }
  else
    *pCp = cp;

  return *start ? 1 : 0;
}


static char *
rtrim (char *str)
{
  char *endPtr;

  if (str == NULL || *str == '\0')
    return NULL;

  for (endPtr = &str[strlen (str) - 1]; endPtr >= str && isspace (*endPtr);
      endPtr--);
  endPtr[1] = 0;
  return endPtr >= str ? endPtr : NULL;
}


/*
 *  Parse the in-memory copy of the configuration data
 */
static int
_cfg_parse (PCONFIG pconfig)
{
  int isContinue, inString;
  char *imgPtr;
  char *endPtr;
  char *lp;
  char *section;
  char *id;
  char *value;
  char *comment;

  if (cfg_valid (pconfig))
    return 0;

  endPtr = pconfig->image + pconfig->size;
  for (imgPtr = pconfig->image; imgPtr < endPtr;)
    {
      if (!_cfg_getline (&imgPtr, &lp))
	continue;

      section = id = value = comment = NULL;

      /*
         *  Skip leading spaces
       */
      if (iswhite (*lp))
	{
	  lp = _cfg_skipwhite (lp);
	  isContinue = 1;
	}
      else
	isContinue = 0;

      /*
       *  Parse Section
       */
      if (*lp == '[')
	{
	  section = _cfg_skipwhite (lp + 1);
	  if ((lp = strchr (section, ']')) == NULL)
	    continue;
	  *lp++ = 0;
	  if (rtrim (section) == NULL)
	    {
	      section = NULL;
	      continue;
	    }
	  lp = _cfg_skipwhite (lp);
	}
      else if (*lp != ';')
	{
	  /* Try to parse
	   *   1. Key = Value
	   *   2. Value (iff isContinue)
	   */
	  if (!isContinue)
	    {
	      /* Parse `<Key> = ..' */
	      id = lp;
	      if ((lp = strchr (id, '=')) == NULL)
		continue;
	      *lp++ = 0;
	      rtrim (id);
	      lp = _cfg_skipwhite (lp);
	    }

	  /* Parse value */
	  inString = 0;
	  value = lp;
	  while (*lp)
	    {
	      if (inString)
		{
		  if (*lp == inString)
		    inString = 0;
		}
	      else if (*lp == '"' || *lp == '\'')
		inString = *lp;
	      else if (*lp == ';' && iswhite (lp[-1]))
		{
		  *lp = 0;
		  comment = lp + 1;
		  rtrim (value);
		  break;
		}
	      lp++;
	    }
	}

      /*
       *  Parse Comment
       */
      if (*lp == ';')
	comment = lp + 1;

      if (cfg_storeentry (pconfig, section, id, value, comment,
	      0) == -1)
	{
	  pconfig->dirty = 1;
	  return -1;
	}
    }

  pconfig->flags |= CFG_VALID;

  return 0;
}


int
cfg_storeentry (
    PCONFIG pconfig,
    char *section,
    char *id,
    char *value,
    char *comment,
    int dynamic)
{
  PCFGENTRY data;

  if ((data = _cfg_poolalloc (pconfig, 1)) == NULL)
    return -1;

  data->flags = 0;
  if (dynamic)
    {
      if (section)
	section = strdup (section);
      if (id)
	id = strdup (id);
      if (value)
	value = strdup (value);
      if (comment)
	comment = strdup (value);

      if (section)
	data->flags |= CFE_MUST_FREE_SECTION;
      if (id)
	data->flags |= CFE_MUST_FREE_ID;
      if (value)
	data->flags |= CFE_MUST_FREE_VALUE;
      if (comment)
	data->flags |= CFE_MUST_FREE_COMMENT;
    }

  data->section = section;
  data->id = id;
  data->value = value;
  data->comment = comment;

  return 0;
}


static PCFGENTRY
_cfg_poolalloc (PCONFIG p, unsigned int count)
{
  PCFGENTRY newBase;
  unsigned int newMax;

  if (p->numEntries + count > p->maxEntries)
    {
      newMax =
	  p->maxEntries ? count + p->maxEntries + p->maxEntries / 2 : count +
	  4096 / sizeof (TCFGENTRY);
      newBase = (PCFGENTRY) malloc (newMax * sizeof (TCFGENTRY));
      if (newBase == NULL)
	return NULL;
      if (p->entries)
	{
	  memcpy (newBase, p->entries, p->numEntries * sizeof (TCFGENTRY));
	  free (p->entries);
	}
      p->entries = newBase;
      p->maxEntries = newMax;
    }

  newBase = &p->entries[p->numEntries];
  p->numEntries += count;

  return newBase;
}


/*** COMPATIBILITY LAYER ***/


int
cfg_rewind (PCONFIG pconfig)
{
  if (!cfg_valid (pconfig))
    return -1;

  pconfig->flags = CFG_VALID;
  pconfig->cursor = 0;

  return 0;
}


/*
 *  returns:
 *	 0 success
 *	-1 no next entry
 *
 *	section	id	value	flags		meaning
 *	!0	0	!0	SECTION		[value]
 *	!0	!0	!0	DEFINE		id = value|id="value"|id='value'
 *	!0	0	!0	0		value
 *	0	0	0	EOF		end of file encountered
 */
int
cfg_nextentry (PCONFIG pconfig)
{
  PCFGENTRY e;

  if (!cfg_valid (pconfig) || cfg_eof (pconfig))
    return -1;

  pconfig->flags &= ~(CFG_TYPEMASK);
  pconfig->id = pconfig->value = NULL;

  while (1)
    {
      if (pconfig->cursor >= pconfig->numEntries)
	{
	  pconfig->flags |= CFG_EOF;
	  return -1;
	}
      e = &pconfig->entries[pconfig->cursor++];

      if (e->section)
	{
	  pconfig->section = e->section;
	  pconfig->flags |= CFG_SECTION;
	  return 0;
	}
      if (e->value)
	{
	  pconfig->value = e->value;
	  if (e->id)
	    {
	      pconfig->id = e->id;
	      pconfig->flags |= CFG_DEFINE;
	    }
	  else
	    pconfig->flags |= CFG_CONTINUE;
	  return 0;
	}
    }
}


int
cfg_find (PCONFIG pconfig, char *section, char *id)
{
  int atsection;

  if (!cfg_valid (pconfig) || cfg_rewind (pconfig))
    return -1;

  atsection = 0;
  while (cfg_nextentry (pconfig) == 0)
    {
      if (atsection)
	{
	  if (cfg_section (pconfig))
	    return -1;
	  else if (cfg_define (pconfig))
	    {
	      char *szId = remove_quotes (pconfig->id);
	      int bSame;
	      if (szId)
		{
		  bSame = !strcasecmp (szId, id);
		  free (szId);
		  if (bSame)
		    return 0;
		}
	    }
	}
      else if (cfg_section (pconfig)
	  && !strcasecmp (pconfig->section, section))
	{
	  if (id == NULL)
	    return 0;
	  atsection = 1;
	}
    }
  return -1;
}


/*** WRITE MODULE ****/


/*
 *  Change the configuration
 *
 *  section id    value		action
 *  --------------------------------------------------------------------------
 *   value  value value		update '<entry>=<string>' in section <section>
 *   value  value NULL		delete '<entry>' from section <section>
 *   value  NULL  NULL		delete section <section>
 */
int
cfg_write (
    PCONFIG pconfig,
    char *section,
    char *id,
    char *value)
{
  PCFGENTRY e, e2, eSect;
  int idx;
  int i;

  if (!cfg_valid (pconfig) || section == NULL)
    return -1;

  /* find the section */
  e = pconfig->entries;
  i = pconfig->numEntries;
  eSect = 0;
  while (i--)
    {
      if (e->section && !strcasecmp (e->section, section))
	{
	  eSect = e;
	  break;
	}
      e++;
    }

  /* did we find the section? */
  if (!eSect)
    {
      /* check for delete operation on a nonexisting section */
      if (!id || !value)
	return 0;

      /* add section first */
      if (cfg_storeentry (pconfig, section, NULL, NULL, NULL,
	      1) == -1
	  || cfg_storeentry (pconfig, NULL, id, value, NULL,
	      1) == -1)
	return -1;

      pconfig->dirty = 1;
      return 0;
    }

  /* ok - we have found the section - let's see what we need to do */

  if (id)
    {
      if (value)
	{
	  /* add / update a key */
	  while (i--)
	    {
	      e++;
	      /* break on next section */
	      if (e->section)
		{
		  /* insert new entry before e */
		  idx = e - pconfig->entries;
		  if (_cfg_poolalloc (pconfig, 1) == NULL)
		    return -1;
		  memmove (e + 1, e,
		      (pconfig->numEntries - idx) * sizeof (TCFGENTRY));
		  e->section = NULL;
		  e->id = strdup (id);
		  e->value = strdup (value);
		  e->comment = NULL;
		  if (e->id == NULL || e->value == NULL)
		    return -1;
		  e->flags |= CFE_MUST_FREE_ID | CFE_MUST_FREE_VALUE;
		  pconfig->dirty = 1;
		  return 0;
		}

	      if (e->id && !strcasecmp (e->id, id))
		{
		  /* found key - do update */
		  if (e->value && (e->flags & CFE_MUST_FREE_VALUE))
		    {
		      e->flags &= ~CFE_MUST_FREE_VALUE;
		      free (e->value);
		    }
		  pconfig->dirty = 1;
		  if ((e->value = strdup (value)) == NULL)
		    return -1;
		  e->flags |= CFE_MUST_FREE_VALUE;
		  return 0;
		}
	    }

	  /* last section in file - add new entry */
	  if (cfg_storeentry (pconfig, NULL, id, value, NULL,
		  1) == -1)
	    return -1;
	  pconfig->dirty = 1;
	  return 0;
	}
      else
	{
	  /* delete a key */
	  while (i--)
	    {
	      e++;
	      /* break on next section */
	      if (e->section)
		return 0;	/* not found */

	      if (e->id && !strcasecmp (e->id, id))
		{
		  /* found key - do delete */
		  eSect = e;
		  e++;
		  goto doDelete;
		}
	    }
	  /* key not found - that' ok */
	  return 0;
	}
    }
  else
    {
      /* delete entire section */

      /* find e : next section */
      while (i--)
	{
	  e++;
	  /* break on next section */
	  if (e->section)
	    break;
	}
      if (i < 0)
	e++;

      /* move up e while comment */
      e2 = e - 1;
      while (e2->comment && !e2->section && !e2->id && !e2->value
	  && (iswhite (e2->comment[0]) || e2->comment[0] == ';'))
	e2--;
      e = e2 + 1;

    doDelete:
      /* move up eSect while comment */
      e2 = eSect - 1;
      while (e2->comment && !e2->section && !e2->id && !e2->value
	  && (iswhite (e2->comment[0]) || e2->comment[0] == ';'))
	e2--;
      eSect = e2 + 1;

      /* delete everything between eSect .. e */
      for (e2 = eSect; e2 < e; e2++)
	{
	  if (e2->flags & CFE_MUST_FREE_SECTION)
	    free (e2->section);
	  if (e2->flags & CFE_MUST_FREE_ID)
	    free (e2->id);
	  if (e2->flags & CFE_MUST_FREE_VALUE)
	    free (e2->value);
	  if (e2->flags & CFE_MUST_FREE_COMMENT)
	    free (e2->comment);
	}
      idx = e - pconfig->entries;
      memmove (eSect, e, (pconfig->numEntries - idx) * sizeof (TCFGENTRY));
      pconfig->numEntries -= e - eSect;
      pconfig->dirty = 1;
    }

  return 0;
}


/*
 *  Write a formatted copy of the configuration to a file
 *
 *  This assumes that the inifile has already been parsed
 */
static void
_cfg_outputformatted (PCONFIG pconfig, FILE *fd)
{
  PCFGENTRY e = pconfig->entries;
  int i = pconfig->numEntries;
  int m = 0;
  int j, l;
  int skip = 0;

  while (i--)
    {
      if (e->section)
	{
	  /* Add extra line before section, unless comment block found */
	  if (skip)
	    fprintf (fd, "\n");
	  fprintf (fd, "[%s]", e->section);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);

	  /* Calculate m, which is the length of the longest key */
	  m = 0;
	  for (j = 1; j <= i; j++)
	    {
	      if (e[j].section)
		break;
	      if (e[j].id && (l = strlen (e[j].id)) > m)
		m = l;
	    }

	  /* Add an extra lf next time around */
	  skip = 1;
	}
      /*
       *  Key = value
       */
      else if (e->id && e->value)
	{
	  if (m)
	    fprintf (fd, "%-*.*s = %s", m, m, e->id, e->value);
	  else
	    fprintf (fd, "%s = %s", e->id, e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Value only (continuation)
       */
      else if (e->value)
	{
	  fprintf (fd, "  %s", e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Comment only - check if we need an extra lf
       *
       *  1. Comment before section gets an extra blank line before
       *     the comment starts.
       *
       *          previousEntry = value
       *          <<< INSERT BLANK LINE HERE >>>
       *          ; Comment Block
       *          ; Sticks to section below
       *          [new section]
       *
       *  2. Exception on 1. for commented out definitions:
       *     (Immediate nonwhitespace after ;)
       *          [some section]
       *          v1 = 1
       *          ;v2 = 2   << NO EXTRA LINE >>
       *          v3 = 3
       *
       *  3. Exception on 2. for ;; which certainly is a section comment
       *          [some section]
       *          definitions
       *          <<< INSERT BLANK LINE HERE >>>
       *          ;; block comment
       *          [new section]
       */
      else if (e->comment)
	{
	  if (skip && (iswhite (e->comment[0]) || e->comment[0] == ';'))
	    {
	      for (j = 1; j <= i; j++)
		{
		  if (e[j].section)
		    {
		      fprintf (fd, "\n");
		      skip = 0;
		      break;
		    }
		  if (e[j].id || e[j].value)
		    break;
		}
	    }
	  fprintf (fd, ";%s", e->comment);
	}
      fprintf (fd, "\n");
      e++;
    }
}


/*
 *  Write the changed file back
 */
int
cfg_commit (PCONFIG pconfig)
{
  FILE *fp;

  if (!cfg_valid (pconfig))
    return -1;

  if (pconfig->dirty)
    {
      if ((fp = fopen (pconfig->fileName, "w")) == NULL)
	return -1;

      _cfg_outputformatted (pconfig, fp);

      fclose (fp);

      pconfig->dirty = 0;
    }

  return 0;
}


int
cfg_next_section(PCONFIG pconfig)
{
  PCFGENTRY e;

  do
    if (0 != cfg_nextentry (pconfig))
      return -1;
  while (!cfg_section (pconfig));

  return 0;
}


int
list_sections (PCONFIG pCfg, char * lpszRetBuffer, int cbRetBuffer)
{
  int curr = 0, sect_len = 0;
  lpszRetBuffer[0] = 0;

  if (0 == cfg_rewind (pCfg))
    {
      while (curr < cbRetBuffer && 0 == cfg_next_section (pCfg)
	  && pCfg->section)
	{
	  sect_len = strlen (pCfg->section) + 1;
	  sect_len =
	      sect_len > cbRetBuffer - curr ? cbRetBuffer - curr : sect_len;

	  memmove (lpszRetBuffer + curr, pCfg->section, sect_len);

	  curr += sect_len;
	}
      if (curr < cbRetBuffer)
	lpszRetBuffer[curr] = 0;
      return curr;
    }
  return 0;
}


int
list_entries (PCONFIG pCfg, const char * lpszSection, char * lpszRetBuffer, int cbRetBuffer)
{
  int curr = 0, sect_len = 0;
  lpszRetBuffer[0] = 0;

  if (0 == cfg_rewind (pCfg))
    {
      while (curr < cbRetBuffer && 0 == cfg_nextentry (pCfg))
	{
	  if (cfg_define (pCfg)
	      && !strcmp (pCfg->section, lpszSection) && pCfg->id)
	    {
	      sect_len = strlen (pCfg->id) + 1;
	      sect_len =
		  sect_len >
		  cbRetBuffer - curr ? cbRetBuffer - curr : sect_len;

	      memmove (lpszRetBuffer + curr, pCfg->id, sect_len);

	      curr += sect_len;
	    }
	}
      if (curr < cbRetBuffer)
	lpszRetBuffer[curr] = 0;
      return curr;
    }
  return 0;
}

int cfg_getstring (PCONFIG pconfig, char *section, char *id, char *valptr)
{
	if(!pconfig || !section || !id || !valptr) return -1;
	if(cfg_find(pconfig,section,id) == -1) return -1;
	strcpy(valptr,pconfig->value);
	return 0;
}

int cfg_getlong (PCONFIG pconfig, char *section, char *id, long *valptr)
{
	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) == -1) return -1;
	*valptr = atoi(pconfig->value);
	return 0;
}

int cfg_getint (PCONFIG pconfig, char *section, char *id, int *valptr)
{
	return cfg_getlong(pconfig,section,id,(long *)valptr);
}

int GetPrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,
    char * lpszFilename)
{
  char *defval = (char *) lpszDefault, *value = NULL;
  int len = 0;
  PCONFIG pCfg;

  /*
   *  Sorry for this one -- Windows cannot handle a default value of
   *  "" in GetPrivateProfileString, so it is passed as " " instead.
   */
  if (!lpszDefault || (lpszDefault[0] == '\0') || (lpszDefault[0] == ' ' && lpszDefault[1] == '\0')) value = "";
  else value = lpszDefault;
  
  strncpy (lpszRetBuffer, value, cbRetBuffer - 1);

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 0))
    {
      goto fail;
    }

  /*
   *  Check whether someone else has modified the .ini file
   */
  cfg_refresh (pCfg);

  if (!cfg_find (pCfg, (char *) lpszSection, (char *) lpszEntry)){
    value = pCfg->value;
    strncpy (lpszRetBuffer, value, cbRetBuffer - 1);
  }

  cfg_done (pCfg);

fail:
  len = strlen (lpszRetBuffer);
  return len;
}

int GetPrivateProfileInt (char * lpszSection, char * lpszEntry,
    int iDefault, char * lpszFilename)
{
  int ret=iDefault;
  PCONFIG pCfg;

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 0))
    {
      return iDefault;
    }

  if (!cfg_find (pCfg, (char *) lpszSection, (char *) lpszEntry)){
    ret = atoi(pCfg->value);
  }

  cfg_done (pCfg);

  return ret;
}

int WritePrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszString, char * lpszFilename)
{
  int ret;
  PCONFIG pCfg;

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 1))
    {
      return -1;
    }
  ret = cfg_write (pCfg, lpszSection, lpszEntry, lpszString);
  cfg_commit (pCfg);
  cfg_done (pCfg);
  return ret;
}

int cfg_get_item (PCONFIG pconfig, char *section, char *id, char * fmt, ...)
{
	int ret;
	va_list ap;
	
	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) == -1) return -1;
	va_start(ap, fmt);
	ret = vsscanf(pconfig->value, fmt, ap );
	va_end(ap);
	if(ret > 0) return 0;
	else return -1;
}

int cfg_write_item(PCONFIG pconfig, char *section, char *id, char * fmt, ...)
{
	int ret;
	char buf[CFG_MAX_LINE_LENGTH];
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(buf, CFG_MAX_LINE_LENGTH, fmt, ap);
	va_end(ap);
	if(ret < 0) return -1;
	return cfg_write(pconfig,section,id,buf);
}
