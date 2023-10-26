#include "include/csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg);
void echo(int connfd);
void sigchild_handler(int sig);

//处理一个HTTP事务
void doit(int fd) {
    int is_static; //静态资源
    struct stat sbuf;
    char buf[MAXLINE];
    char method[MAXLINE];
    char uri[MAXLINE];
    char version[MAXLINE];
    char filename[MAXLINE];
    char cgiargs[MAXLINE];
    rio_t rio;

    //Read request lines and headers
    Rio_readinitb(&rio, fd); //初始化一个空缓冲区并于fd关联
    Rio_readlineb(&rio, buf, MAXLINE); //在这里假设客户输入的是：Get /hello.txt HTTP/1.0
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(!(strcasecmp(method, "Get") == 0) && !(strcasecmp(method, "HEAD") == 0)) {
        clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio); //检测是否读取到了\r\n,读取到了则继续执行 

    //Parse URI from GET request
    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
        return;
    }

    if(is_static) {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read this file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method);
    }
    else {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs, method);
    }
}

//打印出错信息
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg) {
    char buf[MAXLINE];
    char body[MAXLINE];

    //Build the HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    //Print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if(!strstr(uri, "cgi-bin")) { //请求静态资源
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if(uri[strlen(uri) - 1] == '/') {
            strcat(filename, "home.html");
        }
        return 1;
    }
    else {
        ptr = index(uri, '?');
        if(ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

//取得文件类型
void get_filetype(char *filename, char *filetype) {
    if(strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    }
    else if(strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    }
    else if(strstr(filename, ".png")) {
        strcpy(filetype, "image/png");
    }
    else if(strstr(filename, ".jpg")) {
        strcpy(filetype, "image/jpeg");
    }
    else {
        strcpy(filetype, "text/plain");
    }
}

//请求静态资源
void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp;
    char filetype[MAXLINE];
    char buf[MAXLINE];

    //Send response headers to client
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf)); //写给客户端
    printf("Response headers: \n");
    printf("%s", buf);

    if(strcasecmp(method, "HEAD") == 0) {
        return;
    }

    //Send response body to client 
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize); //写给客户端
    Munmap(srcp, filesize);
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE];
    char *emptylist[] = {
        NULL
    };

    //Run first part of HTTP response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if(Fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
        setenv("REQUEST_METHOD", method, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
}

//原样返回请求行
void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        if(strcmp(buf, "\r\n") == 0) {
            break;
        }
        Rio_writen(connfd, buf, n);
    }
}

//信号处理程序
void sigchild_handler(int sig) {
    int old_errno = errno;
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG)) >0 ) {

    }
    errno = old_errno;
}

int main(int argc, char* argv[])
{
    int listenfd;
    int connfd;
    char hostname[MAXLINE];
    char port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //Check command-line args
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if(Signal(SIGCHLD, sigchild_handler) == SIG_ERR) {
        unix_error("signal child handler error");
    }

    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd); //处理一个HTTP事务
        echo(connfd); //原样返回请求行
        Close(connfd);
    }

    return 0;
}




