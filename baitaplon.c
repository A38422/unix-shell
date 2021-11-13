#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define MAXLINE 80   /* Độ dài tối đa của lệnh */
#define MAXWORD 20  /* Độ dài tối đa của một tham số dòng lệnh*/

/* Bộ mô tả tệp cho đầu vào, chuyển hướng đầu ra */
int fdi, fdo;
int saved_stdin, saved_stdout;

char* args[MAXLINE / 2 + 1]; /* Tham số dòng lệnh */
int nArgs; /* Số lượng tham số */

// phân tích từ vựng trong chuỗi
void stringTokenizer(char* tokens[], char* source, char* delim, int *nWords)
{
    char *ptr = strtok(source, delim);
    int count = 0;
    while (ptr != NULL)
    {
        strcpy(tokens[count], ptr);
        count++;
        ptr = strtok(NULL, delim);
    }
    *nWords = count;
    return;
}
// phân bổ bộ nhớ
void allocateMemory(char* argArray[])
{
    for (int i = 0; i < MAXLINE / 2 + 1; i++)
    {
        argArray[i] = (char *)malloc(sizeof(char) * MAXWORD);
    }
}
// giải phóng bộ nhớ
void freeMemory(char* argArray[])
{
    for (int i = 0; i < MAXLINE / 2 + 1; i++)
    {
        if (argArray[i] != NULL)
        {
            free(argArray[i]);
            argArray[i] = NULL;
        }
    }
}
// xử lý chuyển hướng đầu ra
void handle_output_redirect()
{
    saved_stdout = dup(STDOUT_FILENO);
    fdo = open(args[nArgs - 1], O_CREAT | O_WRONLY | O_TRUNC);
    dup2(fdo, STDOUT_FILENO);
    free(args[nArgs - 2]);
    args[nArgs - 2] = NULL;
}
// tạo bản ghi mới stdout
void restore_STDOUT()
{
    close(fdo);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
}
// xử lý chuyển hướng đầu vào
void handle_input_redirect()
{
    saved_stdin = dup(STDIN_FILENO);
    fdi = open(args[nArgs - 1], O_RDONLY);
    dup2(fdi, STDIN_FILENO);
    free(args[nArgs - 2]);
    args[nArgs - 2] = NULL; 
}
// tạo bản ghi stdin mới
void restore_STDIN()
{
    close(fdi);
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
}
// nhận lệnh từ bàn phím
void enterCommandLine(char command[])
{
    fgets(command, MAXLINE, stdin);
    int len = strlen(command);
    command[len - 1] = 0;
}
// Hàm thực thi lệnh hệ thống pipe
void execArgsPiped(char *parsed[], char *parsedpipe[])
{
    int pipefd[2];
    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 < 0)
    {
        printf("\nCould not fork"); // không thể gọi hệ thống
        return;
    }

    if (pid1 == 0)
    {
        if (pipe(pipefd) < 0)
        {
            printf("\nPipe could not be initialized"); // không thể khởi tạo pipe
            return;
        }

        pid2 = fork();
        if (pid2 < 0)
        {
            printf("\nCould not fork");
            exit(0);
        }

        // Tiến trình con 2 đang thực hiện
        if (pid2 == 0)
        {
            // đóng đọc pipe
            close(pipefd[0]);
            // ghi pipe vào stdout 
            dup2(pipefd[1], STDOUT_FILENO);
            
            if (execvp(parsed[0], parsed) < 0)
            {
                printf("\nCould not execute command 1"); // không thể thực hiện lệnh 1
                exit(0);
            }
        }
        else
        // Tiến trình con 1 đang thực hiện
        {
            wait(NULL);
            // đóng ghi pipe 
            close(pipefd[1]);
            // ghi pipe vào sidin
            dup2(pipefd[0], STDIN_FILENO);
            
            if (execvp(parsedpipe[0], parsedpipe) < 0)
            {
                printf("\nCould not execute command 2"); // không thể thực hiện lệnh 2
                exit(0);
            }
        } 
    }
    else
    {
        // chờ 2 tiến trình con thực hiện xog rồi tiến trình cha mới thực hiện
        wait(NULL);
    }

}

// hàm tìm kiếm và phân tách pipe
// truyền vào cmd và mảng để lưu lại pipe trong cmd
int parsePipe(char *cmd, char* cmdpiped[])
{
    int i;
    for (i = 0; i < 2; i++)
    {
        cmdpiped[i] = strsep(&cmd, "|");
        if (cmdpiped[i] == NULL)
            break;
    }
    if (cmdpiped[1] == NULL)
        return 0; // trả về 0 nếu không tìm thấy
    else
    {
        return 1;
    }
}
// phân tách từng tham số
// truyền vào cmd và mảng để lưu lại các lệnh trong cmd
void parseSpace(char *cmd, char *parsedArg[])
{
    int i = 0;

    while (cmd != NULL)
    {
        parsedArg[i] = strsep(&cmd, " "); // tách tham số truyền vào
        if (parsedArg[i][0] != 0)
            i++;
    }
    parsedArg[i] = NULL;
}
// kiểm tra cmd có pipe hay không
int processString(char *cmd, char *parsed[], char *parsedpipe[])
{
    char *cmdpiped[2];
    int piped = 0;

    piped = parsePipe(cmd, cmdpiped); // kiểm tra pipe có tồn tại hay không
    // nếu tồn tại pipe thì tiến hành phân tách các lệnh và pipe
    if (piped)
    {
        parseSpace(cmdpiped[0], parsed);
        parseSpace(cmdpiped[1], parsedpipe);
    }
    return piped;
}

