EXTENSION = eeg
OBJS = eeg.o

DATA = eeg--1.0.sql
DOCS = README.eeg
MODULE_big = eeg

MODULE_big = eeg

PG_CONFIG = pg_config
SHLIB_LINK += -lfftw3 -lm
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
