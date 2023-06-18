#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <winsock2.h>
#include <string>
#include <map>
#include "sqlite3.h"
#define BUFFER_SIZE 512
#define MAX_CLIENTS 100

using namespace std;

// Функция возвращает текущую дату и время в формате 14.06.2023 18:12:56
string getDateTime() {
    char date_time[128];
    time_t t = time(nullptr);
    tm* now = localtime(&t);
    strftime(date_time, sizeof(date_time), "%d.%m.%Y %X", now);
    return date_time;
}

// Функция вызывается для каждой полученной строки из БД командой SELECT
int callBack(void* notUsed, int colCount, char** columns, char** colNames) {
    cout << "   ";
    for (int i = 0; i < colCount; i++) {
        cout << " " << columns[i];
    }
    cout << endl;
    return 0;
}

// Класс для работы с базой данных
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
    DB(const char* name){
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

// Словарь, для хранения всех активных соединений с клиентами
map<SOCKET, SOCKADDR_IN> ClientsMAP;

// Объект для манипуляции БД
DB base("server.db");


/* На каждого подключившегося клиента создается отдельный поток ClientRead() для принятия
   от него сообщений. При отключении клиента, "его" поток завершается */
void ClientRead(SOCKET clientSocket) {
    char msg[BUFFER_SIZE];
    string ClientID = "[U_"+to_string(clientSocket)+"] ";
    while (true) {
        // Если клиент не разорвал соединение, ожидаем msg
        if (recv(clientSocket, msg, sizeof(msg), NULL) == -1) {
            cout << getDateTime() + ' ' + ClientID + "Client closed connection!" << endl;
            // Если разрыв - удаляем адресную информацию из словаря
            ClientsMAP.erase(clientSocket);
            break;
        }
        else {
            cout << getDateTime() << ' ' << ClientID + msg << endl;
            base.Insert(ClientID + msg);
        }
    }
}

/* Функция запускается в отдельном потоке и позволяет оператору отправлять сообщения 
   клиентам, а так же выполнять различные команды: очистка консоли, просмотр БД, очистка БД и т.д.*/
void ServerWrite() {
    char msg[BUFFER_SIZE];
    string Header = "[SERVER] ";
    while (true) {
        memset(msg, '\0', BUFFER_SIZE);
        cin.getline(msg, BUFFER_SIZE);
        if (strcmp(msg, "end") == 0) {
            base.closeDB();
        }
        else if (strcmp(msg, "his") == 0) {
            base.printHistory();
        }
        else if (strcmp(msg, "cls") == 0) {
            system("cls");
            cout << "\t>> SERVER <<\n" << endl;
        }
        else if(strcmp(msg, "del") == 0){
            base.clearTable();
        }
        else {
            SOCKET ClientID = atoi(msg);
            if (ClientID && ClientsMAP.count(ClientID)) {
                int pos = 0;
                while (msg[pos] < size(msg)) {
                    if (msg[pos] >= '0' && msg[pos] <= '9') pos++;
                    else { pos++; break; }
                }
                send(ClientID, msg + pos, BUFFER_SIZE, NULL);
                base.Insert(Header + msg);
            }
            else
                cout << "Client " << ClientID << " not found!" << endl;
        }
    }
}

/* В основном потоке сначала производим запуск сервера, потом запуск ServerWrite(), далее 
   в бесконечном цикле ожидаем подлючение клиентов. На каждого нового клиента, запускаем ClientRead()
   и добвляем его адресную информацию в словарь ClientsMAP. Эта информация будет использована для
   отправки клиенту сообщения. В данной системе, id клиента - это номер "его" сокета на стороне сервера */
int main(){
    
    cout << "\t>> SERVER <<\n" << endl;
    
    WSAData wsaData;
    WORD Version = MAKEWORD(2, 1);              // Запрашиваем версию библиотеки winsock
    if (WSAStartup(Version, &wsaData) != 0) {   // Загружаем версию библиотеки. Сохраняем параметры в структуру wsaData
        cout << "WSAStartup ERROR!" << endl;
        exit(-1);
    }
    
    SOCKADDR_IN addr;                               // Заполняем структуру для хранения адреса сокета
    int SADDR_size = sizeof(addr);                  // Размер структуры
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // IP адрес хоста
    addr.sin_port = htons(1111);                    // Порт - идентификатор программы
    addr.sin_family = AF_INET;                      // Семейство протоколов

    // Создаем слушающий сокет
    SOCKET sListen = socket(AF_INET, SOCK_STREAM, NULL);

    // Привязываем адрес и порт к сокету sListen
    bind(sListen, (SOCKADDR*)&addr, sizeof(addr));

    // Запускаем прослушивание порта в ожидании подключения клиента
    listen(sListen, MAX_CLIENTS);    // SOMAXCONN

    // Запускаем поток для отправки сообщений клиентам
    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ServerWrite, NULL, NULL, NULL);

    char msg[BUFFER_SIZE] = "Hello, client!";
    while (true) {
        // БЛОКИРУЮЩАЯ ФУНКЦИЯ! Ожидаем подключения нового клиента
        SOCKET newClient = accept(sListen, (SOCKADDR*)&addr, &SADDR_size);

        if (newClient == 0) {  // Если клиент не смог подключиться к сервере
            cout << "ERROR [new connectid]" << endl;
        }
        else {
            cout << "[U_" + to_string(newClient) + "] "+"Client Connected" << endl;
            send(newClient, msg, BUFFER_SIZE, NULL);
            ClientsMAP[newClient] = addr;

            // Запускаем поток для принятия сообщений от клиента
            CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientRead, LPVOID(newClient), NULL, NULL);
        }
    }
    return 0;
}