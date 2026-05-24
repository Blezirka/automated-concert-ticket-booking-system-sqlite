#define _HAS_STD_BYTE 0

#include "db.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <windows.h> // Теперь встанет без конфликтов

// 2. ОТКАЗЫВАЕМСЯ ОТ GLOBAL USING NAMESPACE STD
// Прописываем только то, что нужно для функций базы данных точечно:
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::stringstream;
using std::left;
using std::setw;
using std::fixed;
using std::setprecision;

sqlite3* conn = nullptr;

// === ФУНКЦИИ КОНВЕРТАЦИИ КОДИРОВОК (ТЕПЕРЬ БЕЗ КОНФЛИКТОВ) ===

// Из CP1251 (консоль/исходный код) в UTF-8 (для SQLite и DBeaver)
string toUTF8(const string& cp1251) {
    if (cp1251.empty()) return "";
    int wlen = MultiByteToWideChar(1251, 0, cp1251.c_str(), -1, nullptr, 0);
    wchar_t* wstr = new wchar_t[wlen];
    MultiByteToWideChar(1251, 0, cp1251.c_str(), -1, wstr, wlen);
    
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char* ustr = new char[ulen];
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, ustr, ulen, nullptr, nullptr);
    
    string result(ustr);
    delete[] wstr;
    delete[] ustr;
    return result;
}

// Из UTF-8 (из базы) в CP1251 (чтобы консоль не ломалась)
string toCP1251(const char* utf8) {
    if (!utf8 || strlen(utf8) == 0) return "";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    wchar_t* wstr = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wlen);
    
    int clen = WideCharToMultiByte(1251, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char* cstr = new char[clen];
    WideCharToMultiByte(1251, 0, wstr, -1, cstr, clen, nullptr, nullptr);
    
    string result(cstr);
    delete[] wstr;
    delete[] cstr;
    return result;
}

// =====================================

// Подключение к БД
bool dbConnect(string connStr) {
    int rc = sqlite3_open(connStr.c_str(), &conn);
    if (rc != SQLITE_OK) {
        cout << "Ошибка открытия/создания БД: " << sqlite3_errmsg(conn) << endl;
        sqlite3_close(conn);
        conn = nullptr;
        return false;
    }
    return true;
}

// Создание таблиц при первом запуске 
void dbInit() {
    char* errMsg = nullptr;

    const char* sqlUsers =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  login TEXT UNIQUE NOT NULL,"
        "  password TEXT NOT NULL,"
        "  full_name TEXT NOT NULL,"
        "  role TEXT NOT NULL DEFAULT 'user'"
        ");";

    const char* sqlConcerts =
        "CREATE TABLE IF NOT EXISTS concerts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  artist TEXT NOT NULL,"
        "  title TEXT NOT NULL,"
        "  concert_date TEXT NOT NULL,"
        "  venue TEXT NOT NULL,"
        "  price REAL NOT NULL,"
        "  total_seats INTEGER NOT NULL,"
        "  available_seats INTEGER NOT NULL"
        ");";

    const char* sqlTickets =
        "CREATE TABLE IF NOT EXISTS tickets ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  concert_id INTEGER REFERENCES concerts(id),"
        "  user_id INTEGER REFERENCES users(id),"
        "  seat_number INTEGER NOT NULL,"
        "  status TEXT NOT NULL DEFAULT 'active',"
        "  purchase_date TEXT NOT NULL"
        ");";

    sqlite3_exec(conn, sqlUsers, nullptr, nullptr, &errMsg);
    sqlite3_exec(conn, sqlConcerts, nullptr, nullptr, &errMsg);
    sqlite3_exec(conn, sqlTickets, nullptr, nullptr, &errMsg);

    // Проверяем администратора
    sqlite3_stmt* stmt;
    int adminCount = 0;
    if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM users WHERE role='admin';", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            adminCount = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (adminCount == 0) {
        // Запрос конвертируем в UTF-8 перед отправкой
        string insertAdmin = "INSERT INTO users (login, password, full_name, role) "
            "VALUES ('admin', 'admin', 'Администратор системы', 'admin');";
        sqlite3_exec(conn, toUTF8(insertAdmin).c_str(), nullptr, nullptr, &errMsg);
        cout << "  Создан администратор: логин=admin, пароль=admin\n";
    }

    // Проверяем тестовые концерты
    int concertCount = 0;
    if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM concerts;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            concertCount = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (concertCount == 0) {
        // Конвертируем блок кириллических тестовых данных в UTF-8
        string insertConcerts =
            "INSERT INTO concerts (artist, title, concert_date, venue, price, total_seats, available_seats) VALUES "
            "('Кай Ангел', 'Tusa VIP', '2026-08-20', 'Дворец спорта Минск', 35.00, 200, 200),"
            "('Темный Принц', 'Мрачный вечер', '2026-08-20', 'Клуб Арена', 25.00, 150, 150),"
            "('Сематари', 'Бензопила', '2026-09-01', 'Страшное дерево', 50.00, 400, 400),"
            "('DJ Арбуз', 'Электронная ночь', '2027-05-07', 'Клуб Папараць-Кветка', 20.00, 300, 300);";
        sqlite3_exec(conn, toUTF8(insertConcerts).c_str(), nullptr, nullptr, &errMsg);
        cout << "  Добавлены тестовые концерты.\n";
    }
}

