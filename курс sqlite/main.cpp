#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <windows.h>

#include<conio.h>
#include "models.h"
#include "db.h"

using namespace std;

// Текущий вошедший пользователь
User currentUser;

// Вспомогательные функции ввода
void cls() {
    system("cls");
}

void pause() {
    cout << "\nНажмите Enter для продолжения...";
    cin.get();
}

bool isValidDate(const string& s) {
    if (s.size() != 10)              return false;
    if (s[4] != '-' || s[7] != '-') return false;

    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit(s[i]))   return false;
    }

    int y = stoi(s.substr(0, 4));
    int m = stoi(s.substr(5, 2));
    int d = stoi(s.substr(8, 2));

    return y >= 2000 && y <= 2100
        && m >= 1 && m <= 12
        && d >= 1 && d <= 31;
}

string inputStr(string prompt) {
    while (true) {
        cout << prompt;
        string s;
        getline(cin, s);
        if (!s.empty()) return s;
        cout << "\n  Ошибка: поле не может быть пустым!\n";
    }
}

int inputInt(string prompt) {
    int n;
    while (true) {
        cout << prompt;
        if (cin >> n) {
            cin.ignore(1000, '\n');
            return n;
        }
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "  Ошибка: введите целое число!\n";
    }
}

double inputDouble(string prompt) {
    double n;
    while (true) {
        cout << prompt;
        if (cin >> n) {
            cin.ignore(1000, '\n');
            return n;
        }
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "  Ошибка: введите число!\n";
    }
}

// Функции вывода таблиц
void printConcerts(vector<Concert> list) {
    if (list.empty()) {
        cout << "\n  Нет данных для отображения.\n";
        return;
    }

    cout << "\n" << string(112, '=') << "\n";
    cout << left
        << setw(5) << "ID"
        << setw(20) << "Исполнитель"
        << setw(25) << "Название"
        << setw(13) << "Дата"
        << setw(22) << "Площадка"
        << setw(10) << "Цена"
        << setw(8) << "Мест"
        << setw(7) << "Своб."
        << "\n";
    cout << string(112, '-') << "\n";

    for (size_t i = 0; i < list.size(); i++) {
        Concert c = list[i];
        cout << left
            << setw(5) << c.id
            << setw(20) << c.artist.substr(0, 19)
            << setw(25) << c.title.substr(0, 24)
            << setw(13) << c.concertDate
            << setw(22) << c.venue.substr(0, 21)
            << setw(10) << fixed << setprecision(2) << c.price
            << setw(8) << c.totalSeats
            << setw(7) << c.availableSeats
            << "\n";
    }
    cout << string(112, '=') << "\n";
}

void printTickets(vector<Ticket> list) {
    if (list.empty()) {
        cout << "\n  Нет данных для отображения.\n";
        return;
    }

    cout << "\n" << string(97, '=') << "\n";
    cout << left
        << setw(5) << "ID"
        << setw(22) << "Концерт"
        << setw(18) << "Исполнитель"
        << setw(16) << "Пользователь"
        << setw(7) << "Место"
        << setw(12) << "Статус"
        << setw(12) << "Дата"
        << "\n";
    cout << string(97, '-') << "\n";

    for (size_t i = 0; i < list.size(); i++) {
        Ticket t = list[i];
        string statusRu = (t.status == "active") ? "Активен" : "Отменён";

        cout << left
            << setw(5) << t.id
            << setw(22) << t.concertTitle.substr(0, 21)
            << setw(18) << t.artistName.substr(0, 17)
            << setw(16) << t.userLogin.substr(0, 15)
            << setw(7) << t.seatNumber
            << setw(12) << statusRu
            << setw(12) << t.purchaseDate << "\n";
    }

    cout << string(97, '=') << "\n";
}

void printUsers(vector<User> list) {
    if (list.empty()) {
        cout << "\n  Нет пользователей.\n";
        return;
    }

    cout << "\n" << string(68, '=') << "\n";
    cout << left
         << setw(5)  << "ID"
         << setw(20) << "Логин"
         << setw(30) << "ФИО"
         << setw(10) << "Роль"
         << "\n" << string(68, '-') << "\n";

    for (size_t i = 0; i < list.size(); i++) {
        User u = list[i];
        cout << left
             << setw(5)  << u.id
             << setw(20) << u.login
             << setw(30) << u.fullName
             << setw(10) << (u.isAdmin() ? "Админ" : "Пользователь")
             << "\n";
    }

    cout << string(68, '=') << "\n";
}

