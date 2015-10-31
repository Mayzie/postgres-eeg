# PostgreSQL EEG Extension

A PostgreSQL extension that can interface with EEG data.

# Prerequisites

  * PostgreSQL headers
  * PGXS (PostgreSQL build system)
  * FFTW3 headers (Fast Fourier Transform library, version 3)

*All packages should be able to be installed via your distributions repositories. Under ArchLinux, these packages are:*

  * postgresql
  * fftw

# Installation

  1. Enter this directory
  2. `make` -- Compiles extension with your local PostgreSQL version (`pg_config` in your `PATH`)
  3. `sudo make install` -- Installs your extension into your PostgreSQL distribution
  4. In a Postgres schema, type (SQL): `CREATE EXTENSION EEG;` to load the extension into your current schema

# Resources

  * Extension Creation Guide: http://www.postgresql.org/docs/9.4/static/extend.html
  * Brief reference on creating custom Postgres data types: http://www.postgresql.org/docs/9.4/static/xtypes.html
  * Information on EEG data: https://en.wikipedia.org/wiki/Quantitative_electroencephalograph

# File Hierarchy

(All files are relative to the root directory)

* `./eeg.c` - PostgreSQL extension C source code file
* `./Makefile` - Build instructions for building and installing the extension (see above, under "Installation" heading)
* `./eeg.control` - Extension definition file
* `./eeg--1.0.sql` - Bridge between the built shared library and the database/developer. Contains all SQL functions and the  `signal` data structure definition
* `./README.eeg` - Documentation on how to use the various functions the extension provides
