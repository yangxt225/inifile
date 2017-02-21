/************ write *****************
使用inifile库的例子
假设要创建一个write.ini，内容如下：
; write.ini
[section1]
entry1 = abc
entry2 = 123 0X12AC

然后把entry2改为256 300,再用另外的方法创建：
[section2]
entry1 = test
*/

/**************** read **************
使用inifile库的例子
假设当前目录下有一文件read.ini，内容如下：
; read.ini
[section1]
entry1 = test1
entry2 = 123

[section2]
entry1 = 10.32abc
entry2 = adfasd dsf 100

下面的代码演示了如何读取该文件,但没有判断任何错误。
*/

/**************** delete *************
使用inifile库的例子
假设当前目录下有一文件delete.ini，内容如下：
; delete.ini
[section1]
entry1 = test1
entry2 = 123

[section2]
entry1 = 10.32abc
entry2 = adfasd dsf 100

现在删掉section1:entry1,然后删掉整个section2
*/


#include "inifile.h"
#include "zlog.h"

