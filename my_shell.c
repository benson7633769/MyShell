#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#define LSH_TOK_DELIM " \t\r\n\a'"

int cd(char **args);
int help(char **args);
int sh_exit(char **args);
int sh_record(char **args);
int sh_replay(char **args);
int sh_mypid(char **args);
int sh_echo(char **args);

// parse指令時，用此資料結構來存，一個list node 為一個子命令，以 "|" 區分，其中parse出來的字串存在 S 裡面。 
struct list{
    char **s;
    int size;
    int in;
    int out;
    int background;
    char *filename1;
    char *filename2;
    struct list* next;
};

char **recordcommand;
int recordcount;
int parentfd0,parentfd1;
int backgroundd;

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "record",
  "replay",
  "mypid",
  "echo"
};

int (*builtin_func[]) (char **) = {
  &cd,
  &help,
  &sh_exit,
  &sh_record,
  &sh_replay,
  &sh_mypid,
  &sh_echo
};
// 算有幾個 builtin function
int num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}
// 目前的子指令是不是 builtin function?
bool is_builtins(struct list*args){
  for (int i = 0; i < num_builtins(); i++) {
    if (strcmp(((args->s)[0]), builtin_str[i]) == 0) {
       return true;
    }
  }
  return false;
}

//某字串是不是數字?.
int isnum(char* str){
  for(int i=0;i<strlen(str);i++){
    if(str[i]>=48 && str[i]<=57){
      continue;
    }
    else{
      return 0;
    }
  }
  return 1;
}
//執行builtin function或是外部指令，在這之前看有無">"、"<" 並做redirection。
int lsh_launch(struct list*args)
{
  pid_t pid, wpid;
  int status;


  int fd1,fd2;
  if(args->out){
    fd1 = open(args->filename2,O_RDWR);
    dup2(fd1,STDOUT_FILENO);
    close(fd1);
  }
  if(args->in){
    fd2 = open(args->filename1,O_RDWR);
    dup2(fd2,STDIN_FILENO);
    close(fd2);
  }
  if(is_builtins(args)){
    for (int i = 0; i < num_builtins(); i++) {
      if (strcmp(((args->s)[0]), builtin_str[i]) == 0) {
        //printf("%d %d\n",args->background,i);
        /*
        if(args->background && i!=2){
          if ((pid = fork ()) == 0)
          {
              (*builtin_func[i])((args->s));
              exit(1);
          }
          else{
            printf("[pid]: %d\n",pid);
            return 1;
          }
        }
        */
          (*builtin_func[i])((args->s));
          //exit(0);
      }
    }
  }
  else{
    if (execvp((args->s)[0], args->s) == -1) {
        perror("lsh");
    }
  }

  return 1;
}


/*
  Builtin function implementations.
*/
int sh_echo(char **args){
    //echon -n情形，代表不換行。
  if(strcmp(args[1],"-n")==0){
    int i=2;
    while(1){
      if(args[i]==NULL){
        break;
      }
      printf("%s",args[i]);
      if(args[i+1]!=NULL){
        printf(" ");
      }
      i++;
    }
  }
   //無-n情形，要換行。
  else{
    int i=1;
    while(1){
      if(args[i]==NULL){
        printf("\n");
        break;
      }
      printf("%s",args[i]);
      if(args[i+1]!=NULL){
        printf(" ");
      }
      i++;
    }
  }
  return 1;
}

