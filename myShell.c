#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_INPUT_SIZE 1000
#define MAX_HISTORY_SIZE 10
#define MAX_TOKEN_NUM 100
#define MAX_BG_PROCESSES 100

int histIdx = 0;
int bgPcId = 0;
const char* builtInCommands[] = {"cd", "myinfo", "history", "exit", "jobs", "fg"};

typedef struct bgProcess{
    int id;
    int status;
    pid_t pid;
    pid_t parentPid;
    char cmd[MAX_INPUT_SIZE];
}bgProcess;

bgProcess* bgPcs;

void initPrompt(){
    printf("\n\n*************SHLOK'S SHELL*************\n\n");
    printf("USER: %s\n", getenv("USER"));
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        printf("Operating System: %s %s\n", sys_info.sysname, sys_info.release);
        printf("Machine Architecture: %s\n", sys_info.machine);
    } else {
        printf("Failed to retrieve OS information.\n");
    }
    printf("\n****************************************\n\n");
}

int readCommandLine(char** cmdLine){
    char prompt[1024];
    char cwd[1024];
    char *username = getenv("USER");
    getcwd(cwd, sizeof(cwd));
    sprintf(prompt, "\033[0;31m%s\033[0;33m@\033[0;36m%s\033[0;32m x-)>>\033[0m ", username, cwd+1);
    *cmdLine = readline(prompt);
	if(strlen(*cmdLine)!=0){
        add_history(*cmdLine);
        histIdx++;
        return 1;
    }
    else{
        return 0;
    }
}

bool parseCommand(char* cmdLine, char** cmd, int* argNum){
    int i = 0;
    char *cpycmdLine = (char*)malloc(MAX_INPUT_SIZE*sizeof(char));
    strcpy(cpycmdLine, cmdLine);
    bool is_comp_cmd = false;
    cmd[i] = strtok(cpycmdLine, " ");
    while(i<MAX_TOKEN_NUM && cmd[i]!=NULL){
        cmd[++i] = strtok(NULL, " ");
        if(cmd[i]!=NULL && strlen(cmd[i])==1){
            if(strlen(cmd[i])==0){  i--;    continue;   }
            else if(strcmp(cmd[i], "<")==0 || strcmp(cmd[i], ">")==0 || strcmp(cmd[i], "|")==0){
                is_comp_cmd = true;
            }
        }
    }
    cmd[i] = NULL;
    *argNum = i;
    return is_comp_cmd;
}

int executeBuiltInCommand(char **cmd, int ipFd, int opFd){

    int cmd_no = -1;
    for(int i=0; i<6; i++){
        if(strcmp(cmd[0], builtInCommands[i])==0){
            cmd_no = i;
            if(ipFd!=-1){
                dup2(ipFd, STDIN_FILENO);
                close(ipFd);
            }
            if(opFd!=-1){
                dup2(opFd, STDOUT_FILENO);
                close(opFd);
            }
            break;
        }
    }

    switch(cmd_no){
        case 0:
            if(cmd[1]==NULL)    chdir(getenv("HOME"));
            else{
                if(chdir(cmd[1])!=0){
                    printf("No such directory exists.\n");
                    printf("Usage: cd <abs or rel directory path>\n");
                }
            }
            return 1;
        case 1:
            initPrompt();
            return 1;
        case 2:
            printf("\n*************COMMAND HISTORY*************\n");
            HIST_ENTRY **history = history_list();
            for(int i=histIdx-10>=0?histIdx-10:0; i<histIdx; i++){
                if(history[i]==NULL)    break;
                printf("%s\n", history[i]->line);
            }
            return 1;
        case 3:
            printf("\n*************THANKS FOR USING MY SHELL*************\n\n");
            exit(0);
        case 4:
            printf("\n*************BACKGROUND PROCESSES*************\n");
            printf("ID  PID  STATUS\t\tCOMMAND\n\n");
            char *status = (char*)malloc(10*sizeof(char));
            for(int i=0; i<bgPcId; i++){
                if(bgPcs[i].pid>0){
                    status = bgPcs[i].status==1?"Running":bgPcs[i].status==0?"Done":"Terminated";
                    printf("[%d] %d %s\t%s\n", bgPcs[i].id, bgPcs[i].pid, status, bgPcs[i].cmd);
                    if(bgPcs[i].status<=0)    bgPcs[i].pid = -1;
                }
            }
            puts("");
            return 1;
        case 5:
            if(cmd[1]==NULL){
                printf("No job id specified.\n");
                printf("Usage: fg <job id>\n");
                return 1;
            }
            int job_id = atoi(cmd[1]);
            if(job_id<0 || job_id>=bgPcId){
                printf("Invalid job id.\n");
                return 1;
            }
            if(bgPcs[job_id].status<=0){
                printf("Background process has been %s.\n", bgPcs[job_id].status==0?"completed":"terminated");
                return 1;
            }
            printf("Waiting...\n");
            int stat;
            waitpid(bgPcs[job_id].parentPid, &stat, 0);
            printf("Process [%d] %s\n", bgPcs[job_id].id, stat==0?"completed":"terminated");
            return 1;
        default:
            return 0;
    }
}

