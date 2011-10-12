/*-
 * Copyright (c) 2009 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: users/kaiwang27/elftc/libelftc.h 392 2009-05-31 19:17:46Z kaiwang27 $
 */

#ifndef	_LIBELFTC_H_
#define	_LIBELFTC_H_

typedef struct _Bfd_Target Bfd_Target;

/* Target types. */
typedef enum {
	ETF_NONE,
	ETF_ELF,
	ETF_BINARY,
	ETF_SREC,
	ETF_IHEX
} Bfd_Target_Flavor;

/*
 * Demangler flags.
 */

/* Name mangling style. */
#define	ELFTC_DEM_ARM	0x00000001U
#define ELFTC_DEM_GNU2	0x00000002U
#define	ELFTC_DEM_GNU3	0x00000004U

/* Demangling behaviour control. */
#define ELFTC_DEM_NOPARAM	0x00010000U

__BEGIN_DECLS
Bfd_Target		*elftc_bfd_find_target(const char *tgt_name);
Bfd_Target_Flavor	 elftc_bfd_target_flavor(Bfd_Target *tgt);
unsigned int		 elftc_bfd_target_byteorder(Bfd_Target *tgt);
unsigned int		 elftc_bfd_target_class(Bfd_Target *tgt);
unsigned int		 elftc_bfd_target_machine(Bfd_Target *tgt);
int		 	 elftc_demangle(const char *mangledname, char *buffer,
			     size_t bufsize, unsigned int flags);
const char		*elftc_version(void);
__END_DECLS

#endif	/* _LIBELFTC_H_ */
