/*Standard ASF Utility include file.*/
#ifndef __ASF_H
#define __ASF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "caplib.h"
#include "asf_meta.h"
#include "error.h"
#include "log.h"
#include "cla.h"

#ifndef PI
# define PI 3.14159265358979323846
#endif
#ifndef M_PI
# define M_PI PI
#endif 
#define D2R (PI/180.0)
#define R2D (180.0/PI)

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/* The maximum allowable length in characters (not including trailing
   null character )of result strings from the appendExt function.  */
#define MAX_APPENDEXT_RESULT_STRING_LENGTH 255

/* Print an error with printf-style formatting codes and args, then die.  */
void bail(const char *message, ...)/* ; is coming, don't worry.  */
/* The GNU C compiler can give us some special help if compiling with
   -Wformat or a warning option that implies it.  */
#ifdef __GNUC__
     /* Function attribute format says function is variadic al la
        printf, with argument 1 its format spec and argument 2 its
        first optional argument corresponding to the spec.  Function
        attribute noreturn says function doesn't return.  */
     __attribute__ ((format (printf, 1, 2), noreturn))
#endif 
; /* <-- Semicolon for bail prototype.  */
void StartWatch(void);
void StartWatchLog(FILE *fLog);
void StopWatch(void);
void StopWatchLog(FILE *fLog);

/*****************************************
 * FileUtil:
 * A collection of file I/O utilities. Implemented in asf.a/fileUtil.c */
int extExists(const char *name,const char *newExt);
int fileExists(const char *name);
char *findExt(char *name);
char *appendExt(const char *name,const char *newExt);
void create_name(char *out,const char *in,const char *newExt);
FILE *fopenImage(const char *name,const char *accessType);


/*****************************************
 * ioLine:
 * Grab a a line of any data type and fill a buffer of _type_ data. Puts data in
 * correct endian order. Implemented in asf.a/ioLine.c */
int get_float_line(FILE *file, meta_parameters *meta, int line_number,
		float *dest);
int put_float_line(FILE *file, meta_parameters *meta, int line_number,
		const float *source);
int get_double_line(FILE *file, meta_parameters *meta, int line_number,
		double *dest);
int put_double_line(FILE *file, meta_parameters *meta, int line_number,
		const double *source);

#endif
