# MySQLOO 9
An object oriented MySQL module for Garry's Mod.
This module is an almost entirely rewritten version of MySQLOO 8.1.
It supports several new features such as multiple result sets, prepared queries and transactions.
The module also fixed the memory leak issues the previous versions of MySQLOO had.

For further information please [visit this forum thread](https://forum.facepunch.com/f/gmodaddon/jjdq/gmsv-mysqloo-v9-Rewritten-MySQL-Module-prepared-statements-transactions/1/).

# Install instructions:
There are prebuilt binaries available for [Linux and Windows](https://github.com/syl0r/MySQLOO/releases) (both 32 and 64 bit).
They have to be placed within your garrysmod/lua/bin/ folder (create the bin folder if it does not exist).

**Note**: Previously you were requried to place libmysqlclient.dll besides your srcds executable. This is not required anymore since MySQLOO now links statically against libmysqlclient.



# Documentation


```LUA

-- mysqloo table

mysqloo.connect( host, username, password, database [, port, socket] )
-- returns [Database]
-- Initializes the database object, note that this does not actually connect to the database.

mysqloo.VERSION -- [String] Current MySQLOO version (currently "9")
mysqloo.MINOR_VERSION -- [String] minor version of this library

mysqloo.DATABASE_CONNECTED -- [Number] - Database is connected
mysqloo.DATABASE_CONNECTING -- [Number] - Datbase is connecting
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
-- Returns [Boolean, String]
-- Attempts to set the connection's character set to the one specified.
-- Please note that this does block the main server thread if there is a query currently being ran
-- Returns true on success, false and an error message on failure

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
-- Returns true if aborting was successful, false otherwise

Query:lastInsert()
-- Returns [Number]
-- Gets the autoincrement index of the last inserted row of the current result set

Query:status()
-- Returns [Number] (mysqloo.QUERY_* enums)
-- Gets the status of the query.

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
-- Gets the error caused by the query (if any).

Query:hasMoreResults()
-- Returns [Boolean]
-- Returns true if the query still has more data associated with it (which means getNextResults() can be called)

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
-- This shouldn't be used anymore, just start the same prepared multiple times with different parameters


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

To build the project you first need to generate the appropriate solution for your system using [premake](https://premake.github.io/download.html).

```
premake5 --os=windows --file=BuildProjects.lua vs2017
premake5 --os=macosx --file=BuildProjects.lua gmake
premake5 --os=linux --file=BuildProjects.lua gmake
```
Then building MySQLOO should be as easy as either running make (linux) or pressing the build project button in Visual Studio (windows).
**Note**: To build MySQLOO in 64-bit, run `make config=release_x86_64`
**Note**: On Linux you might have to install some additional libraries required in the linking process, but I personally have not experienced any such issues.
**Note:** Mac is currently not supported since the MariaDB connector is not available on mac (at least not precompiled).
