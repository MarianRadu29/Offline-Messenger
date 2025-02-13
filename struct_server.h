#include<sqlite3.h>
#include<mutex>
#include <thread>
#include<vector>
using std::vector;
struct ClientInfo {
    std::thread clientThread;
    int clientSocket;

    ClientInfo(std::thread&& th, const int socket)
        : clientThread(std::move(th)), clientSocket(socket) {}
};
struct User {
    int clientSocket;
    string username;
    User(const int clientSocket, const string &username) {
        this->clientSocket = clientSocket;
        this->username = username;
    }
};
class Server {
    vector<ClientInfo> clients;
    vector<User> onlineUsers;
    std::mutex clientsMutex;
    std::mutex onlineUserMutex;
public:

    void addClient(std::thread&& th, int socket) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.emplace_back(std::move(th), socket);
    }

    void addOnlineUser(User& user) {
        std::lock_guard<std::mutex> lock(onlineUserMutex);
        if(std::find_if(onlineUsers.begin(), onlineUsers.end(),
        [&user](const User& u){return u.clientSocket==user.clientSocket;})==onlineUsers.end())
        {
            onlineUsers.emplace_back(user);
        }
    }

    bool userIsOnline(const string& username) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        return onlineUsers.end()!=std::find_if(onlineUsers.begin(), onlineUsers.end(),[&username](const User& user) {
            return user.username == username;
        });
    }

    vector<string> getOnlineUsers() {
        std::lock_guard<std::mutex> lock(clientsMutex);
        vector<string> result;
        //functie din biblioteca std::algorithm,
        //preiau din structura de date User doar username-ul
        std::transform(
           onlineUsers.begin(), onlineUsers.end(),
           std::back_inserter(result),
           [](const User& user) {
               return user.username;
           }
       );
        return result;
    }

    //sterg un client dupa socket-ul acestuia

    void removeClient(int socket) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        //std::cout << "Incep parcurgerea clientilor\n";

        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->clientSocket == socket) {
                //std::cout << "Am gasit clientul\n";

                // detasez threadul pentru a l lasa sa ruleze independent
                if (it->clientThread.joinable()) {
                    it->clientThread.detach();
                }

                clients.erase(it);

                return;
            }
        }
    }

    void removeOnlineUser(int socket) {
        std::lock_guard<std::mutex> lock(onlineUserMutex);
        const auto it = std::find_if(onlineUsers.begin(), onlineUsers.end(),
                       [&socket](const User& user) {
                           return user.clientSocket == socket;
                       });
        if (it != onlineUsers.end()) {
            onlineUsers.erase(it);
        }
    }

    void removeOnlineUser(const string& username) {
        std::lock_guard<std::mutex> lock(onlineUserMutex);
        const auto it = std::find_if(onlineUsers.begin(), onlineUsers.end(),
                       [&username](const User& user) {
                           return user.username == username;
                       });
        if (it != onlineUsers.end()) {
            onlineUsers.erase(it);
        }
    }

    string getOnlineUser(const int socket) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::find_if(onlineUsers.begin(), onlineUsers.end(),[&socket](const User& user) {
            return user.clientSocket == socket;
        });
        if(it==onlineUsers.end()) {
            return "";
        }
        return it->username;
    }
    int getOnlineUser(const string& username) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        const auto it = std::find_if(onlineUsers.begin(), onlineUsers.end(),[&username](const User& user) {
            return user.username == username;
        });
        if(it==onlineUsers.end()) {
            return -1;
        }
        return it->clientSocket;
    }

    //astept sa se termine toate threadurile
    void joinAllThreads() {
        for (auto& client : clients) {
            if (client.clientThread.joinable()) {
                client.clientThread.join();
            }
        }
    }

    // inchid toate socket-urile
    void closeAllSockets() {
        for (const auto& client : clients) {
            close(client.clientSocket);
        }
    }
};




