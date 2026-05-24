#pragma once
#include "models.h"
#include <vector>
#include <sqlite3.h> // Вместо <libpq-fe.h>

// Глобальный указатель на базу данных SQLite
extern sqlite3* conn;

// Подключение и инициализация (connStr теперь принимает имя файла, например "database.db")
bool dbConnect(string connStr);
void dbInit();

// Пользователи
bool         dbLogin(string login, string password, User& outUser);
vector<User> dbGetAllUsers();
bool         dbAddUser(User u);
bool         dbUpdateUser(User u);
bool         dbDeleteUser(int id);

// Концерты
vector<Concert> dbGetAllConcerts(string orderBy = "id");
Concert         dbGetConcertById(int id);
vector<Concert> dbSearchByArtist(string artist);
vector<Concert> dbFilterByPrice(double maxPrice);
vector<Concert> dbGetAvailableConcerts();
bool            dbAddConcert(Concert c);
bool            dbUpdateConcert(Concert c);
bool            dbDeleteConcert(int id);

// Билеты
vector<Ticket> dbGetAllTickets();
vector<Ticket> dbGetMyTickets(int userId);
bool           dbBookTicket(int concertId, int userId, int seat);
bool           dbCancelTicket(int ticketId, int& outConcertId);
bool           dbDeleteTicket(int id);
void           dbPrintStatistics();