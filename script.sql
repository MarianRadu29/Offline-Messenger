CREATE TABLE messages (
                          id INTEGER NOT NULL,                  -- ID unic pentru fiecare mesaj
                          sender TEXT NOT NULL,                 -- Expeditorul mesajului
                          recipient TEXT NOT NULL,              -- Destinatarul mesajului
                          message TEXT NOT NULL,                -- Mesajul efectiv trimis
                          reply INTEGER  NOT NULL               -- ID ul mesajul la care se da reply,este optional,poate fii null
);

CREATE TABLE users(
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL
);

INSERT INTO users(username, password) VALUES ('Marian','1234');
INSERT INTO users(username, password) VALUES ('Matei','1234');

--INSERT INTO messages (id,sender, recipient, message) VALUES (1,'Marian', 'Matei', 'Salut!');
--INSERT INTO messages (id,sender, recipient, message) VALUES (2,'Matei', 'Marian', 'Bruhhh');
--INSERT INTO messages (id,sender, recipient, message) VALUES (3,'Marian', 'Matei', 'GOAT');

--  sqlite3 database.db < script.sql