void executeSimpleCommand(char **cmd, int ipFd, int opFd){
    int childPID = fork();
    if(childPID==-1){
        printf("Error in forking child process.\n");
        return;
    }
    if(childPID==0){
        if(ipFd!=-1){
            dup2(ipFd, STDIN_FILENO);
            close(ipFd);
        }
        if(opFd!=-1){
            dup2(opFd, STDOUT_FILENO);
            close(opFd);
        }
        if(execvp(cmd[0], cmd)==-1){
            printf("Command not found.\n");
            exit(-1);
        }
    }
    else{
        waitpid(childPID, NULL, 0);
    }
}

void executeComplexCommand(char **cmd){
    int i=0, curr_cmd=0, ipFD = -1, opFD = -1;
    int prev_pipe[2] = {-1,-1}, next_pipe[2];

    while(cmd[i]!=NULL){
        if(strcmp(cmd[i], "<")==0){
            if(cmd[i+1]==NULL){
                printf("No input file specified.\n");
                return;
            }
            else{
                ipFD = open(cmd[i+1], O_RDONLY);
                if(ipFD==-1){
                    printf("Error in opening input file.\n");
                    return;
                }
                cmd[i] = NULL;
                i++;
            }
        }
        else if(strcmp(cmd[i], ">")==0){
            if(cmd[i+1]==NULL){
                printf("No output file specified.\n");
                return;
            }
            else{
                opFD = open(cmd[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(opFD==-1){
                    printf("Error in opening output file.\n");
                    return;
                }
                cmd[i] = NULL;
                i++;
            }
        }
        else if(strcmp(cmd[i], "|")==0){
            cmd[i] = NULL;
            if(pipe(next_pipe)<0){
                printf("Error in creating pipe.\n");
                return;
            }
            int childPID = fork();
            if(childPID==-1){
                printf("Error in forking child process.\n");
                return;
            }
            if(childPID==0) exit(executeBuiltInCommand(cmd+curr_cmd, prev_pipe[0]==-1?ipFD:prev_pipe[0], next_pipe[1]));
            
            else{
                int status;
                waitpid(childPID, &status, 0);
                if(status==0)   executeSimpleCommand(cmd+curr_cmd, prev_pipe[0]==-1?ipFD:prev_pipe[0], next_pipe[1]);
            }

            close(prev_pipe[0]);
            close(next_pipe[1]);
            ipFD = next_pipe[0];
            opFD = -1;

            prev_pipe[0] = next_pipe[0];
            prev_pipe[1] = next_pipe[1];

            curr_cmd = i+1;
        }
        i++;
    }
    if(executeBuiltInCommand(cmd+curr_cmd, ipFD, opFD)==0)
        executeSimpleCommand(cmd+curr_cmd, ipFD, opFD);
    close(ipFD);
    close(opFD);
}

int main(){
    bool is_cmd, is_comp_cmd;
    int og_ipFD = dup(STDIN_FILENO), og_opFD = dup(STDOUT_FILENO);
    int argNum;
    char *cmdLine, *cmd[MAX_TOKEN_NUM+1];
    pid_t childPID;
    bgPcs = (bgProcess*)mmap(NULL, MAX_BG_PROCESSES*sizeof(bgProcess), PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    initPrompt();

    while(1){
        is_cmd = readCommandLine(&cmdLine);
        if(!is_cmd)    continue;

        is_comp_cmd = parseCommand(cmdLine, cmd, &argNum);

        if(strcmp(cmd[argNum-1], "&")==0){
            childPID = fork();
            if(childPID==-1){
                printf("Error in forking child process.\n");
                continue;
            }
            if(childPID>0){
                printf("Process running in background with ID: %d\n", bgPcId);
                bgPcs[bgPcId].id = bgPcId;
                bgPcs[bgPcId].status = 1;
                bgPcs[bgPcId].parentPid = childPID;
                strcpy(bgPcs[bgPcId].cmd, cmdLine);
                bgPcId++;
                continue;
            }
            else{
                int bgPcPid = fork();
                if(bgPcPid==-1){
                    printf("Error in forking child process.\n");
                    exit(-1);
                }
                if(bgPcPid>0){
                    bgPcs[bgPcId].pid = bgPcPid;
                    int status;
                    waitpid(bgPcPid, &status, 0);
                    bgPcs[bgPcId].status = status==0?0:-1;
                    exit(status);
                }
                else{
                    cmd[argNum-1] = NULL;
                    if(!is_comp_cmd){
                        if(executeBuiltInCommand(cmd, -1, -1)==0)
                            executeSimpleCommand(cmd, -1, -1);
                    }
                    else    executeComplexCommand(cmd);
                    exit(0);
                }
            }
        }

        if(!is_comp_cmd){
            if(executeBuiltInCommand(cmd, -1, -1)==0)
                executeSimpleCommand(cmd, -1, -1);
            continue;
        }
        else{
            executeComplexCommand(cmd);
            dup2(og_ipFD, STDIN_FILENO);
            dup2(og_opFD, STDOUT_FILENO);
        }

    }
    return 0;
}