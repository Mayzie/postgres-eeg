-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION eeg" to load this file. \quit

CREATE TYPE signal;

CREATE OR REPLACE FUNCTION signal_in(cstring) RETURNS signal
AS 'eeg', 'signal_in'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION signal_out(signal) RETURNS cstring
AS 'eeg', 'signal_out'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE signal (
    internallength = VARIABLE,
    input = signal_in,
    output = signal_out,
    alignment = int4
);

CREATE OR REPLACE FUNCTION to_signal(float8[], float8) RETURNS signal
AS 'eeg', 'to_signal'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION get_signal(eeg_id integer, epoch integer, channel integer) RETURNS signal
AS $$
    SELECT to_signal(e.signal::float8[], s.recordduration)
    FROM eegchannelsignal e 
    INNER JOIN shreddedeeg s ON (e.eeg_id = s.eeg_id)
    WHERE e.eeg_id = get_signal.eeg_id
    AND e.epoch = get_signal.epoch
    AND e.channel = get_signal.channel;
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION spectral_power(signal, float8, float8) RETURNS float8
AS 'eeg', 'spectral_power'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION alpha(sgnl signal) RETURNS float8
AS $$
    SELECT spectral_power(sgnl, 7.5, 13);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION beta(sgnl signal) RETURNS float8
AS $$
    SELECT spectral_power(sgnl, 13, 30);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION delta(sgnl signal) RETURNS float8
AS $$
    SELECT spectral_power(sgnl, 1, 4);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION theta(sgnl signal) RETURNS float8
AS $$
    SELECT spectral_power(sgnl, 4, 7.5);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION signal_in_range(min_freq FLOAT8, max_freq FLOAT8, power_min INTEGER, power_max INTEGER)
RETURNS TABLE (eeg_id INTEGER, epoch INTEGER, channel INTEGER, power_freq FLOAT8)
AS $$
    SELECT o.id, o.epoch, o.channel, o.power
    FROM (
        SELECT e.eeg_id, e.epoch, e.channel, spectral_power(get_signal(e.eeg_id, e.epoch, e.channel), min_freq, max_freq)
        FROM eegchannelsignal e
    ) AS o(id, epoch, channel, power)
    WHERE o.power >= power_min
    AND o.power <= power_max;
$$ LANGUAGE SQL;
