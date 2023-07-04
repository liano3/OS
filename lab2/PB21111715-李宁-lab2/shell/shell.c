#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_CMDLINE_LENGTH  1024    /* max cmdline length in a line*/
#define MAX_BUF_SIZE        4096    /* max buffer size */
#define MAX_CMD_ARG_NUM     32      /* max number of single command args */
#define WRITE_END 1     // pipe write end
#define READ_END 0      // pipe read end

/* 
 * 需要大家完成的代码已经用注释`TODO:`标记
 * 可以编辑器搜索找到
 * 使用支持TODO高亮编辑器（如vscode装TODO highlight插件）的同学可以轻松找到要添加内容的地方。
 */

/*  
    int split_string(char* string, char *sep, char** string_clips);

    基于分隔符sep对于string做分割，并去掉头尾的空格

    arguments:      char* string, 输入, 待分割的字符串 
                    char* sep, 输入, 分割符
                    char** string_clips, 输出, 分割好的字符串数组

    return:   分割的段数 
*/

int split_string(char* string, char *sep, char** string_clips) {
    
    char string_dup[MAX_BUF_SIZE];
    // C语言库函数strtok，分解字符串，返回第一个子串
    string_clips[0] = strtok(string, sep);
    int clip_num=0;
    
    do {
        // 去除首尾空格
        char *head, *tail;
        head = string_clips[clip_num];
        tail = head + strlen(string_clips[clip_num]) - 1;
        while(*head == ' ' && head != tail)
            head ++;
        while(*tail == ' ' && tail != head)
            tail --;
        *(tail + 1) = '\0';
        string_clips[clip_num] = head;
        clip_num ++;
    } while(string_clips[clip_num]=strtok(NULL, sep));
    return clip_num;
}

/*
    执行内置命令
    arguments:
        argc: 输入，命令的参数个数
        argv: 输入，依次代表每个参数，注意第一个参数就是要执行的命令，
        若执行"ls a b c"命令，则argc=4, argv={"ls", "a", "b", "c"}
        fd: 输出，命令输入和输出的文件描述符 (Deprecated)
    return:
        int, 若执行成功返回0，否则返回值非零
*/
char old_path[MAX_BUF_SIZE]; // 保存上一次的工作目录
// NOTE: 文件描述符（file descriptor）是内核为了高效管理已被打开的文件所创建的索引，用于指代被打开的文件
int exec_builtin(int argc, char**argv, int *fd) {
    if(argc == 0) {
        return 0;
    }
    /* TODO: 添加和实现内置指令 */

    if (strcmp(argv[0], "cd") == 0) {
        // "cd -" 切换到上一次的工作目录
        if (strcmp(argv[1], "-") == 0) {
            char tmp[MAX_BUF_SIZE];
            getcwd(tmp, MAX_BUF_SIZE);
            chdir(old_path);
            strcpy(old_path, tmp);
            return 0;
        }
        else {
            getcwd(old_path, MAX_BUF_SIZE);
            // C语言库函数chdir，改变当前工作目录
            return chdir(argv[1]);
        }
    }
    else if (strcmp(argv[0], "kill") == 0) {
        // C语言库函数kill，向进程发送信号
        if (argc == 2)
            return kill(atoi(argv[1]), SIGTERM);
        else if (argc == 3)
            return kill(atoi(argv[1]), atoi(argv[2]));
        else
            return -1;
    }
    else if (strcmp(argv[0], "exit") == 0){
        // C语言库函数exit，退出程序
        exit(0);
    }
    else {
        // 不是内置指令时
        return -1;
    }
}

/*
    从argv中删除重定向符和随后的参数，并打开对应的文件，将文件描述符放在fd数组中。
    运行后，fd[0]读端的文件描述符，fd[1]是写端的文件描述符
    arguments:
        argc: 输入，命令的参数个数
        argv: 输入，依次代表每个参数，注意第一个参数就是要执行的命令，
        若执行"ls a b c"命令，则argc=4, argv={"ls", "a", "b", "c"}
        fd: 输出，命令输入和输出使用的文件描述符
    return:
        int, 返回处理过重定向后命令的参数个数
*/

