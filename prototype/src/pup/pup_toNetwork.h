/*
Pack/unpack to network-byte-order integer/float/double.
On-the-wire sizes correspond to Java sizes for 
int, long, float, and double.

Orion Sky Lawlor, uiuc.edu@acm.org, 11/1/2001
 */
#ifndef __OSL_PUP_TONETWORK_H
#define __OSL_PUP_TONETWORK_H

#include "pup.h"


typedef PUP_TYPEDEF_INT4 PUP_NETWORK_INT4;
#ifdef PUP_LONG_LONG
typedef PUP_LONG_LONG PUP_NETWORK_INT8;
#else
typedef long PUP_NETWORK_INT8; /* long is 8 bytes */
#endif

/// Integer type the same size as "float"
typedef PUP_NETWORK_INT4 PUP_FLOAT_SIZED_INT;
/// Integer type the same size as "double"
typedef PUP_NETWORK_INT8 PUP_DOUBLE_SIZED_INT;


class PUP_toNetwork_sizer : public PUP::er {
	int nBytes;
	virtual void bytes(void *p,int n,size_t itemSize,PUP::dataType t);
 public:
	PUP_toNetwork_sizer(void) :PUP::er(IS_SIZING) {nBytes=0;}
	int size(void) const {return nBytes;}
};

class PUP_toNetwork_pack : public PUP::er {
	unsigned char *buf,*start;
	inline void w(PUP_NETWORK_INT4 i) {
		//Write big-endian integer to output stream
		*buf++=(unsigned char)(i>>24); //High end first
		*buf++=(unsigned char)(i>>16);
		*buf++=(unsigned char)(i>>8);
		*buf++=(unsigned char)(i>>0);  //Low end last
	}
	inline void w(PUP_NETWORK_INT8 i) {
		w(PUP_NETWORK_INT4(i>>32));
		w(PUP_NETWORK_INT4(i));
	}
	//Write floating-point number to stream.
	inline void w(float f) {
		w(*(PUP_FLOAT_SIZED_INT *)&f);
	}
	//Write floating-point number to stream.
	inline void w(double f)  {
		w(*(PUP_DOUBLE_SIZED_INT *)&f);
	}

	virtual void bytes(void *p,int n,size_t itemSize,PUP::dataType t);
 public:
	PUP_toNetwork_pack(void *dest) :PUP::er(IS_PACKING) {
		start=buf=(unsigned char *)dest;
	}
	inline int size(void) const {return buf-start;}
};

class PUP_toNetwork_unpack : public PUP::er {
	const unsigned char *buf,*start;
	inline PUP_NETWORK_INT4 read_int(void) {
		//Read big-endian integer to output stream
		PUP_NETWORK_INT4 ret=(buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
		buf+=4;
		return ret;
	}
	inline PUP_NETWORK_INT8 read_PUP_NETWORK_INT8(void) {
		PUP_NETWORK_INT8 hi=0xffFFffFFu&(PUP_NETWORK_INT8)read_int();
		PUP_NETWORK_INT8 lo=0xffFFffFFu&(PUP_NETWORK_INT8)read_int();
		return (hi<<32)|(lo);
	}
	inline float read_float(void) {
		PUP_NETWORK_INT4 i=read_int();
		return *(float *)&i;
	}
	inline double read_double(void) {
		PUP_NETWORK_INT8 i=read_PUP_NETWORK_INT8();
		return *(double *)&i;
	}

	virtual void bytes(void *p,int n,size_t itemSize,PUP::dataType t);
 public:
	PUP_toNetwork_unpack(const void *src) :PUP::er(IS_UNPACKING) {
		start=buf=(const unsigned char *)src;
	}
	inline int size(void) const {return buf-start;}
};

#endif

