# MySQLOO Lua Integration Tests

This folder contains integration tests for MySQLOO.

## Running

- Place this folder into the server's addons folder.
- Adjust the database settings in lua/autorun/server/init.lua
  - ensure that the database used is **empty** as it will be filled with test data
- run `mysqloo_start_tests` in the server console

Each of the tests outputs its result to the console and at the end there is an overview of all tests printed.