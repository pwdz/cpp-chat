#include <string>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <list> 
#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdbool.h>

using namespace std;

// #define PORT 8080
#define MESSAGE_SIZE 1024

struct client
{
    string name;
    pthread_t tid;
    int socket;
};
struct group
{
    string group_id;
    vector<client*> members;
};

group groups[10];
int groups_count = 0;
   
void checkkErr(int statusCode, string msg);
void *handle_client(void *client_sock);
void handle_event(client *client, string command, string input);
void send_msg(client *client, string msg);
void set_name(client *client, string input);
void join_group(client *client,string input);
int find_group(string group_id);
void send_to_gp(client *client, string msg, string group_id, bool is_leave_msg);
void leave_gp(client *client, string group_id);

int main(int argc, char *argv[]) {

    if(argc < 2){
        puts("[ERROR]Not enough args");
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd;

    cout << "[SERVER] starting server ..." << "\n";

    struct sockaddr_in address;
 
    server_fd = socket(AF_INET, SOCK_STREAM, 0);//0 -> choose protocol automatically

    address.sin_family = AF_INET;//IP_V4
    address.sin_addr.s_addr = INADDR_ANY;//addresses to accept any incoming messages
    address.sin_port = htons(port);

    const int addrlen = sizeof(address);
    
    checkkErr(bind(server_fd, (struct sockaddr *)&address, sizeof(address)), "Binding failed");

    checkkErr(listen(server_fd, 3), "Listen error");

    printf("[SERVER] Listen on %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    while (true){
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        checkkErr(new_socket, "accept"); 

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, handle_client, (void *)new_socket); 
        if (rc) {
            printf("[SERVER] Error:unable to create thread, %d\n", rc);
            exit(-1);
        }
    }
    return 0;
}    
void checkkErr(int statusCode, string msg){
    if(statusCode < 0){
        perror(msg.c_str());
        exit(EXIT_FAILURE);
    }
}
void *handle_client(void *client_sock){
    int client_socket = (int)(long)client_sock;

    char buffer[MESSAGE_SIZE];
    bzero(buffer, MESSAGE_SIZE);

    int byte_count;
    client client;
    client.name = "";
    client.socket = client_socket;
    client.tid = pthread_self();

    while(true){
        byte_count = read( client_socket , buffer, MESSAGE_SIZE); 
        if(byte_count > 0){
            string input(buffer);
            bzero(buffer, MESSAGE_SIZE);

            string command = input.substr(0, input.find(" "));
            input = input.substr(input.find(" ") + 1, input.size());

            handle_event(&client, command, input);
        }
    };
}
void handle_event(client *client, string command, string input){

    if(command == "/name"){
        set_name(client , input);
    }else if(command == "/join"){
        join_group(client, input);
    }else if(command == "/send"){
        string group_id = input.substr(0, input.find(" "));
        input = input.substr(input.find(" ") , input.size());
        send_to_gp(client, input, group_id, false);
    }else if(command == "/leave"){
        input = input.substr(input.find(" ") + 1, input.size());
        leave_gp(client, input);
    }else if(command == "/quit"){
        send_msg(client, "[FROM SERVER] Disconnected");
        cout << "[SERVER] User " << client->name << " disconnected form server";
        pthread_cancel(client->tid);
    }

}
void set_name(client *client, string input){
    cout << "[SERVER] '" << input << "' Joined the server. Socket:" << client->socket << "\n";
    client->name = input;
    send_msg(client, "[FROM SERVER] >>>>>>> Welcome "+ client->name + " <<<<<<<");
}
void join_group(client *client,string input){
    string group_id = input.substr(0, input.find(" "));
    
    int i;
    if((i = find_group(group_id)) != -1){
        if(!count(groups[i].members.begin(), groups[i].members.end(), client)){ //Element Not Found
            cout << "[SERVER] Adding '" << client->name << "' to group '" << group_id << "'\n"; 
            send_msg(client, "[FROM SERVER] You are added to " + group_id);
            groups[i].members.push_back(client);
        }else{
            cout << "[SERVER] " << client->name << " is already in group" << "\n";
            send_msg(client, "[FROM SERVER] You are already a member of group " + group_id);
        }
        return;
    }

    //If reaches here, means there is not such a group
    group group;
    group.group_id = group_id;
    group.members.push_back(client);
    groups[groups_count] = group;
    groups_count++;

    send_msg(client, "[FROM SERVER] Group " + group_id + " created.");
    cout << "[SERVER] Group '" << group_id << "' created by '" << client->name <<"'\n";
}
void send_msg(client *client, string msg){
    send(client->socket, msg.c_str(), strlen(msg.c_str()), 0);
}
void send_to_gp(client *client, string msg, string group_id, bool is_leave_msg){
    int index;
    string msg_generator = client->name;
    if((index = find_group(group_id)) != -1){
        if(is_leave_msg)
            msg_generator = "Group";
        cout << "[SERVER] Sending message from " + msg_generator +" to group "+ group_id + "\n";
        for(int i=0; i<groups[index].members.size(); i++){
            if(groups[index].members[i]->name != client->name){
                send_msg(groups[index].members[i], "[FROM SERVER]["+group_id+"]["+msg_generator+"]:"+msg);
                cout << "[SERVER][" << group_id << "][" << msg_generator << "]:" << msg << "\n";
            }
        }
    }else{
        send_msg(client, "[FROM SERVER][ERROR] Group "+group_id+" doesn't exist!");
        cout << "[SERVER][ERROR] Group " + group_id + " doesn't exist |Message from:" + client->name + "|\n";
    }
}
void leave_gp(client *client, string group_id){
    int index;
    if((index = find_group(group_id)) != -1){

        if(!count(groups[index].members.begin(), groups[index].members.end(), client)){ //Element Not Found
            send_msg(client, "[FROM SERVER][ERROR] You are not a member of group " + group_id);
            cout << "[SERVER][ERROR] User " << client->name << " is not a member of group " << group_id + "|due to leave request|\n";
        }else{
            send_to_gp(client, "user "+client->name+" left the group.", group_id, true);
            groups[index].members.erase(remove(groups[index].members.begin(), groups[index].members.end(), client), groups[index].members.end());
            send_msg(client, "[FROM SERVER] You left the group "+ group_id);
        }
    }else{
        send_msg(client, "[FROM SERVER][ERROR] Group "+group_id+" doesn't exist!");
        cout << "[SERVER][ERROR] Group " << group_id << " doesn't exist |Leave request from:" << client->name << "|\n";

    }
  

}
int find_group(string group_id){
    for(int i=0; i<groups_count; i++){
        if(groups[i].group_id == group_id){
            return i;
        }
    }
    return -1;
}