// Авторизация
bool loginAs(string requiredRole) {
    cls();
    if (requiredRole == "admin")
        cout << "=== ВХОД ДЛЯ АДМИНИСТРАТОРА ===\n\n";
    else
        cout << "=== ВХОД ДЛЯ ПОЛЬЗОВАТЕЛЯ ===\n\n";
    string pass;
    char ch;
    string login = inputStr("  Логин  : ");
    while (true) {
        cout << "  Пароль : ";
        pass = "";
        while ((ch = _getch()) != '\r') { // \r - Enter
            if (ch == '\b') { // Обработка Backspace
                if (!pass.empty()) {
                    pass.pop_back();
                    cout << "\b \b";
                }
            }
            else {
                pass += ch;
                cout << '*';
            }
        }
        if (!pass.empty()) break;
        cout <<"\n   Ошибка: введите строчку!\n";
    }
    if (!dbLogin(login, pass, currentUser)) {
        cout << "\n  Ошибка: неверный логин или пароль!\n";
        pause();
        return false;
    }

    if (currentUser.role != requiredRole) {
        cout << "\n  Ошибка: недостаточно прав доступа!\n";
        pause();
        return false;
    }

    cout << "\n  Добро пожаловать, " << currentUser.fullName << "!\n";
    pause();
    return true;
}

// Меню АДМИНИСТРАТОРА
void adminAddConcert() {
    cls();
    cout << "=== ДОБАВЛЕНИЕ КОНЦЕРТА ===\n\n";
    Concert c;
    c.artist      = inputStr("  Исполнитель        : ");
    c.title       = inputStr("  Название концерта  : ");
    while (true) {
        c.concertDate = inputStr("  Дата (ГГГГ-ММ-ДД): ");
        if (isValidDate(c.concertDate)) break;
        cout << "  Ошибка: введите дату в формате ГГГГ-ММ-ДД\n";
    }
    c.venue       = inputStr("  Место проведения   : ");
    c.price       = inputDouble("  Цена билета (руб.) : ");
    c.totalSeats  = inputInt("  Количество мест    : ");

    if (dbAddConcert(c))
        cout << "\n  Концерт успешно добавлен!\n";
    else
        cout << "\n  Ошибка при добавлении!\n";
    pause();
}

void adminEditConcert() {
    cls();
    cout << "=== РЕДАКТИРОВАНИЕ КОНЦЕРТА ===\n";
    printConcerts(dbGetAllConcerts());

    int id = inputInt("\n  Введите ID концерта (0 - отмена): ");
    if (id == 0) return;

    Concert c = dbGetConcertById(id);
    if (c.id == 0) {
        cout << "  Концерт с таким ID не найден!\n";
        pause(); return;
    }

    cout << "\n  Оставьте поле пустым - оставит старое значение.\n\n";

    string temp;
    cout << "  Исполнитель [" << c.artist << "]: ";
    getline(cin, temp); if (!temp.empty()) c.artist = temp;

    cout << "  Название [" << c.title << "]: ";
    getline(cin, temp); if (!temp.empty()) c.title = temp;

    while (true) {
        cout << "  Дата [" << c.concertDate << "]: ";
        getline(cin, temp);
        if (temp.empty()) break;
        if (isValidDate(temp)) { c.concertDate = temp; break; }
        cout << "  Ошибка: введите дату в формате ГГГГ-ММ-ДД\n";
    }

    cout << "  Площадка [" << c.venue << "]: ";
    getline(cin, temp); if (!temp.empty()) c.venue = temp;

    cout << "  Цена [" << c.price << "]: ";
    getline(cin, temp); if (!temp.empty()) c.price = stod(temp);

    cout << "  Мест [" << c.totalSeats << "]: ";
    getline(cin, temp); 
    if (!temp.empty()) {
        int oldTotal = c.totalSeats;
        c.totalSeats = stoi(temp);
        // Корректируем свободные места в зависимости от изменения общего кол-ва мест
        c.availableSeats += (c.totalSeats - oldTotal); 
    }

    if (dbUpdateConcert(c))
        cout << "\n  Концерт обновлён!\n";
    else
        cout << "\n  Ошибка при обновлении!\n";
    pause();
}

