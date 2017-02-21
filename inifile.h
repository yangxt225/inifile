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

#ifndef _INIFILE_H
#define _INIFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <stdarg.h>
#ifndef _MAC
#include <sys/types.h>
#endif

#define CFG_MAX_LINE_LENGTH 1024

/* configuration file entry */
typedef struct TCFGENTRY
  {
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;
  }
TCFGENTRY, *PCFGENTRY;

/* values for flags */
#define CFE_MUST_FREE_SECTION	0x8000
#define CFE_MUST_FREE_ID	0x4000
#define CFE_MUST_FREE_VALUE	0x2000
#define CFE_MUST_FREE_COMMENT	0x1000

/* configuration file */
typedef struct TCFGDATA
  {
    char *fileName;		/* Current file name */

    int dirty;			/* Did we make modifications? */

    char *image;		/* In-memory copy of the file */
    size_t size;		/* Size of this copy (excl. \0) */
    time_t mtime;		/* Modification time */

    unsigned int numEntries;
    unsigned int maxEntries;
    PCFGENTRY entries;

    /* Compatibility */
    unsigned int cursor;
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;

  }
TCONFIG, *PCONFIG;

#define CFG_VALID		0x8000
#define CFG_EOF			0x4000

#define CFG_ERROR		0x0000
#define CFG_SECTION		0x0001
#define CFG_DEFINE		0x0002
#define CFG_CONTINUE		0x0003

#define CFG_TYPEMASK		0x000F
#define CFG_TYPE(X)		((X) & CFG_TYPEMASK)
#define cfg_valid(X)	((X) != NULL && ((X)->flags & CFG_VALID))
#define cfg_eof(X)	((X)->flags & CFG_EOF)
#define cfg_section(X)	(CFG_TYPE((X)->flags) == CFG_SECTION)
#define cfg_define(X)	(CFG_TYPE((X)->flags) == CFG_DEFINE)
#define cfg_cont(X)	(CFG_TYPE((X)->flags) == CFG_CONTINUE)

/*
 * Name：   cfg_file_exist
 * Desc：   判断配置文件是否存在
 * param1： 配置文件名 
 * */
int cfg_file_exist (const char *filename);

/*
 * Name：    cfg_init 
 * Desc：    初始化配置文件
 * param1：  保存返回的 配置文件结构 
 * param2：  要初始化的 配置文件名
 * param3：  如果文件不存在，是否创建; 非0：创建
 * */
int cfg_init (PCONFIG * ppconf, const char *filename, int doCreate);

/*
 * Name：   cfg_done
 * Desc：   释放所有和配置文件相关的内存
 * param1： 配置文件结构
 * */
int cfg_done (PCONFIG pconfig);

/*
 * Name：   cfg_freeimage 
 * Desc：   释放配置结构中的image数据占用的内存
 * param1： 配置文件结构
 * */
int cfg_freeimage (PCONFIG pconfig);

/*
 * Name：   cfg_freeimage 
 * Desc：   (如果配置文件修改)刷新pconfig结构 
 * param1： 配置文件结构
 * */
int cfg_refresh (PCONFIG pconfig);

int cfg_storeentry (PCONFIG pconfig, char *section, char *id,
    char *value, char *comment, int dynamic);

int cfg_rewind (PCONFIG pconfig);
int cfg_nextentry (PCONFIG pconfig);
int cfg_find (PCONFIG pconfig, char *section, char *id);
int cfg_next_section (PCONFIG pconfig);

/*
 * Name：   cfg_write
 * Desc：   针对打开的配置结构，写入一个实体(一条配置记录)，只是写入到配置结构，还未存盘 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 实体值
 * */
int cfg_write (PCONFIG pconfig, char *section, char *id, char *value);

/*
 * Name：   cfg_commit
 * Desc：   将配置结构中的数据写入硬盘文件(存盘) 
 * param1： 配置文件结构
 * */
int cfg_commit (PCONFIG pconfig);

/*
 * Name：   cfg_getstring
 * Desc：   获取配置文件中的实体值 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 返回的实体值的存放位置
 * */
int cfg_getstring (PCONFIG pconfig, char *section, char *id, char *valptr);

/*
 * Name：   cfg_getlong
 * Desc：   获取long类型值 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 返回的实体值的存放位置
 * */
int cfg_getlong (PCONFIG pconfig, char *section, char *id, long *valptr);

/*
 * Name：   cfg_getint
 * Desc：   获取int类型值 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 返回的实体值的存放位置
 * */
int cfg_getint (PCONFIG pconfig, char *section, char *id, int *valptr);

/*
 * Name：   cfg_get_item
 * Desc：   按照param4指定的格式获取配置文件的实体值 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 指定要获取的实体的格式 
 * param5： 按照格式获取到的实体值的存放位置(参数列)
 * */
int cfg_get_item (PCONFIG pconfig, char *section, char *id, char * fmt, ...);

/*
 * Name：   cfg_write_item
 * Desc：   按照param4指定的格式写入配置文件的实体值 
 * param1： 配置文件结构
 * param2： section名
 * param3： 实体名
 * param4： 指定要写入的实体的格式 
 * param5： 按照格式将要写入的实体值(参数列)
 * */
int cfg_write_item(PCONFIG pconfig, char *section, char *id, char * fmt, ...);

int list_entries (PCONFIG pCfg, const char * lpszSection, char * lpszRetBuffer, int cbRetBuffer);
int list_sections (PCONFIG pCfg, char * lpszRetBuffer, int cbRetBuffer);

int GetPrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,
    char * lpszFilename);
int GetPrivateProfileInt (char * lpszSection, char * lpszEntry,
    int iDefault, char * lpszFilename);

/*
 * Name：   WritePrivateProfileString
 * Desc：   内部会先调用cfg_init完成初始化，再cfg_write写入数据，并自己负责存盘调用cfg_commit和内存释放cfg_done 
 * param1： section名
 * param2： 实体名
 * param3： 指定要写入的实体的格式 
 * param4： 配置文件名
 * */
int WritePrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszString, char * lpszFilename);

#ifdef  __cplusplus
}
#endif

#endif /* _INIFILE_H */