int main(void)
{
    printf(">>>>> UNIX - SHELL <<<<<\n");
    sleep(3);
    system("clear");
    char command[MAXLINE]; // lenh do nguoi dung nhap
    char lastCommand[MAXLINE]; // luu lai lich su lenh
    strcpy(lastCommand, "\0");
    char* parsedArgs[MAXLINE / 2 + 1];  // lenh dau tien cua pipe
    char* parsedArgsPiped[MAXLINE / 2 + 1]; // lenh thu hai cua pipe
    int pipeExec = 0;   // thong bao co thuc hien pipe hay khong
    pid_t pid;
    int isRedirectOutput = 0, isRedirectInput = 0;
    // đầu ra chuyển hướng và đầu vào chuyển hướng
    while (1)
    {
        // truong hop nguoi dung khong nhap gi
        do
        {
            printf("minhdt>");
            fflush(stdout);
            enterCommandLine(command);
        } while (command[0] == 0);

        // nếu truyền tham số !! nhận được 0
        if (strcmp(command, "!!") == 0)
        {
            if (lastCommand[0] == 0) // và nếu lịch sử lệnh trống thì in ra không có lệnh trong lịch sử
            {
                printf("No commands in history!\n");
                continue;
            }
            else
            {
                printf("%s\n", lastCommand); // ngược lại thì in ra lệnh trong lịch sử và lưu lệnh lại
                strcpy(command, lastCommand);
            }
        }
        else
        {
            //  lưu lệnh người dùng nhập
            strcpy(lastCommand, command);
        }
        
        // pipeExec trả về 0 nếu là một lệnh đơn giản
        // trả về 1 nếu là lệnh gồm pipe
        pipeExec = processString(command, parsedArgs, parsedArgsPiped);

        if (pipeExec == 0)
        {
            allocateMemory(args);
            strcpy(command, lastCommand);
            stringTokenizer(args, command, " ", &nArgs);
            free(args[nArgs]);
            // sau đối số cuối cùng phải là NULL
            args[nArgs] = NULL;

            if (strcmp(args[0], "exit") == 0) // nếu lệnh là exit thì thoát khỏi shell
            {
                break;
            }
            else
            {
                if (strcmp(args[0], "~") == 0)
                {
                    char* homedir = getenv("HOME"); // trả về giá trị của home nếu tồn tại không thì trả về NULL
                    printf("Home directory: %s\n", homedir); // thư mục chính
                }
                else // cd lệnh thay đổi thư mục
                if (strcmp(args[0], "cd") == 0)
                {
                    char dir[MAXLINE];
                    strcpy(dir, args[1]);
                    if (strcmp(dir, "~") == 0)
                    {
                        strcpy(dir, getenv("HOME"));
                    }
                    chdir(dir);
                    printf("Current directory: "); // thư mục hiện tại
                    getcwd(dir, sizeof(dir));
                    printf("%s\n", dir);                    
                }
                else
                {
                    // nếu đối số cuối cùng là & thì tiến trình cha không đợi chạy đồng thời
                    // ngược lại thì tiến trình cha đợi tiến trình con hoàn thành rồi mới chạy
                    int parentWait = strcmp(args[nArgs - 1], "&");

                    if (parentWait == 0)
                    {
                        free(args[nArgs - 1]);
                        args[nArgs - 1] = NULL;
                    }

                    // chuyển hướng đầu ra
                    if ((nArgs > 1) && strcmp(args[nArgs - 2], ">") == 0)
                    {
                        handle_output_redirect();
                        isRedirectOutput = 1;
                    }
                    else
                        // chuyển hướng đầu vào
                        if ((nArgs > 1) && strcmp(args[nArgs - 2], "<") == 0)
                    {
                        handle_input_redirect();
                        isRedirectInput = 1;
                    }

                    pid = fork();

                    if (pid < 0)
                    {
                        fprintf(stderr, "FORK FAILED\n"); // nếu pid < 0 thì gọi fork thất bại
                        return -1;
                    }
                    if (pid == 0)
                    {

                        int ret = execvp(args[0], args); // kiểm tra lệnh có hợp lệ hay không

                        if (ret == -1)
                        {
                            printf("Invalid command\n"); 
                        } 
                        exit(ret);
                    }
                    else
                    {
                        if (parentWait)
                        {
                            while (wait(NULL) != pid);
                            if (isRedirectOutput)
                            {
                                restore_STDOUT();
                                isRedirectOutput = 0;
                            }
                            if (isRedirectInput)
                            {
                                restore_STDIN();
                                isRedirectInput = 0;
                            }
                        }
                        freeMemory(args);
                    }
                }
            }
        }
        else
        {
            execArgsPiped(parsedArgs, parsedArgsPiped);
        }
    }
    freeMemory(args);
    return 0;
}

// pipe là đường ống giúp thực hiện nhiều lệnh "|"