// Авторизация
bool dbLogin(string login, string password, User& outUser) {
    string query = "SELECT id, login, password, full_name, role FROM users WHERE login=? AND password=?;";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // Логин и пароль обычно английские, но на случай русских символов тоже конвертируем
        sqlite3_bind_text(stmt, 1, toUTF8(login).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toUTF8(password).c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outUser.id = sqlite3_column_int(stmt, 0);
            outUser.login = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            outUser.password = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            outUser.fullName = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            outUser.role = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
            success = true;
        }
        sqlite3_finalize(stmt);
    }
    return success;
}

vector<User> dbGetAllUsers() {
    vector<User> users;
    string query = "SELECT id, login, password, full_name, role FROM users ORDER BY id;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User u;
            u.id = sqlite3_column_int(stmt, 0);
            u.login = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            u.password = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
            u.fullName = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            u.role = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
            users.push_back(u);
        }
        sqlite3_finalize(stmt);
    }
    return users;
}

bool dbAddUser(User u) {
    string check = "SELECT COUNT(*) FROM users WHERE login=?;";
    sqlite3_stmt* stmt;
    int count = 0;
    if (sqlite3_prepare_v2(conn, check.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, toUTF8(u.login).c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0) {
        cout << "  Логин '" << u.login << "' уже занят!\n";
        return false;
    }

    string query = "INSERT INTO users (login, password, full_name, role) VALUES (?, ?, ?, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, toUTF8(u.login).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toUTF8(u.password).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, toUTF8(u.fullName).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, toUTF8(u.role).c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool dbUpdateUser(User u) {
    string query = "UPDATE users SET login=?, password=?, full_name=?, role=? WHERE id=?;";
    sqlite3_stmt* stmt;
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, toUTF8(u.login).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toUTF8(u.password).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, toUTF8(u.fullName).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, toUTF8(u.role).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, u.id);

        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool dbDeleteUser(int id) {
    sqlite3_stmt* stmt;
    string q1 = "DELETE FROM tickets WHERE user_id=?;";
    if (sqlite3_prepare_v2(conn, q1.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    string query = "DELETE FROM users WHERE id=?;";
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

// Чтение концертов из выборки
static vector<Concert> readConcerts(sqlite3_stmt* stmt) {
    vector<Concert> list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Concert c;
        c.id = sqlite3_column_int(stmt, 0);
        c.artist = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        c.title = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        c.concertDate = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        c.venue = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        c.price = sqlite3_column_double(stmt, 5);
        c.totalSeats = sqlite3_column_int(stmt, 6);
        c.availableSeats = sqlite3_column_int(stmt, 7);
        list.push_back(c);
    }
    sqlite3_finalize(stmt);
    return list;
}

vector<Concert> dbGetAllConcerts(string orderBy) {
    if (orderBy != "id" && orderBy != "concert_date" && orderBy != "price ASC" &&
        orderBy != "artist ASC" && orderBy != "total_seats DESC" && orderBy != "available_seats DESC") {
        orderBy = "id";
    }
    string query = "SELECT id, artist, title, concert_date, venue, price, total_seats, available_seats FROM concerts ORDER BY " + orderBy + ";";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        return readConcerts(stmt);
    }
    return vector<Concert>();
}

Concert dbGetConcertById(int id) {
    string query = "SELECT id, artist, title, concert_date, venue, price, total_seats, available_seats FROM concerts WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        vector<Concert> list = readConcerts(stmt);
        if (!list.empty()) return list[0];
    }
    return Concert();
}

vector<Concert> dbSearchByArtist(string artist) {
    string query = "SELECT id, artist, title, concert_date, venue, price, total_seats, available_seats FROM concerts WHERE LOWER(artist) LIKE LOWER(?) ORDER BY artist;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        string pattern = "%" + artist + "%";
        sqlite3_bind_text(stmt, 1, toUTF8(pattern).c_str(), -1, SQLITE_TRANSIENT);
        return readConcerts(stmt);
    }
    return vector<Concert>();
}

