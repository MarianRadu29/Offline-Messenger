#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include<thread>
#include <atomic>
#include<mutex>
#include "struct.h"
#include"struct_client.h"
using std::string;

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
#define PORT 8080

/* user-ul logat la messenger */
std::mutex userMutex;
auto USER = string();

/* perechea user-ului care converseaza user-ul */
std::mutex PairUserChatMutex;
auto PairUserChat = std::string();

/* conversatia dintre cei 2 useri din chat-ul deschis */
std::mutex conversationMutex;
std::vector<Message> conversation;

std::atomic<bool> stopThread(false);

void readFromServer(const int socket) {
    while (true) {
          Packet msg_receive_server;
          const ssize_t msg_size = read (socket, &msg_receive_server,sizeof(Packet));
          if (msg_size< 0)
          {
              stopThread.store(true);
              perror ("[client]Eroare la read() de la server.\n");
              break;
          }
          if(msg_size==0) {
              stopThread.store(true);
              break;
          }

          switch (msg_receive_server.typeMsg) {
                case LoginSuccess: {
                      std::lock_guard<std::mutex> lock(userMutex);
                      USER = std::string(msg_receive_server.buffer);
                      std::cout << "\033[4D";
                      printf("[%s] Logare cu succes pentru user-ul \"%s\".\n",USER.c_str(),USER.c_str());
                      std::cout << "\033[1;32m> \033[0m";
                      fflush(stdout);
                }
                break;
                case LoginFailed: {
                      std::cout << "\033[4D";//ma duc cu 4 pozitii la stanga deoarece am apasat de 2 orii enter(odata la username odata la password)
                      printf("[client] %s\n",msg_receive_server.buffer);
                      std::cout << "\033[1;32m> \033[0m";
                      fflush(stdout);
                }
                break;
                case RegisterSuccess: {
                      std::lock_guard<std::mutex> lock(userMutex);
                      USER = std::string(msg_receive_server.buffer);
                      std::cout << "\033[6D";
                      printf("[%s] Inregistrare s-a efectuat cu succes pentru user-ul \"%s\".\n",USER.c_str(),USER.c_str());
                      std::cout << "\033[1;32m> \033[0m";
                      fflush(stdout);
                  }
                break;
                case RegisterFailed:{
                    std::cout << "\033[6D";//ma duc cu 4 pozitii la stanga deoarece am apasat de 2 orii enter(odata la username odata la password)
                    printf("[client] User-ul exista deja in baza de date\n");
                    std::cout << "\033[1;32m> \033[0m";
                    fflush(stdout);
                }
                break;
                case OnlineUserList: {
                      std::lock_guard<std::mutex> lock(userMutex);
                      if (msg_receive_server.buffer[0]=='\0') {
                          std::cout << "\033[2K\033[0G";
                          printf("[%s] Nu exista utilizatori online.\n",USER.empty()?"client":USER.c_str());
                          std::cout << "\033[1;32m> \033[0m";
                          fflush(stdout);
                      }
                      else {
                          std::cout << "\033[2K\033[0G";// mut cursorul la coloana 2 din linia curenta
                          //printf("[%s] %s \n",USER.empty()?"client":USER.c_str(),msg_receive_server.buffer);
                          auto users = split_whitespace(msg_receive_server.buffer);
                          for (auto user: users)
                              std::cout<<'\t'<<user<<'\n';
                          std::cout << "\033[1;32m> \033[0m";
                          fflush(stdout);
                      }
                }
                break;
                case LogoutConfirm: {
                      std::lock_guard<std::mutex> lock(userMutex);
                      USER.clear();
                      std::lock_guard<std::mutex> lock2(PairUserChatMutex);
                      PairUserChat.clear();
                      std::lock_guard<std::mutex> lock3(conversationMutex);
                      conversation.clear();
                      printf("\033[2D[client]Delogarea s-a efectuat cu succes\n");
                      std::cout << "\033[1;32m> \033[0m";
                      fflush(stdout);
                }
                break;
                case SuccessTakePairUserChat: {
                      std::lock_guard<std::mutex> lock(PairUserChatMutex);
                      PairUserChat = std::string(msg_receive_server.buffer);
                      Packet msg;
                      while(true) {
                        if (read (socket, &msg,sizeof(Packet)) < 0) {
                          perror ("[client]Eroare la read() de la server.\n");
                          return;
                        }
                        if (msg.typeMsg == TakeMessageConversation) {
                          //std::cout<<msg.buffer<<std::endl;
                          std::lock_guard<std::mutex> lock(conversationMutex);
                          conversation.push_back(Message::from_string(std::string(msg.buffer)));
                        }
                        else break;
                      }
                      std::cout<<"\033[2K\033[0G["<<USER<<"] Chat-ul cu "<<msg.buffer<<" s-a deschis\n\033[1;32m> \033[0m";
                    fflush(stdout);
                }
                break;
                case FailedTakePairUserChat: {
                    printf("%s\n",msg_receive_server.buffer);
                }
                break;
                case ServerSendToUserMessage: {
                      //primesc mesajul de la server,treb sa verific daca am chatul deschis ca sa l afisez
                      //trebuie sa l introduc in vectorul de conversatii daca chatul e open
                      auto msg = Message::from_string(std::string(msg_receive_server.buffer));
                      if(PairUserChat==string(msg.from)) {
                        std::lock_guard<std::mutex> lock(conversationMutex);
                        conversation.push_back(msg);
                          //\033[34;43m" schimb culoare in terminal a mesajului
                        std::cout << "\033[2D"<<msg.from<<": \033[34;43m"<<msg.message<<"\033[0m"<<"\n\033[1;32m> \033[0m";
                        fflush(stdout);
                      }
                }
                break;
                case ServerSendToUserReplyMessage:{
                  auto msg = Message::from_string(std::string(msg_receive_server.buffer));
                  if(PairUserChat==string(msg.from)) {
                    //std::cout<<PairUserChat<<": "<<msg.message<<std::endl;
                    std::lock_guard<std::mutex> lock(conversationMutex);
                    conversation.push_back(msg);
                    std::cout << "\033[2DReply:"<<
                        (!strcmp(conversation[msg.reply_id-1].from,PairUserChat.c_str())?"\033[34;43m":"")<<
                            conversation[msg.reply_id-1].message<<
                                (!strcmp(conversation[msg.reply_id-1].from,PairUserChat.c_str())?"\033[0m":"")
                      <<"  "<<msg.from<<": \033[34;43m"<<msg.message<<"\033[0m"<<"\n\033[1;32m> \033[0m";
                      fflush(stdout);
                  }
                  //afisez ca mai sus mesajul primit + mesajul reply
                }
                break;
                case ConfirmCloseClient: {
                    //std::cout<<"ConfirmCLoseClient\n";
                    return;
                }
                default: {
                    printf ("[client]Ceva neprevazut\n");
                }
          };
    }
}

