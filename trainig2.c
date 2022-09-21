#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_client
{
    int id;
    int fd;
    struct s_client *next;
} t_client;

t_client *cli = NULL;
int sockfd, id = 0;
fd_set curr, reads, writes;
char msg[42 * 4096], buf[42 * 4096 + 42];

void fatal()
{
    write(2, "Fatal error\n", 12);
    close(sockfd);
    exit(1);
}

int get_id(int fd)
{
    t_client *tmp = cli;

    while (tmp)
    {
        if (tmp->fd == fd)
            return tmp->id;
        tmp = tmp->next;
    }
    return -1;
}

int get_max_fd()
{
    int maxfd = sockfd;
    t_client *tmp = cli;

    while (tmp)
    {
        if (tmp->fd > maxfd)
            maxfd = tmp->fd;
        tmp = tmp->next;
    }
    return maxfd;
}

void send_all(int from)
{
    t_client *tmp = cli;

    while (tmp)
    {
        if (tmp->fd != from && FD_ISSET(tmp->fd, &writes))
            send(tmp->fd, buf, strlen(buf), 0);
        tmp = tmp->next;
    }
}

void add_client()
{
    t_client *new;
    int clientfd;
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
    if (clientfd < 0)
        fatal();
    bzero(&buf, sizeof(buf));
    sprintf(buf, "server: client %d just arrived\n", id);
    send_all(clientfd);
    FD_SET(clientfd, &curr);
    if (!(new = calloc(1, sizeof(t_client))))
        fatal();
    new->fd = clientfd;
    new->id = id++;
    new->next = NULL;

    if (!cli)
        cli = new;
    else
    {
        t_client *tmp = cli;

        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
}

void remove_client(int fd)
{
    t_client *del = NULL;

    bzero(&buf, sizeof(buf));
    sprintf(buf, "server: client %d just left\n", get_id(fd));
    send_all(fd);

    if (cli && cli->fd == fd)
    {
        del = cli;
        cli = cli->next;
    }
    else
    {
        t_client *tmp = cli;

        while (tmp && tmp->next && tmp->next->fd != fd)
            tmp = tmp->next;
        if (tmp && tmp->next && tmp->next->fd == fd)
        {
            del = tmp->next;
            tmp->next = tmp->next->next;
        }
    }
    if (del)
        free(del);

    close(fd);
    FD_CLR(fd, &curr);
}

void extract_message(int fd)
{
    int i = 0, j = 0;
    char tmp[42 * 4096];

    bzero(&tmp, sizeof(tmp));
    while (msg[i])
    {
        tmp[j] = msg[i];
        j++;
        if (msg[i] == '\n')
        {
            bzero(&buf, sizeof(buf));
            sprintf(buf, "client %d: %s", get_id(fd), tmp);
            send_all(fd);
            j = 0;
            bzero(&tmp, sizeof(tmp));
        }
        i++;
    }
    bzero(&msg, sizeof(msg));
}

int main(int ac, char **av)
{
    if (ac != 2)
    {
        write(2, "Wrong numbers of arguments\n", 26);
        exit(1);
    }
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        fatal();
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        fatal();
    if (listen(sockfd, 128) != 0)
        fatal();

    FD_ZERO(&curr);
    FD_SET(sockfd, &curr);

    bzero(&msg, sizeof(msg));
    bzero(&buf, sizeof(buf));

    while (1)
    {
        reads = writes = curr;

        if (select(get_max_fd() + 1, &reads, &writes, 0, 0) < 0)
            continue;
        for (int fd = sockfd; fd <= get_max_fd(); fd++)
        {
            if (FD_ISSET(fd, &reads))
            {
                if (fd == sockfd)
                {
                    add_client();
                    break;
                }
                int ret = 1;
                while (ret == 1 && msg[strlen(msg) - 1] != '\n')
                {
                    ret = recv(fd, msg + strlen(msg), 1, 0);
                    if (ret <= 0)
                        break;
                }
                if (ret <= 0)
                {
                    remove_client(fd);
                    break;
                }
                extract_message(fd);
            }
        }
    }
    return (0);
}