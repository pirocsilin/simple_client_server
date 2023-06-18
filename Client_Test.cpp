#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <winsock2.h>
#include "sqlite3.h"
#define BUFFER_SIZE 512
using namespace std;

string getDateTime() {
    char date_time[128];
    time_t t = time(nullptr);
    tm* now = localtime(&t);
    strftime(date_time, sizeof(date_time), "%d.%m.%Y %X", now);
    return date_time;
}

int callBack(void* notUsed, int colCount, char** columns, char** colNames) {
    cout << "   ";
    for (int i = 0; i < colCount; i++) {
        cout << " " << columns[i];
    }
    cout << endl;
    return 0;
}

class DB {
private:
    sqlite3* db;
    char* err = 0;
    int result;

    void Execute(const char* command, int print = 0) {
        if (print)
            result = sqlite3_exec(db, command, callBack, 0, &err);
        else
            result = sqlite3_exec(db, command, 0, 0, &err);
        if (result != 0) {
            cout << "ERROR_DB: " << err << endl;
            closeDB();
        }
    }

public:
    void closeDB() { sqlite3_close(db); exit(-1); }
    DB(const char* name) {
        result = sqlite3_open(name, &db);
        if (result != 0) {
            cout << "ERROR_DB: " << sqlite3_errmsg(db) << endl;
            closeDB();
        }
        else {
            Execute("CREATE TABLE IF NOT EXISTS history "
                "(date_time TEXT, message TEXT);");
        }
    }

    void Insert(string msg) {
        char sql[200];
        sprintf(sql, "INSERT INTO history (date_time, message) "
            "VALUES('%s', '%s')", getDateTime().c_str(), msg.c_str());
        Execute(sql);
    }

    void printHistory() { Execute("SELECT * FROM history", 1); }

    void clearTable() { Execute("DELETE FROM history"); }
};

DB base("server.db");

// Функция запускается в отдельном потоке и в бесконечном цикле ожидает входящее сообщение от сервера.
void ServerRead(SOCKET serverSocket) {
    char msg[BUFFER_SIZE];
    string Header = "[SERVER] ";
    while (true) {
        if (recv(serverSocket, msg, sizeof(msg), NULL) == -1) {
            cout << "Server closed connection!" << endl;
            base.closeDB();
        }
        cout << getDateTime() << ' ' << Header + msg << endl;
        base.Insert(Header + msg);
    }
}
/* В основном потоке сначала пытаемя установить связь с сервером, если сервер недоступен - exit, 
   иначе запускаем в отдельном потоке ServerRead() для получения и вывода сообщений с сервера 
   затем клиент в бесконечном цикле может отправлять сообщения на сервер. Все сообщения пишутся в БД*/
int main() {

    cout << "\t>> SERVER <<\n" << endl;

    WSAData wsaData;
    WORD Version = MAKEWORD(2, 1);              
    if (WSAStartup(Version, &wsaData) != 0) {   
        cout << "WSAStartup ERROR!" << endl;
        exit(-1);
    }

    SOCKADDR_IN addr;                               
    int SADDR_size = sizeof(addr);                  
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  
    addr.sin_port = htons(1111);                    
    addr.sin_family = AF_INET;                      

    // Создаем сокет для соединения с сервером
    SOCKET Connection = socket(AF_INET, SOCK_STREAM, NULL);

    // Пытаемся подключиться к серверу
    int rezConnect = connect(Connection, (SOCKADDR*)&addr, sizeof(addr));
    if (rezConnect != 0) {
        cout << "ERROR [connect to server]" << endl;
        exit(-1);
    }
    cout << "Connecting is sucess!" << endl;

    // Запускаем поток для принятия сообщений от сервера
    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ServerRead, LPVOID(Connection), NULL, NULL);

    char msg[BUFFER_SIZE];
    while (true) {
        memset(msg, '\0', BUFFER_SIZE);
        cin.getline(msg, BUFFER_SIZE);
        if (strcmp(msg, "end") == 0) {
            base.closeDB();
        }
        else if (strcmp(msg, "his") == 0){
            base.printHistory();
        }
        else if (strcmp(msg, "del") == 0) {
            base.clearTable();
        }
        else if (strcmp(msg, "cls") == 0) {
            system("cls");
            cout << "\t>> SERVER <<\n" << endl;
        }
        else {
            send(Connection, msg, BUFFER_SIZE, NULL);
            base.Insert((string)"[MY] " + msg);
        }
    }
    return 0;
}
