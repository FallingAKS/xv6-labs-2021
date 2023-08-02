#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int   argcExec = argc - 1;
    char* argvExec[MAXARG];          //新参数列表
    char  para[256], *pPara = para;  //参数暂存
    char  ch;                        //读取到的字符

    for (int i = 1; i < argc; i++) {
        argvExec[i - 1] = argv[i];
    }
    //字符串的不同位置写入不同的para
    argvExec[argcExec++] = pPara;

    while (read(0, &ch, sizeof(char)) == sizeof(char)) {
        if (ch == ' ') {
            *pPara++ = '\0';
            //字符串的不同位置写入不同的para
            argvExec[argcExec++] = pPara;
        }
        else if (ch == '\n') {
            *pPara             = '\0';
            argvExec[argcExec] = 0;
            if (fork() == 0) {
                exec(argvExec[0], argvExec);
                exit(0);
            }
            else {
                wait((int*)0);
                pPara    = para;
                argcExec = argc;
            }
        }
        else {
            *pPara++ = ch;
        }
    }
    exit(0);
}