void adminDeleteConcert() {
    cls();
    cout << "=== УДАЛЕНИЕ КОНЦЕРТА ===\n";
    printConcerts(dbGetAllConcerts());

    int id = inputInt("\n  Введите ID концерта (0 - отмена): ");
    if (id == 0) return;

    int confirm = inputInt("  Подтвердить удаление? (1 - да, 0 - нет): ");
    if (confirm == 1) {
        if (dbDeleteConcert(id))
            cout << "  Концерт и все его билеты удалены!\n";
        else
            cout << "  Ошибка при удалении!\n";
    } else {
        cout << "  Отменено.\n";
    }
    pause();
}

void adminSearchAndFilter() {
    cls();
    cout << "=== ПОИСК И ФИЛЬТРАЦИЯ ===\n\n";
    cout << "  1. Поиск по исполнителям\n";
    cout << "  2. Фильтр: цена не выше...\n";
    cout << "  3. Только концерты со свободными местами\n";
    int ch = inputInt("\n  Ваш выбор: ");

    if (ch == 1) {
        string artist = inputStr("  Введите исполнителя: ");
        printConcerts(dbSearchByArtist(artist));
    } else if (ch == 2) {
        double price = inputDouble("  Максимальная цена (руб.): ");
        printConcerts(dbFilterByPrice(price));
    } else if (ch == 3) {
        printConcerts(dbGetAvailableConcerts());
    }
    pause();
}

void adminSort() {
    cls();
    cout << "=== СОРТИРОВКА ===\n\n";
    cout << "  1. По дате\n";
    cout << "  2. По цене (возрастание)\n";
    cout << "  3. По исполнителю (А-Я)\n";
    cout << "  4. По количеству мест (убывание)\n";
    cout << "  5. По свободным местам (убывание)\n";
    int ch = inputInt("\n  Ваш выбор: ");

    if      (ch == 1) printConcerts(dbGetAllConcerts("concert_date"));
    else if (ch == 2) printConcerts(dbGetAllConcerts("price ASC"));
    else if (ch == 3) printConcerts(dbGetAllConcerts("artist ASC"));
    else if (ch == 4) printConcerts(dbGetAllConcerts("total_seats DESC"));
    else if (ch == 5) printConcerts(dbGetAllConcerts("available_seats DESC"));
    else              printConcerts(dbGetAllConcerts());
    pause();
}

void adminManageUsers() {
    while (true) {
        cls();
        cout << "=== УПРАВЛЕНИЕ ПОЛЬЗОВАТЕЛЯМИ ===\n";
        printUsers(dbGetAllUsers());
        cout << "\n  1. Добавить пользователя\n";
        cout << "  2. Удалить пользователя\n";
        cout << "  3. Изменить роль пользователя\n";
        cout << "  4. Изменить пароль пользователя\n";
        cout << "  0. Назад\n";
        int ch = inputInt("\n  Ваш выбор: ");

        if (ch == 0) return;

        else if (ch == 1) {
            User u;
            u.login = inputStr("  Логин  : ");
            u.password = inputStr("  Пароль : ");
            u.fullName = inputStr("  ФИО    : ");
            int r = inputInt("  Роль (1 - администратор, 2 - пользователь): ");
            u.role = (r == 1) ? "admin" : "user";
            if (dbAddUser(u))
                cout << "  Пользователь добавлен!\n";
            else
                cout << "  Ошибка!\n";
            pause();

        } else if (ch == 2) {
            int id = inputInt("  ID пользователя для удаления: ");
            if (id == currentUser.id) {
                cout << "  Нельзя удалить себя!\n";
            } else {
                if (dbDeleteUser(id))
                    cout << "  Пользователь удалён!\n";
                else
                    cout << "  Ошибка!\n";
            }
            pause();

        } else if (ch == 3) {
            int id = inputInt("  ID пользователя: ");
            vector<User> users = dbGetAllUsers();
            User found;
            for (size_t i = 0; i < users.size(); i++) {
                if (users[i].id == id) { found = users[i]; break; }
            }
            if (found.id == 0) { cout << "  Не найден!\n"; pause(); continue; }

            int r = inputInt("  Новая роль (1 - администратор, 2 - пользователь): ");
            found.role = (r == 1) ? "admin" : "user";
            if (dbUpdateUser(found))
                cout << "  Роль изменена!\n";
            else
                cout << "  Ошибка!\n";
            pause();

        } else if (ch == 4) {
            int id = inputInt("  ID пользователя: ");
            vector<User> users = dbGetAllUsers();
            User found;
            for (size_t i = 0; i < users.size(); i++) {
                if (users[i].id == id) { found = users[i]; break; }
            }
            if (found.id == 0) { cout << "  Не найден!\n"; pause(); continue; }

            found.password = inputStr("  Новый пароль: ");
            if (dbUpdateUser(found))
                cout << "  Пароль изменён!\n";
            else
                cout << "  Ошибка!\n";
            pause();
        }
    }
}