int sh_mypid(char **args){
  pid_t pid;
  char fname[BUFSIZ];
  char buffer[BUFSIZ];
  // mypid -i 直接call getpid()。
  if(strcmp(args[1],"-i")==0){
    pid=getpid();
    printf("%d\n",pid);
  }
  // mypid -p 透過/proc/%s/stat 的 第四個parameter 找到其parent pid。
  else if(strcmp(args[1],"-p")==0){
    if(args[2]==NULL){
      printf("mypid -p: too few argument\n");
      return 1;
    }
    sprintf(fname,"/proc/%s/stat",args[2]);
    int fd = open(fname, O_RDONLY);
    if(fd == -1)
    {
      printf("mypid -p: process id not exist\n");
      return 1;
    }
    size_t ret=read(fd, buffer, BUFSIZ);
    printf("%s\n",buffer);
    strtok(buffer," ");
    strtok(NULL," ");
    strtok(NULL," ");
    char *s_ppid=strtok(NULL," ");
    int ppid=strtol(s_ppid,NULL,10);
    printf("%d\n",ppid);

    close(fd);
  }
    // mypid -c 透過走訪/proc/下的所有pid檔案，看有沒有人的parent pid 等於 自己，有的話，就印出來。
  else if(strcmp(args[1],"-c")==0){
    DIR *dirp;
    struct dirent *direntp;
    if(args[2]==NULL){
      printf("mypid -p: too few argument\n");
      return 1;
    }
    if((dirp=opendir("/proc/"))==NULL){
      printf("open directory error!\n");
      return 1;
    }
    while((direntp=readdir(dirp))!=NULL){
      if(!(isnum(direntp->d_name))){
        continue;
      }
      else{
        sprintf(fname,"/proc/%s/stat",direntp->d_name);
        int fd = open(fname, O_RDONLY);
        if(fd == -1)
        {
          printf("mypid -p: process id not exist\n");
          return 1;
        }
        size_t ret=read(fd, buffer, BUFSIZ);
        strtok(buffer," ");
        strtok(NULL," ");
        strtok(NULL," ");
        char *s_ppid=strtok(NULL," ");
        if(strcmp(s_ppid,args[2])==0){
            printf("%s\n",direntp->d_name);
        }
        close(fd);
      }
    }
    closedir(dirp);
  }
  else{
    printf("wrong type! Please type again!\n");
  }
  return 1;
}
// replay 改在getline完，parse之前做處裡。
int sh_replay(char **args){

  return 1;
}
//運用global circular array來存放歷史指令。
int sh_record(char **args){
  printf("--------------------------------------------------\n");
  printf("Show last 16 command record!!\n");
  int index=1;
  for(int i=recordcount,j=1;j<=16;i++,j++){
    i=i%16;
    if(recordcommand[i][0]!='\0'){
      printf("%d : %s",index,recordcommand[i]);
      index++;
    }
  }
  return 1;
}
//直接運用change directory sys.call。
int cd(char **args)
{
  //printf("You typed: '%s'\n",args[1]);
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}
//help 直接print出一些資訊。
int help(char **args)
{
  int i;
  printf("--------------------------------------------------\n");
  printf("Welcome to our Little Shell!!\n");
  printf("The following are built in:\n");

  for (i = 0; i < num_builtins(); i++) {
    printf("**%s\n", builtin_str[i]);
  }
  return 1;
}
//exit，直接結束main process。
int sh_exit(char **args)
{
  return 0;
}
//每次先把stdin/stdout改回來，因為如果之前是bulitin function加pipe的話，main process的stdin/stdout可能會被改過，所以這邊fork之前先改回來，避免複製到parent的direction導致出錯。
//再fork出child process，再處理pipe()產生的redirection，再判斷是不是background，如果是的話就不用等child process做完
int spawn_proc (int in, int out, struct list* args)
{
  pid_t pid,wpid;
  int status;

  if(is_builtins(args)){
      dup2 (parentfd1, 1);
      dup2 (parentfd0, 0);
      if(backgroundd){
          if ((pid = fork ()) == 0)
          {
            if (in != 0)
            {
              dup2 (in, 0);
              close (in);
            }

            if (out != 1)
            {
              dup2 (out, 1);
              close (out);
            }

            lsh_launch (args);
            exit(1);
          }
          else{
            if(args->background){
              printf("[pid]: %d\n",pid);
            }
            else{
              do {
                  wpid = waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            }
          }
      }
      else{
        if (in != 0)
          {
            dup2 (in, 0);
            close (in);
          }

        if (out != 1)
          {
            dup2 (out, 1);
            close (out);
          }

        lsh_launch (args);
      }
  }
  else {
    dup2 (parentfd1, 1);
    dup2 (parentfd0, 0);
    if ((pid = fork ()) == 0)
      {
        if (in != 0)
          {
            dup2 (in, 0);
            close (in);
          }

        if (out != 1)
          {
            dup2 (out, 1);
            close (out);
          }

        lsh_launch (args);
      }

    else{
      if(args->background){
        printf("[pid]: %d\n",pid);
      }
      else{
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
          } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      }
    }
  }
  return 1;
}

//處理各 子指令 之間的pipe() ，和如何收送direction。
int fork_pipes (struct list* args)
{
  int i;
  pid_t pid;
  int in, fd [2];

  /* The first process should get its input from the original file descriptor 0.  */
  in = 0;

  /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
  while(args->next!=NULL){
      pipe (fd);

      /* f [1] is the write end of the pipe, we carry `in` from the prev iteration.  */
      spawn_proc (in, fd [1], args);

      /* No need for the write end of the pipe, the child will write here.  */
      close (fd [1]);

      /* Keep the read end of the pipe, the next child will read from there.  */
      in = fd [0];
      args=args->next;
  }

  /* Last stage of the pipeline - set stdin be the read end of the previous pipe
     and output to the original file descriptor 1. */  
  if (in != 0)
  {
    spawn_proc (in, 1, args);
    dup2 (parentfd1, 1);
    dup2 (parentfd0, 0);
    return 1;
  }
  /* Execute the last stage with the current process. */
  spawn_proc (0, 1, args);
  dup2 (parentfd1, 1);
  dup2 (parentfd0, 0);
  return 1;
}