vector<Concert> dbFilterByPrice(double maxPrice) {
    string query = "SELECT id, artist, title, concert_date, venue, price, total_seats, available_seats FROM concerts WHERE price<=? ORDER BY price;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, maxPrice);
        return readConcerts(stmt);
    }
    return vector<Concert>();
}

vector<Concert> dbGetAvailableConcerts() {
    string query = "SELECT id, artist, title, concert_date, venue, price, total_seats, available_seats FROM concerts WHERE available_seats>0 ORDER BY concert_date;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        return readConcerts(stmt);
    }
    return vector<Concert>();
}

bool dbAddConcert(Concert c) {
    string query = "INSERT INTO concerts (artist, title, concert_date, venue, price, total_seats, available_seats) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, toUTF8(c.artist).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toUTF8(c.title).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, toUTF8(c.concertDate).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, toUTF8(c.venue).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, c.price);
        sqlite3_bind_int(stmt, 6, c.totalSeats);
        sqlite3_bind_int(stmt, 7, c.totalSeats);

        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool dbUpdateConcert(Concert c) {
    string query = "UPDATE concerts SET artist=?, title=?, concert_date=?, venue=?, price=?, total_seats=?, available_seats=? WHERE id=?;";
    sqlite3_stmt* stmt;
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, toUTF8(c.artist).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toUTF8(c.title).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, toUTF8(c.concertDate).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, toUTF8(c.venue).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, c.price);
        sqlite3_bind_int(stmt, 6, c.totalSeats);
        sqlite3_bind_int(stmt, 7, c.availableSeats);
        sqlite3_bind_int(stmt, 8, c.id);

        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool dbDeleteConcert(int id) {
    sqlite3_stmt* stmt;
    string q1 = "DELETE FROM tickets WHERE concert_id=?;";
    if (sqlite3_prepare_v2(conn, q1.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    string query = "DELETE FROM concerts WHERE id=?;";
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

// Чтение билетов
static vector<Ticket> readTickets(sqlite3_stmt* stmt) {
    vector<Ticket> list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Ticket t;
        t.id = sqlite3_column_int(stmt, 0);
        t.concertId = sqlite3_column_int(stmt, 1);
        t.userId = sqlite3_column_int(stmt, 2);
        t.seatNumber = sqlite3_column_int(stmt, 3);
        t.status = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        t.purchaseDate = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        t.concertTitle = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
        t.artistName = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
        t.userLogin = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
        list.push_back(t);
    }
    sqlite3_finalize(stmt);
    return list;
}

static const string TICKET_SELECT =
"SELECT t.id, t.concert_id, t.user_id, t.seat_number, t.status, t.purchase_date,"
"       c.title, c.artist, u.login "
"FROM tickets t "
"JOIN concerts c ON t.concert_id = c.id "
"JOIN users    u ON t.user_id    = u.id ";

vector<Ticket> dbGetAllTickets() {
    string query = TICKET_SELECT + "ORDER BY t.id;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        return readTickets(stmt);
    }
    return vector<Ticket>();
}

vector<Ticket> dbGetMyTickets(int userId) {
    string query = TICKET_SELECT + "WHERE t.user_id=? ORDER BY t.id;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, userId);
        return readTickets(stmt);
    }
    return vector<Ticket>();
}

bool dbBookTicket(int concertId, int userId, int seat) {
    string check = "SELECT COUNT(*) FROM tickets WHERE concert_id=? AND seat_number=? AND status='active';";
    sqlite3_stmt* stmt;
    int count = 0;
    if (sqlite3_prepare_v2(conn, check.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, concertId);
        sqlite3_bind_int(stmt, 2, seat);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0) {
        cout << "  Место " << seat << " уже занято!\n";
        return false;
    }

    string query = "INSERT INTO tickets (concert_id, user_id, seat_number, status, purchase_date) "
        "VALUES (?, ?, ?, 'active', strftime('%d.%m.%Y', 'now'));";

    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, concertId);
        sqlite3_bind_int(stmt, 2, userId);
        sqlite3_bind_int(stmt, 3, seat);

        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }

    if (ok) {
        string upd = "UPDATE concerts SET available_seats=available_seats-1 WHERE id=?;";
        if (sqlite3_prepare_v2(conn, upd.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, concertId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    return ok;
}

bool dbCancelTicket(int ticketId, int& outConcertId) {
    string query = "SELECT concert_id, status FROM tickets WHERE id=?;";
    sqlite3_stmt* stmt;
    string status = "";
    outConcertId = 0;

    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, ticketId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outConcertId = sqlite3_column_int(stmt, 0);
            status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }

    if (outConcertId == 0) {
        cout << "  Билет не найден!\n";
        return false;
    }

    if (status != "active") {
        cout << "  Этот билет уже отменён!\n";
        return false;
    }

    string upd = "UPDATE tickets SET status='cancelled' WHERE id=?;";
    bool ok = false;
    if (sqlite3_prepare_v2(conn, upd.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, ticketId);
        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }

    if (ok && outConcertId > 0) {
        string restore = "UPDATE concerts SET available_seats=available_seats+1 WHERE id=?;";
        if (sqlite3_prepare_v2(conn, restore.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, outConcertId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    return ok;
}

bool dbDeleteTicket(int id) {
    string query = "DELETE FROM tickets WHERE id=?;";
    sqlite3_stmt* stmt;
    bool ok = false;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_DONE) ok = true;
        sqlite3_finalize(stmt);
    }
    return ok;
}

// Статистика продаж
void dbPrintStatistics() {
    string query =
        "SELECT c.artist, c.title, c.concert_date, c.total_seats, "
        "  (c.total_seats - c.available_seats) AS sold, "
        "  c.available_seats, "
        "  ROUND((CASE WHEN c.total_seats = 0 THEN 0 ELSE (c.total_seats - c.available_seats) * 1.0 / c.total_seats END) * 100, 1) AS pct, "
        "  ROUND((c.total_seats - c.available_seats) * c.price, 2) AS revenue "
        "FROM concerts c ORDER BY sold DESC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cout << "Ошибка подготовки статистики: " << sqlite3_errmsg(conn) << endl;
        return;
    }

    cout << "\n" << string(110, '=') << "\n";
    cout << left
        << setw(20) << "Исполнитель"
        << setw(28) << "Концерт"
        << setw(13) << "Дата"
        << setw(8) << "Всего"
        << setw(9) << "Продано"
        << setw(9) << "Своб."
        << setw(10) << "Запол.%"
        << setw(16) << "Выручка (руб.)"
        << "\n" << string(110, '-') << "\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string artist = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        string title = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        string date = toCP1251(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        int total = sqlite3_column_int(stmt, 3);
        int sold = sqlite3_column_int(stmt, 4);
        int free_seats = sqlite3_column_int(stmt, 5);
        double pct = sqlite3_column_double(stmt, 6);
        double revenue = sqlite3_column_double(stmt, 7);

        stringstream ss_pct;
        ss_pct << fixed << setprecision(1) << pct << "%";

        cout << left
            << setw(20) << artist.substr(0, 19)
            << setw(28) << title.substr(0, 27)
            << setw(13) << date
            << setw(8) << total
            << setw(9) << sold
            << setw(9) << free_seats
            << setw(10) << ss_pct.str()
            << setw(16) << fixed << setprecision(2) << revenue
            << "\n";
    }

    cout << string(110, '=') << "\n";
    sqlite3_finalize(stmt);
}