#include <vector>
using std::vector;

bool is_digit(const char c) {
    return c >= '0' && c <= '9';
}

inline vector<string> split_whitespace(const string& str) {
    vector<string> result;
    std::istringstream stream(str);
    string word;

    while (stream >> word) { // ">>" citesc pana la primul spatiu
        result.push_back(word);
    }

    return result;
}
void trim(char* str) {
    //functia isspace testeaza daca este caracter care nu poate fi printat(vizibil)
    while (isspace(*str)) {
        str++;
    }

    char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        end--;
    }

    *(end + 1) = '\0';
}
string trim(const string& str) {
    const size_t start = str.find_first_not_of(" \t\n\r"); //pozitia primului caracter care este vizibil
    if (start == string::npos)
        return "";

    const size_t end = str.find_last_not_of(" \t\n\r"); //pozitia ultimului caracterc care este vizibil

    return str.substr(start, end - start + 1);
}

