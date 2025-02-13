#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include <algorithm>
#include"struct.h"
#include"struct_server.h"
#include <thread>

using std::string;

#define PORT 8080


Server server;
/* codul de eroare returnat de anumite apeluri */
extern int errno;

std::vector<std::thread> threads; // Vector pentru a gestiona thread-urile

void handleClient(const int cl)//(int cl,int idThread)
{
    Packet msgReceive;

    while(true) {
        memset(&msgReceive,0,sizeof(Packet));
        const ssize_t s = read (cl, &msgReceive,sizeof(Packet));
        usleep(100);
        if(s==0) {
            std::cout<<"Client disconnected"<<std::endl;
            server.removeOnlineUser(cl);
            //std::cout<<"Am trecut de taskul removeOnlineUser\n";
            server.removeClient(cl);
            break;
        }
        if (s<0){
            std::cout<<"Client error"<<std::endl;
            break;
        }


        switch (msgReceive.typeMsg) {
            case LoginData: {
                        bool in_db = false;
                        bool is_online = false;
                        Login input = Login::from_string(std::string(msgReceive.buffer));

                        in_db = isUser(input.username,input.password);
                        is_online = server.userIsOnline(input.username);

                        Packet packet;
                        if(in_db  && !is_online) {
                            packet.typeMsg = LoginSuccess;
                            auto user = User(cl,string(input.username));
                            server.addOnlineUser(user);
                            strcpy(packet.buffer,input.username);
                        }
                        else {
                            packet.typeMsg = LoginFailed;
                            if(is_online) {
                                strcpy(packet.buffer,"User-ul este deja conectat");
                            }else if(!in_db)
                                strcpy(packet.buffer,"Date invalide primite pentru autentificare!!");
                        }
                        //std::cout<<sendmsg.typeMsg<<'\n';
                        if (write (cl, &packet, sizeof(packet)) <= 0)
                        {
                            perror ("[Thread]Eroare la write() catre client.\n");
                        }
                    }
                    break;
            case RegisterData: {
                    auto input = Login::from_string(std::string(msgReceive.buffer));
                    Packet packet;

                    if(isUser(input.username)) {
                        // utilizatorul exista deja,treb dat mesaj de alreadyexists
                        packet = Packet(RegisterFailed, "");

                    } else {
                        // nu exista,il introduc in baza de data
                        insertUserToDB(input.username, input.password);
                        packet = Packet(RegisterSuccess, input.username);
                    }

                    if (write(cl, &packet, sizeof(packet)) <= 0) {
                        perror("[Thread]Eroare la write() catre client.\n");
                    }
            }
            break;
            case RequestOnlineUserList:{
                        std::string user_str;
                        for(auto user : server.getOnlineUsers()) {
                            if (strcmp(user.c_str(),msgReceive.buffer)) {
                                printf("user: %s \n",user.c_str());
                                user_str+=user_str.empty()?user:" " +user;
                            }
                        }
                        Packet sendmsg(OnlineUserList,user_str.c_str());
                        if (write (cl, &sendmsg, sizeof(sendmsg)) <= 0) {
                            perror ("[Thread]Eroare la write() catre client.\n");
                        }
                    }
                    break;
            case LogoutRequest: {
                        server.removeOnlineUser(string(msgReceive.buffer));
                        Packet msg_send(LogoutConfirm,"");
                        if (write (cl, &msg_send, sizeof(msg_send)) <= 0) {
                            perror ("[Thread]Eroare la write() catre client.\n");
                        }
                    }
                    break;
            case RequestPairUserChat: {
                        if(!isUser(msgReceive.buffer)) {
                            const auto sendmsg = Packet(FailedTakePairUserChat,"Utilizatorul cu care vreti sa conversatii nu exista");
                            if (write (cl, &sendmsg, sizeof(sendmsg)) <= 0) {
                                perror ("[Thread]Eroare la write() catre client.\n");
                            }
                        }
                        else {
                            const auto sendmsg = Packet(SuccessTakePairUserChat,msgReceive.buffer);
                            if (write (cl, &sendmsg, sizeof(sendmsg)) <= 0) {
                                perror ("[Thread]Eroare la write() catre client.\n");
                            }
                            std::pair<string,string> pairConversation;//first sunt eu,second este cel cu care am dat request pt chat
                            pairConversation.second=string(msgReceive.buffer);
                            pairConversation.first = server.getOnlineUser(cl);

                            std::vector<Message> history = fetchConversation(pairConversation.first.c_str(),pairConversation.second.c_str());

                            for(const auto &msg:history) {
                                Packet msg_send_client(TakeMessageConversation,msg.to_string().c_str());
                                if (write (cl, &msg_send_client, sizeof(msg_send_client)) <= 0) {
                                    perror ("[Thread]Eroare la write() catre client.\n");
                                    break;
                                }
                            }
                            Packet msg(FinishTakeMessageConversation,pairConversation.second.c_str());
                            if (write (cl, &msg, sizeof(msg)) <= 0) {
                                perror ("[Thread]Eroare la write() catre client.\n");
                            }
                        }
                    }
                    break;
            case UserSendMessage: {
                        auto msg = Message::from_string(string(msgReceive.buffer));
                        //std::cout<<insertMessageToDB(dbName.c_str(),msg)<<std::endl;//momentan nu testam daca se face cu succes,trebuie verificat cu SQLITE_DONE
                        insertMessageToDB(dbName.c_str(),msg);
                        auto to_user = string(msg.to);

                        if(server.userIsOnline(to_user)) {
                            const auto socket_ = server.getOnlineUser(to_user);
                            auto msg_send = Packet(ServerSendToUserMessage,msg.to_string().c_str());
                            if(write (socket_, &msg_send, sizeof(msg_send)) <= 0) {
                                perror ("[Thread]Eroare la write() catre client.\n");
                                return;
                            }
                        }
                        else {
                            //std::cout<<to_user<<" este offline\n";
                        }
                    }
                    break;
            case UserSendReplyMessage: {
                auto msg = Message::from_string(string(msgReceive.buffer));
                insertMessageToDB(dbName.c_str(),msg);
                auto to_user = string(msg.to);

                if(server.userIsOnline(to_user)) {
                    const auto socket_ = server.getOnlineUser(to_user);
                    auto msg_send = Packet(ServerSendToUserReplyMessage,msgReceive.buffer);
                    if(write (socket_, &msg_send, sizeof(msg_send)) <= 0) {
                        perror ("[Thread]Eroare la write() catre client.\n");
                        return;
                    }
                }
                else {
                    //std::cout<<to_user<<" este offline\n";
                }
            }
            break;
            case CloseClient: {
                const Packet msg(ConfirmCloseClient,"");
                write(cl, &msg, sizeof(msg));
                //serverul primeste mesaj de lungime 0 insemnand deconectarea clientului,acolo se face logica stergerii threadului
                 // server.removeOnlineUser(cl);
                 // server.removeClient(cl);
                //std::cout<<"Am trimis confirmcloseclient\n";
            }
            break;
            default: {
                printf("Tip de mesaj invalid");
            }
        };
    }
}


int main() {
    int serverSocket;
    sockaddr_in serverAddr, clientAddr;

    // creez socket-ul serverului
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Server] Eroare la crearea socket-ului");
        return 1;
    }

    // configurez adresa serverului
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // leg socket-ul la port
    if (bind(serverSocket, reinterpret_cast< sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
        perror("[Server] Eroare la bind()");
        close(serverSocket);
        return 1;
    }

    // pun serverul sa asculte conexiuni
    if (listen(serverSocket, 128) == -1) {
        perror("[Server] Eroare la listen()");
        close(serverSocket);
        return 1;
    }

    std::cout << "[Server] Serverul ruleaza pe portul " << PORT << "...\n";

    while (true) {
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);

        if (clientSocket < 0) {
            perror("Eroare la accept()");
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "Client conectat de la " << clientIP << ":" << ntohs(clientAddr.sin_port) << "\n";

        // creez un thread pentru client si-l adaug in lista de thread-uri
        std::thread clientThread(handleClient, clientSocket);
        server.addClient(std::move(clientThread), clientSocket);
    }

    std::cout<<"A iesit din while true\n";
    server.closeAllSockets();
    server.joinAllThreads();

    close(serverSocket);
    return 0;
}
