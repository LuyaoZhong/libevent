#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <event.h>
#include <string.h>
#include <unistd.h>

#define PORT        25342
#define SERVER_PORT 25341
#define BACKLOG     5
#define MEM_SIZE    1024

struct event_base* base;

struct sock_ev {
    struct event* read_ev;
    struct event* write_ev;
    char* buffer;
    int sock;
    int handler;
};

int flag = 3;

void release_sock_event(struct sock_ev* ev)
{
    event_del(ev->read_ev);
    free(ev->read_ev);
    free(ev->write_ev);
    free(ev->buffer);
    free(ev);

}

void on_write(int sock, short event, void* arg)
{
    char* buffer = (char*)arg;
    send(sock, buffer, strlen(buffer), 0);
    printf("send data:%ssize:%ld\n", buffer, strlen(buffer));
    free(buffer);
}

void my_memcpy_write(int sock, short event, void* arg)
{
    struct sock_ev* ev = (struct sock_ev*)arg;

    /* Check if DSA memcpy done */
    // handler.record_.get().is_success() ???

    // if (ev->handler) {
    if (flag > 0) {
        flag -= 1;
        printf("time wait\n");
        struct event* write_after_memcpy_ev = (struct event*)malloc(sizeof(struct event));
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        evtimer_set(write_after_memcpy_ev, my_memcpy_write, ev);
        event_base_set(base, write_after_memcpy_ev);
        event_add(write_after_memcpy_ev, &tv);
    }
    else {
        ev->write_ev = (struct event*)malloc(sizeof(struct event));
        event_set(ev->write_ev, ev->sock, EV_WRITE, on_write, ev->buffer);
        event_base_set(base, ev->write_ev);
        event_add(ev->write_ev, NULL);
    }

    printf("this is my_memcpy_write\n");
    return;

}

void on_read(int sock, short event, void* arg)
{
    int size;
    struct sock_ev* ev = (struct sock_ev*)arg;
    ev->buffer = (char*)malloc(MEM_SIZE);
    bzero(ev->buffer, MEM_SIZE);
    size = recv(sock, ev->buffer, MEM_SIZE, 0);

    printf("receive data:%ssize:%d\n", ev->buffer, size);

    if (size == 0) {
        release_sock_event(ev);
        close(sock);
        return;

    }

    /* DSA memcpy */
    // auto src_data = std::vector<uint8_t>{...};
    // auto dst_data = std::vector<uint8_t>(src_data.size());
    // auto handler = dsa::submit<dsa::software>(dsa::mem_move,
    //                                      dsa::make_view(src_data),
    //                                      dsa::make_view(dst_data));
    // ev->handler = handler


    ev->handler = 0;
    ev->sock = sock;
    struct event* write_after_memcpy_ev = (struct event*)malloc(sizeof(struct event));
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    evtimer_set(write_after_memcpy_ev, my_memcpy_write, ev);
    event_base_set(base, write_after_memcpy_ev);
    event_add(write_after_memcpy_ev, &tv);
}

void on_accept(int sock, short event, void* arg)
{
    struct sockaddr_in cli_addr;
    int newfd, sin_size;
    struct sock_ev* ev = (struct sock_ev*)malloc(sizeof(struct sock_ev));

    ev->read_ev = (struct event*)malloc(sizeof(struct event));
    sin_size = sizeof(struct sockaddr_in);

    newfd = accept(sock, (struct sockaddr*)&cli_addr, &sin_size);
    event_set(ev->read_ev, newfd, EV_READ|EV_PERSIST, on_read, ev);
    event_base_set(base, ev->read_ev);
    event_add(ev->read_ev, NULL);
}


int main(int argc, char* argv[])
{
    struct sockaddr_in my_addr;
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    listen(sock, BACKLOG);

    struct event listen_ev;
    base = event_base_new();
    event_set(&listen_ev, sock, EV_READ|EV_PERSIST, on_accept, NULL);
    event_base_set(base, &listen_ev);
    event_add(&listen_ev, NULL);
    event_base_dispatch(base);

    return 0;

}
