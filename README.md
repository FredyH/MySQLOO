# MySQLOO 9
An object oriented MySQL module for Garry's Mod.
This module is an almost entirely rewritten version of MySQLOO 8.1.
It supports several new features such as multiple result sets, prepared queries and transactions.
The module also fixed the memory leak issues the previous versions of MySQLOO had.

# Install instructions
Download the latest module for your server's operating system and architecture using the links provided below, then place that file within the `garrysmod/lua/bin/` folder on your server. If the `bin` folder doesn't exist, please create it.

* [Windows (32-bit)](https://github.com/FredyH/MySQLOO/releases/latest/download/gmsv_mysqloo_win32.dll)
* [Windows (64-bit)](https://github.com/FredyH/MySQLOO/releases/latest/download/gmsv_mysqloo_win64.dll)
* [Linux (32-bit)](https://github.com/FredyH/MySQLOO/releases/latest/download/gmsv_mysqloo_linux.dll)
* [Linux (64-bit)](https://github.com/FredyH/MySQLOO/releases/latest/download/gmsv_mysqloo_linux64.dll)

### Notes
* If you're unsure of your server's operating system and architecture, type `lua_run print(jit.os, jit.arch)` into the server's console to find out. The output will be something similar to `Windows x86` (x86 is 32-bit, x64 is 64-bit).
* If your server is using Windows, you will need to install vcredist 2019, 2015, possibly 2008 and others for the module to load correctly.
* Previously you were required to place libmysqlclient.dll besides your srcds executable. This is not required anymore since MySQLOO now links statically against libmysqlclient.

# Documentation


```LUA

-- mysqloo table

mysqloo.connect( host, username, password, database [, port, socket] )
-- returns [Database]
-- Initializes the database object, note that this does not actually connect to the database.

mysqloo.VERSION -- [String] Current MySQLOO version (currently "9")
mysqloo.MINOR_VERSION -- [String] minor version of this library

mysqloo.DATABASE_CONNECTED -- [Number] - Database is connected
mysqloo.DATABASE_NOT_CONNECTED -- [Number] - Database is not connected
mysqloo.DATABASE_INTERNAL_ERROR -- [Number] - Internal error

mysqloo.QUERY_NOT_RUNNING -- [Number] - Query not running
mysqloo.QUERY_WAITING -- [Number] - Query is queued/started
mysqloo.QUERY_RUNNING -- [Number] - Query is being processed (on the database server)
mysqloo.QUERY_COMPLETE -- [Number] - Query is complete
mysqloo.QUERY_ABORTED -- [Number] - Query was aborted

mysqloo.OPTION_NUMERIC_FIELDS -- [Number] - Instead of rows being key value pairs, this makes them just be an array of values
mysqloo.OPTION_NAMED_FIELDS -- [Number] - Not used anymore
mysqloo.OPTION_INTERPRET_DATA -- [Number] - Not used anymore
mysqloo.OPTION_CACHE -- [Number] - Not used anymore

-- Database object

-- Functions
Database:connect()
-- Returns nothing
-- Connects to the database (non blocking)
-- This function calls the onConnected or onConnectionFailed callback

Database:disconnect(shouldWait)
-- Returns nothing
-- disconnects from the database and waits for all queries to finish if shouldWait is true

Database:query( sql )
-- Returns [Query]
-- Initializes a query to the database, [String] sql is the SQL query to run.

Database:prepare( sql )
-- Returns [PreparedQuery]
-- Creates a prepared query associated with the database

Database:createTransaction()
-- Returns [Transaction]
-- Creates a transaction that executes multiple statements atomically
-- Check [url]https://en.wikipedia.org/wiki/ACID[/url] for more information

Database:escape( str )
-- Returns [String]
-- Escapes [String] str so that it is safe to use in a query.
-- If the characterset of the database is changed after connecting, this might not work properly anymore

Database:abortAllQueries()
-- Returns the amount of queries aborted successfully
-- Aborts all waiting (QUERY_WAITING) queries

Database:status()
-- Returns [Number] (mysqloo.DATABASE_* enums)
-- Checks the connection to the database
-- This shouldn't be used to detect timeouts to the server anymore (it's not possible anymore)

Database:setAutoReconnect(shouldReconnect)
-- Returns nothing
-- The autoreconnect feature of mysqloo can be disabled if this function is called with shouldReconnect = false
-- This has to be called before Database:connect() to work

Database:setMultiStatements(useMultiStatemets)
-- Returns nothing
-- Multi statements ("SELECT 1; SELECT 2;") can be disabled if this function is called with useMultiStatemets = false
-- This has to be called before Database:connect() to work

Database:setCachePreparedStatements(cachePreparedStatements)
-- Returns nothing
-- This will disable all caching of prepared query handles
-- which will reduce the performance of prepared queries that are being reused
-- Set this to true if you run into the prepared query limit imposed by the server

Database:wait()
-- Returns nothing
-- Forces the server to wait for the connection to finish. (might cause deadlocks)
-- This has to be called after Database:connect()

Database:serverVersion()
-- Returns [Number]
-- Gets the MySQL servers version

Database:serverInfo()
-- Returns [String]
-- Fancy string of the MySQL servers version

Database:hostInfo()
-- Returns [String]
-- Gets information about the connection.

Database:queueSize()
-- Returns [Number]
-- Gets the amount of queries waiting to be processed

Database:ping()
-- Returns [Boolean]
-- Actively checks if the database connection is still up and attempts to reconnect if it is down
-- This will freeze your server for at least 2 times the pingtime to
-- the database server if the connection is down
-- returns true if the connection is still up, false otherwise

Database:setCharacterSet(charSetName)
-- Returns [Boolean]
-- Attempts to set the connection's character set to the one specified.
-- Please note that this does block the main server thread if there is a query currently being ran
-- Returns true on success, false on failure

Database:setSSL(key, cert, ca, capath, cipher)
-- Returns nothing
-- Sets the SSL configuration of the database object. This allows you to enable secure connections over the internet using TLS.
-- Every parameter is optional and can be omitted (set to nil) if not required.
-- See https://dev.mysql.com/doc/c-api/8.0/en/mysql-ssl-set.html for the description of each parameter.

-- Callbacks
Database.onConnected( db )
-- Called when the connection to the MySQL server is successful

Database.onConnectionFailed( db, err )
-- Called when the connection to the MySQL server fails, [String] err is why.

-- Query/PreparedQuery object (transactions also inherit all functions, some have no effect though)

-- Functions
Query:start()
-- Returns nothing
-- Starts the query.

Query:isRunning()
-- Returns [Boolean]
-- True if the query is running or waiting, false if it isn't.

Query:getData()
-- Returns [Table]
-- Gets the data the query returned from the server
-- Format: { row1, row2, row3, ... }
-- Row format: { field_name = field_value } or {first_field, second_field, ...} if OPTION_NUMERIC_FIELDS is enabled

Query:abort()
-- Returns [Boolean]
-- Attempts to abort the query if it is still in the state QUERY_WAITING
-- Returns true if at least one running instance of the query was aborted successfully, false otherwise

Query:lastInsert()
-- Returns [Number]
-- Gets the autoincrement index of the last inserted row of the current result set

Query:affectedRows()
-- Returns [Number]
-- Gets the number of rows the query has affected (of the current result set)

Query:setOption( option )
-- Returns nothing
-- Changes how the query returns data (mysqloo.OPTION_* enums).

Query:wait(shouldSwap)
-- Returns nothing
-- Forces the server to wait for the query to finish.
-- This should only ever be used if it is really necessary, since it can cause lag and
-- If shouldSwap is true, the query is being swapped to the front of the queue
-- making it the next query to be executed


Query:error()
-- Returns [String]
-- Gets the error caused by the query, or "" if there was no error.

Query:hasMoreResults()
-- Returns [Boolean]
-- Returns true if the query still has more data associated with it (which means getNextResults() can be called)
-- Note: This function works unfortunately different that one would expect.
-- hasMoreResults() returns true if there is currently a result that can be popped, rather than if there is an
-- additional result that has data. However, this does make for a nicer code that handles multiple results.
-- See Examples/multi_results.lua for an example how to use it.

Query:getNextResults()
-- Returns [Table]
-- Pops the current result set, chaning the results of lastInsert() and affectedRows()  and getData()
-- to those of the next result set. Returns the rows of the next result set in the same format as getData()
-- Throws an error if attempted to be called when there is no result set left to be popped

-- Callbacks
-- ALWAYS set these callbacks before you start the query or you might run into issues

Query.onAborted( q )
-- Called when the query is aborted.

Query.onError( q, err, sql )
-- Called when the query errors, [String] err is the error and [String] sql is the SQL query that caused it.

Query.onSuccess( q, data )
-- Called when the query is successful, [Table] data is the data the query returned.

Query.onData( q, data )
-- Called when the query retrieves a row of data, [Table] data is the row.


-- PreparedQuery object
-- A prepared query uses a prepared statement that, instead of having the actual values of
-- new rows, etc. in the query itself, it uses parameters, that can be set using the appropriate methods
-- PreparedStatements make sql injections pretty much impossible since the data is sent seperately from the parameterized query
-- Prepared queries also make it easy to reuse one query several times to insert/update/delete multiple rows
-- An example of a parameterized query would be "INSERT INTO users (`steamid`, `nick`) VALUES(?,?)"
-- A few types of queries might not work with prepared statements
-- For more information about prepared statements you can read here: [url]https://en.wikipedia.org/wiki/Prepared_statement[/url]

PreparedQuery:setNumber(index, number)
-- Returns nothing
-- Sets the parameter at index (1-based) to be of type double with value number

PreparedQuery:setString(index, str)
-- Returns nothing
-- Sets the parameter at index (1-based) to be of type string with value str
-- Note: str should not!! be escaped

PreparedQuery:setBoolean(index, bool)
-- Returns nothing
-- Sets the parameter at index (1-based) to be of type boolean with value bool

PreparedQuery:setNull(index)
-- Returns nothing
-- Sets the parameter at index (1-based) to be NULL

PreparedQuery:clearParameters()
-- Returns nothing
-- Clears all currently set parameters inside the prepared statement.

PreparedQuery:putNewParameters()
-- Returns nothing
-- Deprecated: Start the same prepared statement multiple times instead


-- Transaction object
-- Transactions are used to group several seperate queries or prepared queries together.
-- Either all queries in the transaction are executed successfully or none of them are.
-- Please note that single queries are already transactions by themselves so it usually only makes sense to have transactions
-- with at least two queries.
-- Since mysqloo works async, much of the power of transactions (such as manually rolling back a transaction) cannot be used properly, but
-- there's still many areas they can be useful.
-- Important note: Callbacks of individual queries that are part of the transactions are not run (both in error and successful case). Use the callbacks of the transaction instead.

Transaction:addQuery(query)
-- Adds a query to the transaction. The callbacks of the added queries will not be called.
-- query can be either a PreparedQuery or a regular Query

Transaction:getQueries()
-- Returns all queries that have been added to this transaction.

-- Callbacks

Transaction.onError(tr, err)
-- Called when any of the queries caused an error, no queries will have had any effect

Transaction.onSuccess()
--  Called when all queries in the transaction have been executed successfully

```

# Build instructions:

This project uses [CMake](https://cmake.org/) as a build system.

## Windows

### Visual Studio
Visual Studio has support for CMake since Visual Studio 2017. To open the project, run Visual Studio and under `File > Open > CMake...`
select the CMakeList.txt from this directory.

The CMakeSettings.json in this project should already define both a 32 and 64 bit configuration.
You can add new configurations in the combo box that contains the x64 config. Here you can change the build type to Release or RelWithDebInfo and duplicate the config
for a 32 bit build.

To build the project, you can then simply run `` from the toolbar. The output files are placed in the `out/build/{ConfigurationName}/` subfolder
of this project.


### CLion
Simply open the project in CLion and import the CMake project. Assuming you have a [valid toolchain](https://www.jetbrains.com/help/clion/how-to-create-toolchain-in-clion.html) setup,
you can simply build the project using `Build > Build Project` in the toolbar.

To compile for 32 bit rather than 64 bit, you can select a 32 bit VS toolchain, rather than the 64 bit one.

The output files are placed within the `cmake-build-debug/` directory of this project.


## Linux


### Prerequisites
To compile the project, you will need CMake and a functioning c++ compiler. For example, under Ubuntu, the following packages
can be used to compile the module.
```bash
sudo apt install build-essential gcc-multilib cmake
```

### Compiling
To compile the module, follow the following steps:
- enter the project directory and run `cmake .` in bash.
- in the same directory run `make` in bash.
- The module should be compiled and the resulting binary should be placed directly in the project directory.



## Mac
Mac is currently not supported since the MariaDB connector is not available on Mac (at least not precompiled).
However, if you are able to compile the connector yourself, building for Mac should broadly follow the same instructions
as for Linux.