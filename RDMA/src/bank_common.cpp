#include <libpq-fe.h>
#include <cstring>
#include <string>

#define BUFSIZE 1024
#define DB_HOST "128.110.216.20"
#define DB_PORT "5432"
#define POSTGRES_PW "praneeth"

void initdb(int numAccounts) {
    // Initialize connection with customers db
    char conninfo[BUFSIZE];
    sprintf(conninfo, "host = %s port = %s user = postgres password = %s", DB_HOST, DB_PORT, "praneeth");
    PGconn *pgconn = PQconnectdb(conninfo);
    if (PQstatus(pgconn) != CONNECTION_OK) {
        printf("Postgres database connection error\n%s\n", PQerrorMessage(pgconn));
        return;
    }

    // Check if the "accounts" database exists
    PGresult *res = PQexecParams(pgconn, "SELECT 1 FROM pg_database WHERE datname = 'accounts'",
                                  0, NULL, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Query execution failed: %s", PQresultErrorMessage(res));
        PQclear(res);
        PQfinish(pgconn);
        return;
    }
    
    // If the "accounts" database does not exist, create it
    if (PQntuples(res) == 0) {
        PQclear(res);
        res = PQexecParams(pgconn, "CREATE DATABASE accounts", 0, NULL, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Database creation failed: %s", PQresultErrorMessage(res));
            PQclear(res);
            PQfinish(pgconn);
            return;
        }
        printf("Database 'accounts' created successfully\n");
    } else {
        printf("Database 'accounts' already exists\n");
    }
    
    // Cleanup
    PQclear(res);
    PQfinish(pgconn);
    bzero(conninfo, BUFSIZE);
    sprintf(conninfo, "host = %s port = %s dbname = %s user = postgres password = %s", DB_HOST, DB_PORT, "accounts", "praneeth");
    pgconn = PQconnectdb(conninfo);
    if (PQstatus(pgconn) != CONNECTION_OK) {
        printf("Postgres database connection error\n%s\n", PQerrorMessage(pgconn));
        return;
    }

    // Execute sql statement
    char create_table[BUFSIZE];
    sprintf(create_table, "CREATE TABLE IF NOT EXISTS accounts (id SERIAL PRIMARY KEY, balance DOUBLE PRECISION);INSERT INTO accounts (balance) SELECT 1000 FROM generate_series(1, %d)", numAccounts);
    res = PQexec(pgconn, create_table);
    char *error_msg = PQresultErrorMessage(res);
    ExecStatusType status;
    if ((status = PQresultStatus(res)) != PGRES_COMMAND_OK) {
        // Print error message
        printf("Postgresql error: %s\n", PQresStatus(status));
        printf("%s\n", error_msg);
        return;
    }

    PQfinish(pgconn);
}

std::string balance(int accountNum) {
    // Initialize connection with customers db
    char conninfo[BUFSIZE];
    sprintf(conninfo, "host = %s port = %s dbname = %s user = postgres password = %s", DB_HOST, DB_PORT, "accounts", "praneeth");
    PGconn *pgconn = PQconnectdb(conninfo);
    if (PQstatus(pgconn) != CONNECTION_OK) {
        printf("Postgres database connection error\n%s\n", PQerrorMessage(pgconn));
        return "error";
    }

    // Execute sql statement
    char select_statement[BUFSIZE];
    sprintf(select_statement, "SELECT balance FROM accounts WHERE id = %d", accountNum);
    PGresult *res = PQexec(pgconn, select_statement);
    char *error_msg = PQresultErrorMessage(res);
    ExecStatusType status;
    if ((status = PQresultStatus(res)) != PGRES_TUPLES_OK) {
        // Print error message
        printf("Postgresql error: %s\n", PQresStatus(status));
        printf("%s\n", error_msg);
        return "error";
    }

    std::string balance = PQgetvalue(res, 0, 0);
    PQfinish(pgconn);
    PQclear(res);
    return balance;
}

std::string transfer(int accountNum1, int accountNum2, double balance) {
    // Initialize connection with customers db
    char conninfo[BUFSIZE];
    sprintf(conninfo, "host = %s port = %s dbname = %s user = postgres password = %s", DB_HOST, DB_PORT, "accounts", "praneeth");
    PGconn *pgconn = PQconnectdb(conninfo);
    if (PQstatus(pgconn) != CONNECTION_OK) {
        printf("Postgres database connection error\n%s\n", PQerrorMessage(pgconn));
        return "error";
    }

    // Execute sql statement
    char update_statement[BUFSIZE];
    sprintf(update_statement, "UPDATE accounts SET balance = CASE WHEN id = %d THEN balance - %f WHEN id = %d THEN balance + %f END WHERE id IN (%d, %d)", accountNum1, balance, accountNum2, balance, accountNum1, accountNum2);
    PGresult *res = PQexec(pgconn, update_statement);
    char *error_msg = PQresultErrorMessage(res);
    ExecStatusType status;
    if ((status = PQresultStatus(res)) != PGRES_COMMAND_OK) {
        // Print error message
        printf("Postgresql error: %s\n", PQresStatus(status));
        printf("%s\n", error_msg);
        return "error";
    }

    PQfinish(pgconn);
    PQclear(res);

    return "Transaction successful";
}