int process_redirect(int argc, char** argv, int *fd) {
    /* 默认输入输出到命令行，即输入STDIN_FILENO，输出STDOUT_FILENO */
    fd[READ_END] = STDIN_FILENO;
    fd[WRITE_END] = STDOUT_FILENO;
    int i = 0, j = 0;
    while(i < argc) {
        int tfd;
        if(strcmp(argv[i], ">") == 0) {
            //TODO: 打开输出文件从头写入
            tfd = open(argv[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if(tfd < 0) {
                printf("open '%s' error: %s\n", argv[i+1], strerror(errno));
            } 
            else {
                //TODO: 输出重定向
                fd[WRITE_END] = tfd;
            }
            i += 2;
        } 
        else if(strcmp(argv[i], ">>") == 0) {
            //TODO: 打开输出文件追加写入
            tfd = open(argv[i+1], O_WRONLY | O_CREAT | O_APPEND, 0666);
            if(tfd < 0) {
                printf("open '%s' error: %s\n", argv[i+1], strerror(errno));
            } 
            else {
                //TODO:输出重定向
                fd[WRITE_END] = tfd;
            }
            i += 2;
        } 
        else if(strcmp(argv[i], "<") == 0) {
            //TODO: 读输入文件
            tfd = open(argv[i+1], O_RDONLY);
            if(tfd < 0) {
                printf("open '%s' error: %s\n", argv[i+1], strerror(errno));
            } 
            else {
                //TODO: 输入重定向
                fd[READ_END] = tfd;
            }
            i += 2;
        } 
        else {
            argv[j++] = argv[i++];
        }
    }
    argv[j] = NULL;
    return j;   // 新的argc
}



/*
    在本进程中执行，且执行完毕后结束进程。
    arguments:
        argc: 命令的参数个数
        argv: 依次代表每个参数，注意第一个参数就是要执行的命令，
        若执行"ls a b c"命令，则argc=4, argv={"ls", "a", "b", "c"}
    return:
        int, 若执行成功则不会返回（进程直接结束），否则返回非零
*/
int execute(int argc, char** argv) {
    int fd[2];
    // 默认输入输出到命令行，即输入STDIN_FILENO，输出STDOUT_FILENO 
    fd[READ_END] = STDIN_FILENO;
    fd[WRITE_END] = STDOUT_FILENO;
    // 处理重定向符，如果不做本部分内容，请注释掉process_redirect的调用
    argc = process_redirect(argc, argv, fd);
    if(exec_builtin(argc, argv, fd) == 0) {
        exit(0);
    }
    // 将标准输入输出STDIN_FILENO和STDOUT_FILENO修改为fd对应的文件
    dup2(fd[READ_END], STDIN_FILENO);
    dup2(fd[WRITE_END], STDOUT_FILENO);
    /* TODO:运行命令与结束 */
    // C语言库函数execvp，执行命令
    execvp(argv[0], argv);
    // C语言库函数perror，打印错误信息
    perror("execvp");
    exit(1);
}

// 运行一个包含管道或重定向符的命令
int run_command(char *cmdline) {
    /* 由管道操作符'|'分割的命令行各个部分，每个部分是一条命令 */
    /* 拆解命令行 */
    char *commands[128];
    int cmd_count = split_string(cmdline, "|", commands);
    if(cmd_count == 0)
        return 0;
    else if(cmd_count == 1) {     // 没有管道的单一命令
        char *argv[MAX_CMD_ARG_NUM];
        int argc;
        int fd[2];
        // 处理参数，分出命令名和参数
        argc = split_string(commands[0], " ", argv);
        if(exec_builtin(argc, argv, fd) == 0)
            return 0;
        // 创建子进程，运行命令，等待命令运行结束
        int pid = fork();
        if(pid == 0)
            execute(argc, argv);
        else	wait(NULL);
    }
    else {
        int read_fd;    // 上一个管道的读端口（出口）
        for(int i = 0; i < cmd_count; i++) {
            int pipefd[2];
            /* 创建管道，n条命令只需要n-1个管道 */
            if(i != cmd_count - 1) {
                int ret = pipe(pipefd);
                if(ret < 0) {
                    printf("pipe error!\n");
                    continue;
                }
            }
            int pid = fork();
            if(pid == 0) {
                /* 除了最后一条命令外，都将标准输出重定向到当前管道入口 */
                if(i != cmd_count - 1) {
                    dup2(pipefd[WRITE_END], STDOUT_FILENO);
                    close(pipefd[WRITE_END]);
                }
                /* 除了第一条命令外，都将标准输入重定向到上一个管道出口 */
                if(i != 0) {
                    dup2(read_fd, STDIN_FILENO);
                    close(read_fd);
                }
                char *argv[MAX_CMD_ARG_NUM];
                int argc = split_string(commands[i], " ", argv);
                execute(argc, argv);
                exit(255);
            }
            /* 父进程除了第一条命令，都需要关闭当前命令用完的上一个管道读端口 
            * 父进程除了最后一条命令，都需要保存当前命令的管道读端口 
            * 记得关闭父进程没用的管道写端口 */
            if(i != 0) {
                close(read_fd);
            }
            if(i != cmd_count - 1) {
                read_fd = pipefd[READ_END];
                close(pipefd[WRITE_END]);
            }
        }
        // 等待所有子进程结束
        while (wait(NULL) > 0);
    }
}

int main() {
    /* 输入的命令行 */
    char cmdline[MAX_CMDLINE_LENGTH];

    char *commands[128];
    int cmd_count;
    while (1) {
        /* TODO: 增加打印当前目录，格式类似"shell:/home/oslab ->"，你需要改下面的printf */
        printf("shell:%s -> ", getcwd(NULL, 0));
        fflush(stdout);

        fgets(cmdline, 256, stdin);
        strtok(cmdline, "\n");

        /* TODO: 基于";"的多命令执行，请自行选择位置添加 */
        // 多命令执行
        cmd_count = split_string(cmdline, ";", commands);
        for (int i = 0; i < cmd_count; i++) {
            run_command(commands[i]);
        }
    }
}
