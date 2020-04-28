# UPS

### How to run our code
#### Database
1. git clone both backend and frontend
2. psql -U postgres
- username: postgres
- password: abc123
``DROP DATABASE upsdb;``
``CREATE DATABASE upsdb;``
3. ``python3 manage.py migrate`` to create table
4. make sure you do it every time after running and debugging

#### Backend
1. do `make` in the backend directory
2. `std::array` MAY not compile in some system, just replace it with `std::vector`
3.  you need to change IP address and port number in `main.cpp`
4. `./ups`
