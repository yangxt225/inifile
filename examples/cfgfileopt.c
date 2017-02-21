#include "cfgfileopt.h"
#include "stdio.h"

// 使用inifile函数库操作配置文件增删改
int main()
{
    int rc;
    zlog_category_t *z_all;
    //配置文件结构
    PCONFIG pCfg;

    /***添加日志模块，使用zlog动态日志库****/
    rc = zlog_init("zlog.conf");
    if (rc) {
        printf("init failed\n");
        return -1;
    }
    z_all = zlog_get_category("sys_all");
    if (!z_all) {
        printf("get category fail\n");
        zlog_fini();
        return -2;
    }
    zlog_info(z_all, "zlog init completed.");

    /*************** write *************/
    cfg_init (&pCfg, "config.ini", 1);	/* 若config.ini不存在，创建config.ini */
    zlog_info(z_all, "create config.ini file completed.");

    cfg_write (pCfg, "section1", "entry1", "abc");	/* 写section1:entry1 */
    zlog_info(z_all, "write section1:entry1 completed.");

    cfg_write_item (pCfg, "section1", "entry2", "%d 0X%0X", 123,0X12AC);  /* 用另外的方法写section1:entry2 */
    zlog_info(z_all, "write section1:entry2 completed.");

    cfg_write_item (pCfg, "section1", "entry3", "%d", 2017);
    zlog_info(z_all, "write section1:entry3 completed.");

    cfg_commit(pCfg);	/* 存盘 */
    cfg_done(pCfg);

    WritePrivateProfileString("section1", "entry4", "256 300","config.ini");
    zlog_info(z_all, "section1:entry3 completed.");
    WritePrivateProfileString("section2", "entry1", "test","config.ini");
    zlog_info(z_all, "section2:entry1 completed.");

    
    /*************** read *************/
    /* 读取所有配置并打印 */
    char buf[128];
    int value, value1, tmp;
    cfg_init (&pCfg, "config.ini", 0);
    
    cfg_getstring(pCfg, "section1", "entry1", buf);
    printf("section1:entry1 -- %s\n", buf);
    zlog_info(z_all, "read-section1:entry1 -- %s.", buf);

    cfg_getstring(pCfg, "section1", "entry2", buf);
    printf("section1:entry2 -- %s\n", buf);
    zlog_info(z_all, "read-section1:entry2 -- %s.", buf);

    //两种获取整数值的方法，并更新值
    cfg_getint(pCfg, "section1", "entry3", &value);
    printf("section1:entry3 -- %d\n", value);
    zlog_info(z_all, "read-section1:entry3 -- %d", value);

    cfg_get_item(pCfg, "section1", "entry3", "%d", &tmp);
    printf("cfg_get_item - section1:entry3 -- %d\n", tmp);
    zlog_info(z_all, "cfg_get_item - read-section1:entry3 -- %d.", tmp);
    //更新值
    cfg_write_item (pCfg, "section1", "entry3", "%d", 201702);
    cfg_get_item(pCfg, "section1", "entry3", "%d", &value1);
    printf("update - section1:entry3 -- %d\n", value1);
    zlog_info(z_all, "cfg_write_item - update-section1:entry3 -- %d.", value1);

    cfg_getstring(pCfg, "section2", "entry1", buf);
    printf("section2:entry1 -- %s\n", buf);
    zlog_info(z_all, "read-section2:entry1 -- %s.", buf);
    
    cfg_done(pCfg);

    /*************** delete *************/
    cfg_init (&pCfg, "config.ini", 0);
    cfg_write (pCfg, "section1", "entry1", NULL);  /* 删掉section1:entry1 */
    zlog_info(z_all, "delete-section1:entry1.");

    cfg_write (pCfg, "section2", NULL, NULL);	/* 删掉section2 */
    zlog_info(z_all, "delete-section2.");

    cfg_commit (pCfg);	/* 存盘 */
    cfg_done (pCfg);

    zlog_info(z_all, "zlog finish...");
    zlog_fini();
    return 0;
}