void adminMenu() {
    while (true) {
        cls();
        cout << "=== МЕНЮ АДМИНИСТРАТОРА ===\n";
        cout << "  Пользователь: " << currentUser.fullName << "\n\n";
        cout << "  1.  Просмотр всех концертов\n";
        cout << "  2.  Добавить концерт\n";
        cout << "  3.  Редактировать концерт\n";
        cout << "  4.  Удалить концерт\n";
        cout << "  5.  Просмотр всех билетов\n";
        cout << "  6.  Удалить билет\n";
        cout << "  7.  Поиск и фильтрация\n";
        cout << "  8.  Сортировка концертов\n";
        cout << "  9.  Управление пользователями\n";
        cout << "  10. Статистика продаж (инд. задание)\n";
        cout << "  0.  Выход в главное меню\n";

        int ch = inputInt("\n  Ваш выбор: ");

        if (ch == 0) return;
        else if (ch == 1) { cls(); cout << "=== ВСЕ КОНЦЕРТЫ ===\n"; printConcerts(dbGetAllConcerts()); pause(); }
        else if (ch == 2) adminAddConcert();
        else if (ch == 3) adminEditConcert();
        else if (ch == 4) adminDeleteConcert();
        else if (ch == 5) { cls(); cout << "=== ВСЕ БИЛЕТЫ ===\n"; printTickets(dbGetAllTickets()); pause(); }
        else if (ch == 6) {
            cls(); cout << "=== УДАЛЕНИЕ БИЛЕТА ===\n"; printTickets(dbGetAllTickets());
            int id = inputInt("\n  ID билета (0 - отмена): ");
            if (id != 0) {
                if (dbDeleteTicket(id)) cout << "  Билет удалён!\n"; else cout << "  Ошибка!\n";
                pause();
            }
        }
        else if (ch == 7)  adminSearchAndFilter();
        else if (ch == 8)  adminSort();
        else if (ch == 9)  adminManageUsers();
        else if (ch == 10) { cls(); cout << "=== СТАТИСТИКА ПРОДАЖ ===\n"; dbPrintStatistics(); pause(); }
        else { cout << "  Неверный выбор!\n"; pause(); }
    }
}

// Меню ПОЛЬЗОВАТЕЛЯ
void userBookTicket() {
    cls();
    cout << "=== БРОНИРОВАНИЕ БИЛЕТА ===\n";
    printConcerts(dbGetAvailableConcerts());

    int cid = inputInt("\n  Выберите ID концерта (0 - отмена): ");
    if (cid == 0) return;

    Concert c = dbGetConcertById(cid);
    if (c.id == 0) {
        cout << "  Концерт не найден!\n"; pause(); return;
    }
    if (c.availableSeats == 0) {
        cout << "  На этот концерт нет свободных мест!\n"; pause(); return;
    }

    cout << "\n  " << c.artist << " — " << c.title << "\n";
    cout << "  Дата: " << c.concertDate << "  |  Площадка: " << c.venue << "\n";
    cout << "  Цена: " << fixed << setprecision(2) << c.price << " руб."
         << "  |  Свободно мест: " << c.availableSeats << "\n";

    // Показываем, какие места уже заняты (Переписано под SQLite)
    string checkQuery = "SELECT seat_number FROM tickets WHERE concert_id=? AND status='active' ORDER BY seat_number;";
    sqlite3_stmt* stmt;
    cout << "  Занятые места: ";
    bool hasTickets = false;

    if (sqlite3_prepare_v2(conn, checkQuery.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, cid);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cout << sqlite3_column_int(stmt, 0) << " ";
            hasTickets = true;
        }
        sqlite3_finalize(stmt);
    }
    if (!hasTickets) cout << "(нет)";
    cout << "\n";

    int seat = inputInt("\n  Выберите номер места (1–" + to_string(c.totalSeats) + "): ");
    if (seat < 1 || seat > c.totalSeats) {
        cout << "  Неверный номер места!\n"; pause(); return;
    }

    if (dbBookTicket(cid, currentUser.id, seat)) {
        cout << "\n  Билет забронирован!\n";
        cout << "  Концерт: " << c.artist << " — " << c.title << "\n";
        cout << "  Место:   " << seat << "\n";
        cout << "  Цена:    " << fixed << setprecision(2) << c.price << " руб.\n";
    } else {
        cout << "\n  Ошибка бронирования!\n";
    }
    pause();
}