void sendToServer(const int socket) {
      auto buf = string();
      while(!stopThread) {
          //fflush (stdout);
          if (!(buf=="register" && USER.empty())) {
              std::cout << "\033[1;32m> \033[0m";
          }
        //in codul normal nu avea if ul de mai sus,imi ramanea un enter in buffer stdout la register si mi-l punea la urmatoarea executie while dupa afisadu-mi "> > " la prompt
          //std::cout << "\033[1;32m> \033[0m";
          buf.clear();
          std::getline(std::cin, buf);
          buf = trim(buf);
          if (buf.empty()) {
            continue;
          }
          if (is_digit(buf[0])) {//reply msg
                if(USER.empty()) {
                    std::cout<<"Nu sunteti autentificat!!\n";
                    continue;
                }
                if(PairUserChat.empty()) {
                    std::cout<<"Nu aveti chat-ul deschis!!\n";
                    continue;
                }
                if (std::count(buf.begin(), buf.end(), '\"')!=2) {
                      std::cout<<"Comanda invalida\n";
                      continue;
                }
                ssize_t m_size =  0;
                while(is_digit(buf[m_size])) m_size++;

                const int nr_reply_msg = atoi(buf.substr(0,m_size).c_str());
                if(nr_reply_msg==0) {
                    std::cout<<"ID reply invalid\n";
                    continue;
                }
                if (conversation.size()>=nr_reply_msg) {
                      auto msg = Message(static_cast<int>(conversation.size()+1),USER.c_str(),PairUserChat.c_str(),buf.substr(m_size+1,buf.size()-m_size-2).c_str(),nr_reply_msg);
                      conversation.push_back(msg);
                      auto send_msg = Packet(UserSendReplyMessage,msg.to_string().c_str());
                          if( -1==write(socket, &send_msg, sizeof(Packet))) {
                              perror("[client]Eroare la write() de socket.\n");
                              return;
                          }
                }
                else {
                  std::cout<<"Conversatia contine mai putine mesaje("
                          <<conversation.size()
                          <<") decat numarul dat de dvs.("
                          <<nr_reply_msg<<")\n";
                }
                continue;
          }
          if(buf[0]=='"'&& buf[buf.size()-1]=='"') {
            if(USER=="") {
              std::cout<<"Nu sunteti autentificat!!\n";
              continue;
            }
            if(PairUserChat==string()) {
                std::cout<<"Nu aveti chat-ul deschis!!\n";
              continue;
            }
            std::lock_guard<std::mutex> lock(conversationMutex);
            buf = buf.substr(1, buf.size()-2);//elimin ghilimelele
            auto msg = Message(static_cast<int>(conversation.size()+1),USER.c_str(),PairUserChat.c_str(),buf.c_str());
            conversation.push_back(msg);
            //std::cout<<USER<<": "<<msg.message<<std::endl;
            Packet msg_send(UserSendMessage,msg.to_string().c_str());
            if(write(socket,&msg_send,sizeof(Packet))<=0) {
              perror ("[client]Eroare la write() spre server.\n");
              return;
            }
            continue;
          }
          auto args = split_whitespace(buf);
          if(args[0]=="exit") {
            if(args.size()>1 && args[1]=="chat") {
              std::lock_guard<std::mutex> lock(PairUserChatMutex);
              printf("[%s]Am iesit din conversatia cu %s\n",USER.c_str(),PairUserChat.c_str());
              conversation.clear();
              PairUserChat.clear();
            }
            else {
              const Packet msg(CloseClient,"");
              write(socket,&msg,sizeof(Packet));
              break;
            }
          }
          else if(args[0]=="login") {
               std::lock_guard<std::mutex> lock(userMutex);
                if(USER!=std::string()) {
                  printf("[%s]Sunteti deja autentificat\n",USER.c_str());
                  continue;
                }
                Login login;
                printf ("[client]Username: ");
                std::cin>>login.username;
                trim(login.username);
                printf ("[client]Password: ");
                std::cin>>login.password;
                trim(login.password);

                Packet msg(LoginData,login.to_string().c_str());
                if (write (socket,&msg,sizeof(msg)) <= 0) {
                  perror ("[client]Eroare la write() spre server.\n");
                  return;
                }
          }
          else if (args[0] == "onlineuser") {
              std::lock_guard<std::mutex> lock(userMutex);
              Packet msg(RequestOnlineUserList, USER.c_str());
              if (write(socket, &msg, sizeof(msg)) <= 0) {
                  perror("[client]Eroare la write() spre server.\n");
                  return;
              }
          } else if (args[0] == "register") {
              if (USER != std::string()) {
                  printf("[%s]Sunteti deja autentificat\n", USER.c_str());
                  continue;
              }
              Login input;
              printf("[client]Username: ");
              std::cin >> input.username;
              trim(input.username);
              printf("[client]Password: ");
              std::cin >> input.password;
              trim(input.password);
              printf("[client]Confirm password: ");
              auto aux = string();
              std::cin >> aux;
              trim(aux);
              string aux2(input.username);

              if (std::any_of(aux.begin(), aux.end(),
                              [](const char c) {
                                  return std::isalpha(c);
                              })) {
                  std::cout << "\nUsername-ul tau trebuie sa contina cel putin o litera\n";
                  continue;
              }
              if (std::count_if(aux2.begin(), aux2.end(), [](const char c) {
                          return std::isalnum(c) || c == '_';
                      }) != string(input.username).size()) {
                      std::cout << "\nUsername-ul trebuie sa contina doar litere,cifre si '_'\n";
                      continue;
                      }
              if (aux != input.password) {
                      std::cout << "\nConfirmarea parolei nu corespunde cu parola introdusa.\n";
                      continue;
                  }


              Packet msg(RegisterData, input.to_string().c_str());
              if (write(socket, &msg, sizeof(msg)) <= 0) {
                  perror("[client]Eroare la write() spre server.\n");
                  return;
              }

          } else if (args[0] == "logout") {
              if (USER == std::string()) {
                  printf("Utilizatorul nu este logat\n");
                  continue;
              }
              Packet send_msg(LogoutRequest, USER.c_str());
              if (write(socket, &send_msg, sizeof(Packet)) <= 0) {
                  perror("[client]Eroare la write() spre server.\n");
                  return;
              }

          } else if (args[0] == "chat") {

              std::lock_guard<std::mutex> lock(PairUserChatMutex);
              std::lock_guard<std::mutex> lock2(userMutex);
              if (USER.empty()) {
                  printf("[client]Nu sunteti inca logat pentru a forma un chat.\n");
                  continue;
              }
              if (!PairUserChat.empty()) {
                  printf("[%s]Deja sunteti intr-un chat.\n", USER.c_str());
                  continue;
              }
              if (args[1] == USER) {
                  std::cout << "Nu poti deschide o conversatie cu tine=))\n";
                  continue;
              }
              Packet msg_send(RequestPairUserChat, args[1].c_str());
              if (write(socket, &msg_send, sizeof(Packet)) <= 0) {
                  perror("[client]Eroare la write() spre server.\n");
                  return;
              }
          } else if (args[0] == "help") {
              std::cout << "\tregister\t-> Iti creezi un cont nou,daca username-ul nu se afla in baza de date,astfel "
                           "inregistrarea pica;\n";
              std::cout << "\tlogin\t-> Treci prin procesul de logare pentru a te conecta la messenger;\n";
              std::cout << "\tlogout\t-> Te deloghezi de la utilizatorii conectatii la messenger;\n";
              std::cout << "\tonlineuser\t-> Afiseaza o lista cu utilizatorii conectatii in momentul apelului comenzii "
                           "la messenger;\n";
              std::cout << "\tchat username\t-> Deschide chat-ul cu un utilizator din sistem daca exista;\n";
              std::cout << "\texit [chat]\t-> Inchide clientul sau iese din chat,depinde daca s-a introduc optiunea "
                           "chat;\n";
              std::cout << "\t\"message\"\t-> Trimit mesajul la utilizatorul la care am chatul deschis, daca este "
                           "deschis;\n";
              std::cout << "\t<nr>\"message\"\t-> Trimit replay cu un anumit mesaj de pe chat la utilizatorul la care "
                           "am chatul deschis,\n";
              std::cout << "\t\t\tdaca este deschis si daca nr este valid;\n";

          } else if (args[0] == "history") {
              if (USER == std::string()) {
                  printf("Utilizatorul nu este logat\n");
                  continue;
              }
              if (PairUserChat == std::string()) {
                  printf("Chat-ul nu este deschis\n");
                  continue;
              }
              if (conversation.empty()) {
                  printf("Chat-ul este gol\n");
                  continue;
              }
              for (const auto &msg: conversation) {
                  std::cout << '[' << msg.id << "] ";
                  if (msg.from == USER) {
                      std::cout << USER << ": " << msg.message;
                      if (msg.reply_id == -1) {
                          std::cout << "\n";
                      } else {
                          std::cout << " (reply to [" << msg.reply_id << "]: ";
                          if (USER != conversation[msg.reply_id - 1].to) {
                              std::cout << conversation[msg.reply_id - 1].message;
                          } else {
                              std::cout << "\033[34;43m" << conversation[msg.reply_id - 1].message << "\033[0m";
                          }
                          std::cout << ")\n";
                      }
                  } else {
                      std::cout << msg.from << ": \033[34;43m" << msg.message << "\033[0m";
                      if (msg.reply_id == -1) {
                          std::cout << "\n";
                      } else {
                          std::cout << " (reply to [" << msg.reply_id << "]: ";
                          if (conversation[msg.reply_id - 1].from == USER) {
                              std::cout << conversation[msg.reply_id - 1].message;
                          } else {
                              std::cout << "\033[34;43m" << conversation[msg.reply_id - 1].message << "\033[0m";
                          }
                          std::cout << ")\n";
                      }
                  }
              }
          } else {
              std::cout << "Comanda invalida\n";
          }

      }//final while
}

int main () {
  int sd;			// descriptorul de socket
  sockaddr_in server;	// structura folosita pentru conectare

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1){
    perror ("Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  {
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = htonl (INADDR_ANY);//by default 127.0.0.1
    /* portul de conectare */
    server.sin_port = htons (PORT);
  }

  if (connect (sd, reinterpret_cast<sockaddr *>(&server),sizeof (sockaddr)) == -1){
    perror ("[client]Eroare la connect().\n");
    return errno;
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////


  auto t = std::thread(sendToServer, sd);

  readFromServer(sd);
  //ori opresc threadul fortat daca serverul s-a inchis fortat,
  //ori astept sa se apese enter si apoi sa inchid corespunzator programul
  if (!stopThread)
      t.join();

  close(sd);
  std::cout<<"Am inchis clientul!!!\n";
}





//compilare g++ server.cpp -o server -lsqlite3,respectiv si pt client