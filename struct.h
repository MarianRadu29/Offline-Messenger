#include "json.hpp"
using std::string;
using json = nlohmann::json;

#define MAXSIZEMSG 4096


enum TypeMsg:int{
    Nothing = -1,//by default
    //tipurile de pachete pe care le trasmite clientul serverului
    LoginData =0,
    RegisterData,
    LogoutRequest,
    RequestOnlineUserList,
    RequestPairUserChat,
    UserSendMessage,
    UserSendReplyMessage,
    CloseClient,
    //tipurile de pachete pe care le trasmite serverul clientului
    LoginSuccess,
    LoginFailed,
    RegisterSuccess,
    RegisterFailed,
    LogoutConfirm,
    OnlineUserList,
    SuccessTakePairUserChat,
    FailedTakePairUserChat,
    TakeMessageConversation,
    FinishTakeMessageConversation,
    ServerSendToUserMessage,
    ServerSendToUserReplyMessage,
    ConfirmCloseClient,
};

struct Packet{
    TypeMsg typeMsg;
    char buffer[MAXSIZEMSG]{};
    explicit Packet(const string& str) {
        (*this) = from_string(str);
    }
    explicit Packet(const TypeMsg typeMsg,const char * buf) {
        this->typeMsg = typeMsg;
        strcpy(this->buffer,buf);
    }
    Packet(): typeMsg(TypeMsg::Nothing),buffer{}{}
    string to_string() const {
        json j;
        j["typeMsg"] = typeMsg;
        j["buffer"] = buffer;
        return j.dump();  // convertesc JSON-ul intr-un string
    }
    static Packet from_string(const string& str) {
        json j = json::parse(str);
        Packet msg;
        msg.typeMsg = j["typeMsg"].get<TypeMsg>();
        strcpy(msg.buffer, j["buffer"].get<string>().c_str());
        return msg;
    }
};

struct Login {
    char username[20];
    char password[20];

    //"serializez" si o fac in string
    string to_string() const {
        json j;
        j["username"] = username;
        j["password"] = password;
        return j.dump();  // convertesc JSON-ul intr-un string
    }

    //deserializez
    static Login from_string(const string& str) {
        json j = json::parse(str);
        Login login;
        strcpy(login.username, j["username"].get<string>().c_str());
        strcpy(login.password, j["password"].get<string>().c_str());
        return login;
    }
};

struct Message {
    int id;
    char from[20];
    char to[20];
    char message[MAXSIZEMSG];
    int reply_id;

    Message() {
        id = -1;
        strcpy(this->from,"");
        strcpy(this->to,"");
        strcpy(this->message,"");
        reply_id = -1;
    }
    Message(const int id,const char *from,const char *to,const char *message, const int reply_id = -1) {
        this->id = id;
        strcpy(this->from,from);
        strcpy(this->to,to);
        strcpy(this->message,message);
        this->reply_id = reply_id;
    }
    Message(const Message& other) noexcept
        : id(other.id),reply_id(other.reply_id) {
        // copiez datele din 'other' in acest obiect
        std::strncpy(from, other.from, sizeof(from));
        std::strncpy(to, other.to, sizeof(to));
        std::strncpy(message, other.message, sizeof(message));

        from[sizeof(from) - 1] = '\0';
        to[sizeof(to) - 1] = '\0';
        message[sizeof(message) - 1] = '\0';
    }

    string to_string() const {
        json j;
        j["id"] = id;
        j["from"] = from;
        j["to"] = to;
        j["message"] = message;
        j["reply_id"] = reply_id;
        return j.dump();  // convertesc JSON-ul intr-un string

    }
    static Message from_string(const string& str) {
        json j = json::parse(str);
        Message msg;
        msg.id = j["id"].get<int>();
        msg.reply_id = j["reply_id"].get<int>();
        strcpy(msg.from, j["from"].get<string>().c_str());
        strcpy(msg.to, j["to"].get<string>().c_str());
        strcpy(msg.message, j["message"].get<string>().c_str());
        return msg;
    }
};