void userCancelTicket() {
    cls();
    cout << "=== ОТМЕНА БРОНИРОВАНИЯ ===\n";
    vector<Ticket> myTickets = dbGetMyTickets(currentUser.id);
    printTickets(myTickets);

    if (myTickets.empty()) { pause(); return; }

    int id = inputInt("\n  ID билета для отмены (0 - нет): ");
    if (id == 0) return;

    bool found = false;
    for (size_t i = 0; i < myTickets.size(); i++) {
        if (myTickets[i].id == id) { found = true; break; }
    }
    if (!found) {
        cout << "  Этот билет не в вашем списке!\n"; pause(); return;
    }

    int concertId = 0;
    if (dbCancelTicket(id, concertId))
        cout << "  Бронирование отменено. Место освобождено.\n";
    else
        cout << "  Ошибка при отмене!\n";
    pause();
}

void userSearch() {
    cls();
    cout << "=== ПОИСК КОНЦЕРТОВ ===\n\n";
    cout << "  1. По исполнителям\n";
    cout << "  2. По максимальной цене\n";
    int ch = inputInt("\n  Ваш выбор: ");

    if (ch == 1) {
        string artist = inputStr("  Исполнитель: ");
        printConcerts(dbSearchByArtist(artist));
    } else if (ch == 2) {
        double price = inputDouble("  Максимальная цена (руб.): ");
        printConcerts(dbFilterByPrice(price));
    }
    pause();
}

void userMenu() {
    while (true) {
        cls();
        cout << "=== МЕНЮ ПОЛЬЗОВАТЕЛЯ ===\n";
        cout << "  Пользователь: " << currentUser.fullName << "\n\n";
        cout << "  1. Просмотр доступных концертов\n";
        cout << "  2. Поиск концертов\n";
        cout << "  3. Забронировать билет\n";
        cout << "  4. Мои билеты\n";
        cout << "  5. Отменить бронирование\n";
        cout << "  6. Статистика продаж (инд. задание)\n";
        cout << "  0. Выход в главное меню\n";

        int ch = inputInt("\n  Ваш выбор: ");

        if (ch == 0) return;
        else if (ch == 1) { cls(); cout << "=== ДОСТУПНЫЕ КОНЦЕРТЫ ===\n"; printConcerts(dbGetAvailableConcerts()); pause(); }
        else if (ch == 2) userSearch();
        else if (ch == 3) userBookTicket();
        else if (ch == 4) { cls(); cout << "=== МОИ БИЛЕТЫ ===\n"; printTickets(dbGetMyTickets(currentUser.id)); pause(); }
        else if (ch == 5) userCancelTicket();
        else if (ch == 6) { cls(); cout << "=== СТАТИСТИКА ПРОДАЖ ===\n"; dbPrintStatistics(); pause(); }
        else { cout << "  Неверный выбор!\n"; pause(); }
    }
}

// Главное меню
int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    setlocale(LC_ALL, "Russian");
    setlocale(LC_NUMERIC, "C");

    cls();
    cout << "=========================================\n";
    cout << "  БРОНИРОВАНИЕ БИЛЕТОВ НА КОНЦЕРТЫ\n";
    cout << "=========================================\n\n";
    cout << "  Подключение к базе данных...\n";

    // Указываем локальный файл базы данных
    if (!dbConnect("concerts.db")) {
        cout << "\n  Не удалось запустить локальную БД SQLite.\n";
        system("pause");
        return 1;
    }
    dbInit();
    cout << "  Подключение установлено (файл: concerts.db).\n\n";
    pause();

    while (true) {
        cls();
        cout << "=========================================\n";
        cout << "  БРОНИРОВАНИЕ БИЛЕТОВ НА КОНЦЕРТЫ\n";
        cout << "=========================================\n\n";
        cout << "  1. Вход для администратора\n";
        cout << "  2. Вход для пользователя\n";
        cout << "  0. Выход из программы\n";

        int ch = inputInt("\n  Ваш выбор: ");

        if      (ch == 1 && loginAs("admin")) adminMenu();
        else if (ch == 2 && loginAs("user"))  userMenu();
        else if (ch == 0) break;
        else if (ch != 1 && ch != 2) { cout << "  Неверный выбор!\n"; pause(); }
    }

    cout << "\n  До свидания!\n\n";
    sqlite3_close(conn); // Корректное закрытие SQLite
    return 0;
}