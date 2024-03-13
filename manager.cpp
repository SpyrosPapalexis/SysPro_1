#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <vector>
#include <regex>

#include "queue.hpp"

#define BUFFER 1024
#define BIGBUFFER 1024*1024
#define FIFODIR "fifo/"
#define OUTDIR "out/"

using namespace std;

bool flag = 0;

void handler(int signum){
    flag = 1;
    signal(SIGCHLD, handler);
}

int main(int argc, char *argv[]){
    char *dir = (char*)"./";                                    //default directory for inotifywait
    if (argc == 3){
        if (strcmp(argv[1],"-p") == 0){
            dir = argv[2];
        }else cout << "incorrect function call, ./ directory will be used instead" << endl;
    }else cout << "incorrect function call, ./ directory will be used instead" << endl;

    mkdir(FIFODIR, S_IRWXU);
    mkdir(OUTDIR, S_IRWXU);

    int fd[2];
    if (pipe(fd) < 0) return -1;
    if (fork() == 0){
        dup2(fd[1],1);
        close(fd[0]);
        char* arguments[] = {(char*)"inotifywait",dir,(char*)"-m",(char*)"-e",(char*)"create",(char*)"-e",(char*)"moved_to",NULL};
        if(execvp("inotifywait",arguments) == -1) cout << "error" << endl;
        return 0;
    }else{

        Queue workers;
        signal(SIGCHLD, handler);
        while(1){

            char buffer[BUFFER] = "";
            read(fd[0],buffer,BUFFER);
            char *word = NULL;
            char *f = NULL;

            word = strtok(buffer," \n");                                    //look for the last word of inotifywait print
            while(word != NULL){
                f = word;
                word = strtok(NULL," \n");
            }

            while (flag == 1){                                              //if handler detects workers waiting, push them all in queue
                pid_t stopped = waitpid(-1, NULL, WNOHANG | WUNTRACED);
                if (stopped <= 0){
                    flag = 0;
                    break;
                }
                workers.push(stopped);
            }

            if (workers.isempty()){
                pid_t pid = fork();

                if (pid > 0){       //parent
                    const char* fifoname = to_string(pid).c_str();          //fifo name
                    char fifodir[BUFFER] = FIFODIR;                         //fifo directory
                    strcat(fifodir,fifoname);                               //adding fifo name to directory
                    if (mkfifo(fifodir, 0666) < 0){                         //making fifo
                        cout << "Failed to create named pipe" << endl;
                        return -1;
                    }

                    int fifo;
                    if ((fifo = open(fifodir, O_WRONLY)) == -1){            //open fifo
                        cout << "Can't open " << fifodir << endl;
                        return -1; 
                    }
                    if (write(fifo,f,BUFFER) == -1){                        //write in fifo
                        cout << "Can't write at " << fifodir << endl;
                        return -1; 
                    }

                }else if(pid == 0){ //child
                    while(1){
                        const char* fifoname = to_string(getpid()).c_str(); //fifo name
                        char fifodir[BUFFER] = FIFODIR;                     //fifo directory
                        strcat(fifodir,fifoname);                           //adding fifo name to directory
                        int fifo;
                        if ((fifo = open(fifodir, O_RDONLY)) == -1){        //open fifo
                            cout << "Can't open " << fifodir << endl;
                            return -1;
                        }

                        char filedir[BUFFER] = "";                          //buffer used to store directory of files
                        strcat(filedir,dir);                                //adding directory to the directory buffer
                        char buffer[BUFFER] = "";                           //buffer used to store name of txt
                        char txtbuffer[BIGBUFFER] = "";                     //buffer used to store contents of txt

                        if (read(fifo,buffer,BUFFER) == -1){                //read fifo
                            cout << "Can't read from " << fifodir << endl;
                            return -1;
                        }

                        strcat(filedir,buffer);
                        int file;
                        if ((file = open(filedir, O_RDONLY)) == -1){        //open txt
                            cout << "Can't open " << filedir << endl;
                            return -1;
                        }

                        if (read(file,txtbuffer,BIGBUFFER) == -1){          //read txt
                            cout << "Can't read from " << filedir << endl;
                            return -1;
                        }

                        //code below searches for urls and their appearance in the txtbuffer

                        vector<string> urls;                                //url vector
                        vector<int> count;                                  //url count vector
                        string stringbuffer = txtbuffer;                    //turning txtbuffer from char[] to string
                        regex expr("http://[a-z.0-9-]+");
                        smatch temp;
                        while(regex_search(stringbuffer, temp, expr)){      //searching for urls
                            string url = temp.str();
                            int i = url.find("http://");
                            url = url.substr(i+7);
                            i = url.find("www.");
                            if(i!=-1){                                    //check if url has a www.
                                url = url.substr(i+4);                    //remove www.
                            }
                            bool found = 0;
                            for(int i=0; i<urls.size(); i++){               //look for url in vector
                                if  (url == urls[i]){                       //if url already in vector, increase its count
                                    count[i]++;
                                    found = 1;
                                    break;
                                }
                            }
                            if (found == 0){                                //if not, add it in vector
                                urls.push_back(url);
                                count.push_back(1);
                            }

                            stringbuffer = temp.suffix().str();             //add the rest of the text in the buffer
                        }
                        
                        string output;

                        for(int i=0; i<urls.size(); i++){                   //build the output format
                            output += urls[i];
                            output += " ";
                            output += to_string(count[i]);
                            output += '\n';
                        }

                        const char *outputc = output.c_str();               //cast the output

                        char suffix[BUFFER] = ".out";                       //store .out suffix
                        char outdir[BUFFER] = OUTDIR;                       //store directory of .out files
                        strcat(outdir,buffer);                              //add file name to directory
                        strcat(outdir,suffix);                              //add suffix to file name and directory
                        int out;
                        if ((out = open(outdir, O_WRONLY | O_CREAT, 0666)) == -1){  //create and open out
                            cout << "Can't open " << outdir << endl;
                            return -1;
                        }

                        if (write(out,outputc,strlen(outputc)) == -1){              //write out
                            cout << "Can't write at " << outdir << endl;
                            return -1;
                        }

                        kill(getpid(), SIGSTOP);                            //pause child
                    }
                }else{
                    cout << "Failed to create worker" << endl;
                    return -1;
                }

            }else{
                pid_t pid = workers.pop();                                  //get next available worker
                const char* fifoname = to_string(pid).c_str();              //get fifoname by the pid associated with it
                char fifodir[BUFFER] = FIFODIR;
                strcat(fifodir,fifoname);
                int fifo;
                if ((fifo = open(fifodir, O_WRONLY)) == -1){                //open fifo
                    cout << "Can't open " << fifodir << endl;
                    return -1;
                }

                if (write(fifo,f,BUFFER) == -1){                            //write in fifo
                    cout << "Can't write at " << fifodir << endl;
                    return -1; 
                }

                kill(pid, SIGCONT);                                         //signal child to continue
            }
        }
    }
}