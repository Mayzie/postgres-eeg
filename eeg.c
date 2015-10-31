#include <postgres.h>
#include <fmgr.h>
#include <utils/array.h>
#include <fftw3.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/**
 * Signal type definition
 **/
typedef struct {
	char vl_len_[4];
	int32 length;
	float8 duration;
	float8 signal[1];
} Signal;

/**
 * Signal input and output functions
 **/
PG_FUNCTION_INFO_V1(signal_in);

Datum signal_in(PG_FUNCTION_ARGS) {
	char *str = PG_GETARG_CSTRING(0);
	Signal *result;
	int N;
	float8 duration;
	float8 x;
	int chars_read;
	int total_chars_read;
	char lastchar;
	int datatype_length;
	int i;

	if (sscanf(str, "%lf%n", &duration, &chars_read) != 1) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for signal: \"%s\". Duration must be specified first", str)));
	}
	total_chars_read = chars_read;

	if (sscanf(str+total_chars_read, " [ %lf%n", &x, &chars_read) != 1) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for signal: \"%s\". Signal must be comma separated within []", str)));
	}
	total_chars_read += chars_read;
	
	N = 1;

	while (sscanf(str+total_chars_read, " , %lf%n", &x, &chars_read) == 1) {
		N++;
		total_chars_read += chars_read;
	}

	sscanf(str+total_chars_read, " %c%n ", &lastchar, &chars_read);
	if (lastchar != ']') {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for signal: \"%s\". Signal must end with ]. Instead got %c", str, lastchar)));
	}
	total_chars_read += chars_read;
	if (sscanf(str+total_chars_read, " %c ", &lastchar) != EOF) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for signal: \"%s\". Signal must end with ]. Instead got %c", str, lastchar)));
	}

	datatype_length = VARHDRSZ + sizeof(int32) + sizeof(float8) + N*sizeof(float8);
	result = (Signal*) palloc(datatype_length);
	SET_VARSIZE(result, datatype_length);

	result->length = N;
	result->duration = duration;

	sscanf(str, "%*f%n", &chars_read);
	total_chars_read = chars_read;
	sscanf(str + total_chars_read, " [ %lf%n", &(result->signal[0]), &chars_read);
	total_chars_read += chars_read;
	
	for(i=1; i<N; i++) {
		sscanf(str + total_chars_read, " , %lf%n", &(result->signal[i]), &chars_read);
		total_chars_read += chars_read;
	}

	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(signal_out);

Datum signal_out(PG_FUNCTION_ARGS) {
	Signal *signal;
	char *result;
	char *temp;
	char *p;
	int out_length;
	int i;
	
    signal = (Signal*) PG_GETARG_POINTER(0);

	out_length = strlen(psprintf("%.10g ", signal->duration));
	out_length += strlen(psprintf("[%.10g", signal->signal[0]));
	for (i = 1; i < signal->length; i++) {
		out_length += strlen(psprintf(", %.10g", signal->signal[i]));
	}
	out_length += 1; // close bracket

	result = (char*) palloc(out_length);
	p = result;
	temp = psprintf("%.10g ", signal->duration);
	strcpy(p, temp);
	p += strlen(temp);

	temp = psprintf("[%.10g", signal->signal[0]);
	strcpy(p, temp);
	p += strlen(temp);

	for (i = 1; i < signal->length; i++) {
		temp = psprintf(", %.10g", signal->signal[i]);
		strcpy(p, temp);
		p += strlen(temp);
	}

	strcpy(p, "]");

	PG_RETURN_CSTRING(result);
}


/**
 * Returns the spectral power.
 **/
PG_FUNCTION_INFO_V1(spectral_power);
Datum spectral_power(PG_FUNCTION_ARGS) {
	Signal *signal;
	float8 freq_min;
	float8 freq_max;
	float8 time;
	double* in;
	fftw_complex* out;
	fftw_plan plan;
	int N;
	int i;
	float8 power;
	int k_min;
	int k_max;
	int k;
	
	signal = (Signal*) PG_GETARG_POINTER(0);
	freq_min = PG_GETARG_FLOAT8(1);
	freq_max = PG_GETARG_FLOAT8(2);
	time = signal->duration;
	N = signal->length;

	// get double array from arg (smallint?)
	in = (double*) fftw_malloc(sizeof(double) * N);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
	plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

	for (i=0; i < N; i++) {
		in[i] = signal->signal[i];
	}
	
	fftw_execute(plan);

	// At this point we have frequency domain
	
	// Calculate smallest k with frequency >= freq_min
	// Calculate largest k in frequency <= freq_max
	k_min = floor(freq_min * time) + 1;
	k_max = floor(freq_max * time);
	if (k_min < 0) {
		k_min = 0;
	}
	if (k_max > N/2) {
		k_max = N/2;
	}

	power = 0;
	for (k = k_min; k <= k_max; k++) {
		power += out[k][0]*out[k][0] + out[k][1]*out[k][1];
	}
	
	fftw_destroy_plan(plan);
	fftw_free(in);
	fftw_free(out);

	// return power
	PG_RETURN_FLOAT8(power);
}

/**
 * Takes a float8[] and a duration and converts it to a signal
 **/
PG_FUNCTION_INFO_V1(to_signal);
Datum to_signal(PG_FUNCTION_ARGS) {
	ArrayType *signal_array;
	float8 *signal_data;
	float8 duration;
	int N;
	Signal *signal;
	int datatype_length;

	signal_array = PG_GETARG_ARRAYTYPE_P(0);
	signal_data = (float8*) ARR_DATA_PTR(signal_array);
	N = ARR_DIMS(signal_array)[0];

	duration = PG_GETARG_FLOAT8(1);

	datatype_length = VARHDRSZ + sizeof(int32) + sizeof(float8) + N*sizeof(float8);
	signal = (Signal*) palloc(datatype_length);
	SET_VARSIZE(signal, datatype_length);

	signal->length = N;
	signal->duration = duration;

	memcpy(signal->signal, signal_data, N*sizeof(float8));

	PG_RETURN_POINTER(signal);
}

/* vim: noexpandtab tabstop=4 shiftwidth=4
*/
