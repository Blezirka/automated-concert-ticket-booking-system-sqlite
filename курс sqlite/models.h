#pragma once
#include <string>
using namespace std;

// Класс пользователя системы
class User {
public:
    int    id;
    string login;
    string password;
    string fullName;   // ФИО
    string role;       // "admin" или "user"

    User() {
        id = 0;
        login = "";
        password = "";
        fullName = "";
        role = "user";
    }

    bool isAdmin() {
        return role == "admin";
    }
};

// Класс концерта (8 полей)
class Concert {
public:
    int    id;
    string artist;          // Исполнитель
    string title;           // Название концерта
    string concertDate;     // Дата проведения
    string venue;           // Место проведения
    double price;           // Цена билета
    int    totalSeats;      // Всего мест
    int    availableSeats;  // Свободных мест

    Concert() {
        id = 0;
        price = 0.0;
        totalSeats = 0;
        availableSeats = 0;
    }
};

// Класс билета
class Ticket {
public:
    int    id;
    int    concertId;
    int    userId;
    int    seatNumber;   // Номер места
    string status;       // "active" или "cancelled"
    string purchaseDate; // Дата покупки

    // Доп. поля для удобного вывода
    string concertTitle;
    string artistName;
    string userLogin;

    Ticket() {
        id = 0;
        concertId = 0;
        userId = 0;
        seatNumber = 0;
        status = "active";
    }
};