int execute(struct list* args)
{
  int i;
  pid_t pid,wpid;
  int status;
  if (args == NULL) {
    // An empty command was entered.
    return 1;
  }

  //for (i = 0; i < num_builtins(); i++) {
    if (strcmp(((args->s)[0]), builtin_str[2]) == 0) {
      //printf("%d %d\n",args->background,i);
      //if(args->background && i!=2){
        //if ((pid = fork ()) == 0)
        //{
            //(*builtin_func[i])((args->s));
            //exit(1);
        //}
        //else{
          //printf("[pid]: %d\n",pid);
          //return 1;
        //}
      //}
      //else{
        return (*builtin_func[2])((args->s));
      //}
    }
  //}

  return fork_pipes(args);
}

//去getline，接收user打的command。

char *read_line(){
    char *buffer;
    size_t bufsize = 1024;
    size_t characters;

    buffer = (char *)malloc(bufsize * sizeof(char));
    if( buffer == NULL)
    {
        perror("Unable to allocate buffer");
        exit(1);
    }
    while(1){
        characters = getline(&buffer,&bufsize,stdin);
        if(characters) break;
    }
    //printf("%zu characters were read.\n",characters);
    //printf("You typed: '%s'\n",buffer);
    return buffer;
}
//透過strtok 去 parse指令，並存成list node。
struct list* split_line(char *line){
    int bufsize = 1024;
    int flag;
    //char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    struct list* temp=(struct list*)malloc(sizeof(struct list));
    temp->s = malloc(16 * sizeof(char*));
    temp->size=0;
    temp->in=0;
    temp->out=0;
    temp->background=0;
    temp->next=NULL;
    struct list* head=temp; 

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        if(token[0]=='|'){
            struct list* pre=temp;
            struct list* temp2=(struct list*)malloc(sizeof(struct list));
            temp2->s = malloc(16 * sizeof(char*));
            temp2->size=0;
            temp2->in=0;
            temp2->out=0;
            temp2->background=0;
            temp2->next=NULL;
            pre->next=temp2;
            temp=temp2;
        }
        else if(token[0]=='<'){
            temp->in=1;
            flag=1;
        }
        else if(token[0]=='>'){
            temp->out=1;
            flag=2;
        }
        else if(token[0]=='&'){
            temp->background=1;
            backgroundd=1;
        }
        else{
          if(flag>0){
            if(flag==1) temp->filename1=token;
            else if(flag==2) temp->filename2=token;
          }
          if(flag<=1){
            (temp->s)[temp->size] = token;
            temp->size ++;
          }
            flag=0;
        }
        //printf("token: '%s'\n",token);

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    (temp->s)[temp->size] = NULL;

    return head;

}
//loop 無窮迴圈一直做，除非user打exit，此shell才會結束跳出來。
void loop(void)
{
  char word[]="replay";
  char replayline[100];
  int status=1;
  int recordindex;
  char *line;  
  char *token1;
  int a;
  recordcount=0;
  struct list* args;
  do {
      //prompt
    printf(">>>");
    backgroundd=0;
      //得到user打的指令字串。
    line = read_line();
      //如果是一直案enter的情況。
    if(line[0]=='\n'){
      continue;
    }
    //處理replay
    for(int i=0;i<6;i++){
      if(line[i]!=word[i]){
        break;
      }
        //是replay!!
      if(i==5){
          //換算index，user打的replay index換成record circular array的index。
        if(recordcount>=17){
          a= strtol(&line[7],NULL,10);
          recordindex=(recordcount+a)%16-1;
        }
        else{
          a= strtol(&line[7],NULL,10);
          recordindex=a-1;
        }
          //讀出歷史指令。
        strcpy(replayline,recordcommand[recordindex]);
        replayline[strlen(replayline)-1]=NULL;
          //搭配replay -12 | ...(後面如果還有子命令，一樣要接著串接)
        if(a<10){
          strcat(replayline,&line[8]);
        }
        else{
          strcat(replayline,&line[9]);
        }
          //copy回line，保持要進去parsing的資料結構名字一致。
        strcpy(line,replayline);
      }
    }
    //把指令存到record。
    strcpy(recordcommand[recordcount%16],line);
    recordcount++;
    args = split_line(line);
    struct list* head=args;
    //printf("%s",args->s[0]);
    status = execute(args);
      //完成該指令，開始free list node。
    struct list* temp;
    while(head){
        temp=head;
        head=head->next;
        free(temp);
    }
    free(line);
  } while (status);


}



int main(void)
{
    //初始化record circular array。
  recordcommand=malloc(16 * sizeof(char*));
  for(int i=0;i<16;i++){
    recordcommand[i]=(char*)malloc(1024*sizeof(char));
  }
  // 先暫存原始指向stdin/stdout的file，之後才能導回來。
  parentfd0=dup(0);
  parentfd1=dup(1);
  //printf("[pid0]: %d\n",parentfd0);
  //printf("[pid1]: %d\n",parentfd1);
  // Run command loop.
  loop();
  for(int i=0;i<16;i++){
    free(recordcommand[i]);
  }
  free(recordcommand);

  return 0;
}