const auto dbName = string("database.db");
vector<Message> fetchConversation(const string& user1, const string& user2) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    vector<Message> conversation;

    // deschid baza de date
    if (sqlite3_open(dbName.c_str(),&db) != SQLITE_OK) {
        std::cerr << "Nu s-a putut deschide baza de date: " << sqlite3_errmsg(db) << std::endl;
        return conversation;
    }

    // Interogare SQL
    const string query =
        "SELECT id, sender, recipient, message,reply "
        "FROM messages "
        "WHERE (sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?) "
        "ORDER BY id;";


    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Eroare la pregatirea interogarii: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return conversation;
    }

    // leg parametrii
    sqlite3_bind_text(stmt, 1, user1.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user2.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user2.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, user1.c_str(), -1, SQLITE_STATIC);

    //preiau mesajele din baza de date
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(stmt, 0);
        strncpy(msg.from, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)), sizeof(msg.from) - 1);
        strncpy(msg.to, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)), sizeof(msg.to) - 1);
        strncpy(msg.message, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)), sizeof(msg.message) - 1);
        const int r = sqlite3_column_int(stmt, 4);
        msg.reply_id = r;//r!=0 || r!=-1?r:-1;

        msg.from[sizeof(msg.from) - 1] = '\0';
        msg.to[sizeof(msg.to) - 1] = '\0';
        msg.message[sizeof(msg.message) - 1] = '\0';
        conversation.push_back(msg);

        std::cout << "ID: " << msg.id << ", From: " << msg.from << ", To: " << msg.to << ", Message: " << msg.message << ", Reply ID: " << msg.reply_id << std::endl;
    }


    if (sqlite3_finalize(stmt) != SQLITE_OK) {
        std::cerr << "Eroare la finalizarea interogrii: " << sqlite3_errmsg(db) << std::endl;
    }

    // inchid conexiunea la baza de date
    sqlite3_close(db);
    return conversation;
}

int insertMessageToDB(const char* dbName, const Message& msg) {
    sqlite3* db;
    sqlite3_stmt* stmt;

    std::cout<<"reply: "<<msg.reply_id<<'\n';
    int rc = sqlite3_open(dbName, &db);
    if (rc) {
        std::cerr << "Nu s-a putut deschide baza de date: " << sqlite3_errmsg(db) << std::endl;
        return rc;  //returnez codul de eroare
    }


    const auto insertSQL = "INSERT INTO messages (id, sender, recipient, message, reply) VALUES (?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Eroare la pregatirea statement-ului: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return rc;  // returnez codul de eroare
    }


    // leg valorile parametrilor la statement
    sqlite3_bind_int(stmt, 1, msg.id);
    sqlite3_bind_text(stmt, 2, msg.from, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, msg.to, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, msg.message, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, msg.reply_id);



    // execut statement-ul
    rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Eroare la executarea INSERT: " << sqlite3_errmsg(db) << std::endl;
    }


    // finalizez statement-ul si inchid baza de date
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc;
}

int insertUserToDB(const char* username, const char* password) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return rc;
    }
    const auto insertSQL = "INSERT INTO users(username, password) VALUES (?,?);";
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Eroare la pregatirea statement-ului: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return rc;  // returnez codul de eroare
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,2,password,-1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Eroare la executarea INSERT: " << sqlite3_errmsg(db) << std::endl;
    }


    // finalizez statement-ul si inchid baza de date
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc;
}
bool isUser(const char *username,const char* password) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    vector<Login> users;
    if (sqlite3_open(dbName.c_str(),&db) != SQLITE_OK) {
        std::cerr << "Nu s-a putut deschide baza de date: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    const string query =
        "SELECT username,password FROM users WHERE username = ? AND password = ?;";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Eroare la pregatirea interogarii: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,2,password, -1, SQLITE_STATIC);

    //daca interogarea de mai sus contine linii inseamna ca userul este in baza de date
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_finalize(stmt) != SQLITE_OK) {
            std::cerr << "Eroare la finalizarea interogrii: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
        return true;
    }

    if (sqlite3_finalize(stmt) != SQLITE_OK) {
        std::cerr << "Eroare la finalizarea interogrii: " << sqlite3_errmsg(db) << std::endl;
    }

    // inchid conexiunea la baza de date
    sqlite3_close(db);
    return false;
}

bool isUser(const char* username) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    vector<Login> users;
    if (sqlite3_open(dbName.c_str(),&db) != SQLITE_OK) {
        std::cerr << "Nu s-a putut deschide baza de date: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    const string query =
        "SELECT username,password FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Eroare la pregatirea interogarii: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    //daca interogarea de mai sus contine linii inseamna ca userul este in baza de date
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_finalize(stmt) != SQLITE_OK) {
            std::cerr << "Eroare la finalizarea interogrii: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
        return true;
    }

    if (sqlite3_finalize(stmt) != SQLITE_OK) {
        std::cerr << "Eroare la finalizarea interogrii: " << sqlite3_errmsg(db) << std::endl;
    }

    // inchid conexiunea la baza de date
    sqlite3_close(db);
    return false;
}