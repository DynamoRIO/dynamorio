/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "linux_defines.h"
#include "table_defines.h"

/* From "man ioctl_list" */
/* FIXME: "Some ioctls take a pointer to a structure which contains
 * additional pointers."  These are marked below with "FIXME: more".
 * They are listed in the man page but I'm too lazy to add them just now.
 * Some of the ioctls marked "more" take additional arguments as well.
 */

/* The name of the ioctl is "ioctl.name".  To avoid long lines from repeating
 * the request name we wrap it in a macro.
 */
#define IOCTL(request) {PACKNUM(16,54,54, 29), request}, "ioctl." STRINGIFY(request)

/* All ioctls take fd and request as the first two args. */
#define FD_REQ \
    {0, sizeof(int), SYSARG_INLINED, INT_TYPE}, /* fd */ \
    {1, sizeof(int), SYSARG_INLINED, INT_TYPE}  /* request */

#define INT_TYPE DRSYS_TYPE_SIGNED_INT
#define UINT_TYPE DRSYS_TYPE_UNSIGNED_INT

syscall_info_t syscall_ioctl_info[] = {
    // <include/asm-i386/socket.h>
    {IOCTL(FIOSETOWN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SIOCSPGRP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(FIOGETOWN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SIOCGPGRP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SIOCATMARK), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SIOCGSTAMP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct timeval), W}}},

    // <include/asm-i386/termios.h>
    {IOCTL(TCGETS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), W}}},
    {IOCTL(TCSETS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
    {IOCTL(TCSETSW), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios ), R}}},
    {IOCTL(TCSETSF), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
    {IOCTL(TCGETA), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), W}}},
    {IOCTL(TCSETA), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
    {IOCTL(TCSETAW), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
    {IOCTL(TCSETAF), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
    {IOCTL(TCSBRK), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(TCXONC), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(TCFLSH), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(TIOCEXCL), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCNXCL), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCSCTTY), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(TIOCGPGRP), OK, RLONG, 3, {FD_REQ, {2, sizeof(pid_t), W, INT_TYPE}}},
    {IOCTL(TIOCSPGRP), OK, RLONG, 3, {FD_REQ, {2, sizeof(pid_t), R, INT_TYPE}}},
    {IOCTL(TIOCOUTQ), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCSTI), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), R, INT_TYPE}}},
    {IOCTL(TIOCGWINSZ), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct winsize), W}}},
    {IOCTL(TIOCSWINSZ), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct winsize), R}}},
    {IOCTL(TIOCMGET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCMBIS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCMBIC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCMSET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCGSOFTCAR), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCSSOFTCAR), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCINQ /*== FIONREAD*/), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCLINUX), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), R, INT_TYPE}}}, /* FIXME: more */
    {IOCTL(TIOCCONS), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCGSERIAL), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct serial_struct), W}}},
    {IOCTL(TIOCSSERIAL), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct serial_struct), R}}},
    {IOCTL(TIOCPKT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(FIONBIO), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCNOTTY), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCSETD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCGETD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TCSBRKP), OK, RLONG, 3, {FD_REQ, /* int */}},
#if 0 /* FIXME: struct not in my headers */
    {IOCTL(TIOCTTYGSTRUCT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct tty_struct), W}}},
#endif
    {IOCTL(FIONCLEX), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FIOCLEX), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FIOASYNC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCSERCONFIG), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCSERGWILD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCSERSWILD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(TIOCGLCKTRMIOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), W}}},
    {IOCTL(TIOCSLCKTRMIOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct termios), R}}},
#if 0 /* FIXME: struct not in my headers */
    {IOCTL(TIOCSERGSTRUCT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct async_struct), W}}},
#endif
    {IOCTL(TIOCSERGETLSR), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(TIOCSERGETMULTI), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct serial_multiport_struct), W}}},
    {IOCTL(TIOCSERSETMULTI), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct serial_multiport_struct), R}}},

    // <include/linux/ax25.h>
    {IOCTL(SIOCAX25GETUID), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sockaddr_ax25), R}}},
    {IOCTL(SIOCAX25ADDUID), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sockaddr_ax25), R}}},
    {IOCTL(SIOCAX25DELUID), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sockaddr_ax25), R}}},
    {IOCTL(SIOCAX25NOUID), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(SIOCAX25DIGCTL), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SIOCAX25GETPARMS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ax25_parms_struct), R|W}}},
    {IOCTL(SIOCAX25SETPARMS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ax25_parms_struct), R}}},
#endif

    // <include/linux/cdk.h>
    {IOCTL(STL_BINTR), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(STL_BSTART), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(STL_BSTOP), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(STL_BRESET), OK, RLONG, 3, {FD_REQ, /* void */}},

    // <include/linux/cdrom.h>
    {IOCTL(CDROMPAUSE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMRESUME), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMPLAYMSF), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}},
    {IOCTL(CDROMPLAYTRKIND), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_ti), R}}},
    {IOCTL(CDROMREADTOCHDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_tochdr), W}}},
    {IOCTL(CDROMREADTOCENTRY), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_tocentry), R|W}}},
    {IOCTL(CDROMSTOP), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMSTART), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMEJECT), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMVOLCTRL), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_volctrl), R}}},
    {IOCTL(CDROMSUBCHNL), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_subchnl), R|W}}},
    {IOCTL(CDROMREADMODE2), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}}, /* FIXME: more */
    {IOCTL(CDROMREADMODE1), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}}, /* FIXME: more */
    {IOCTL(CDROMREADAUDIO), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_read_audio), R}}}, /* FIXME: more */
    {IOCTL(CDROMEJECT_SW), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(CDROMMULTISESSION), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_multisession), R|W}}},
    {IOCTL(CDROM_GET_UPC), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[8]), W}}},
    {IOCTL(CDROMRESET), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(CDROMVOLREAD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_volctrl), W}}},
    {IOCTL(CDROMREADRAW), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}}, /* FIXME: more */
    {IOCTL(CDROMREADCOOKED), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}}, /* FIXME: more */
    {IOCTL(CDROMSEEK), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cdrom_msf), R}}},

    // <include/linux/cm206.h>
#if 0 /* FIXME: define not in my headers */
    {IOCTL(CM206CTL_GET_STAT), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(CM206CTL_GET_LAST_STAT), OK, RLONG, 3, {FD_REQ, /* int */}},
#endif

    // <include/linux/cyclades.h>
#if 0 /* XXX: cycles has been removed from the kernel */
    {IOCTL(CYGETMON), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct cyclades_monitor), W}}},
    {IOCTL(CYGETTHRESH), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(CYSETTHRESH), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(CYGETDEFTHRESH), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(CYSETDEFTHRESH), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(CYGETTIMEOUT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(CYSETTIMEOUT), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(CYGETDEFTIMEOUT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(CYSETDEFTIMEOUT), OK, RLONG, 3, {FD_REQ, /* int */}},
#endif

    // <include/linux/ext2_fs.h>
    {IOCTL(EXT2_IOC_GETFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(EXT2_IOC_SETFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(EXT2_IOC_GETVERSION), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(EXT2_IOC_SETVERSION), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},

    // <include/linux/fd.h>
    {IOCTL(FDCLRPRM), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDSETPRM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_struct), R}}},
    {IOCTL(FDDEFPRM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_struct), R}}},
    {IOCTL(FDGETPRM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_struct), W}}},
    {IOCTL(FDMSGON), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDMSGOFF), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDFMTBEG), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDFMTTRK), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct format_descr), R}}},
    {IOCTL(FDFMTEND), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDSETEMSGTRESH), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(FDFLUSH), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDSETMAXERRS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_max_errors), R}}},
    {IOCTL(FDGETMAXERRS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_max_errors), W}}},
    {IOCTL(FDGETDRVTYP), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[16]), W}}},
    {IOCTL(FDSETDRVPRM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_drive_params), R}}},
    {IOCTL(FDGETDRVPRM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_drive_params), W}}},
    {IOCTL(FDGETDRVSTAT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_drive_struct), W}}},
    {IOCTL(FDPOLLDRVSTAT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_drive_struct), W}}},
    {IOCTL(FDRESET), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(FDGETFDCSTAT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_fdc_state), W}}},
    {IOCTL(FDWERRORCLR), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(FDWERRORGET), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_write_errors), W}}},
    {IOCTL(FDRAWCMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct floppy_raw_cmd), R|W}}}, /* FIXME: more */
    {IOCTL(FDTWADDLE), OK, RLONG, 3, {FD_REQ, /* void */}},

    // <include/linux/fs.h>
    {IOCTL(BLKROSET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(BLKROGET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(BLKRRPART), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(BLKGETSIZE), OK, RLONG, 3, {FD_REQ, {2, sizeof(unsigned long), W, UINT_TYPE}}},
    {IOCTL(BLKFLSBUF), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(BLKRASET), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(BLKRAGET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(FIBMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(FIGETBSZ), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},

    // <include/linux/hdreg.h>
    {IOCTL(HDIO_GETGEO), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct hd_geometry), W}}},
    {IOCTL(HDIO_GET_UNMASKINTR), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(HDIO_GET_MULTCOUNT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(HDIO_GET_IDENTITY), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct hd_driveid), W}}},
    {IOCTL(HDIO_GET_KEEPSETTINGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(HDIO_GET_CHIPSET), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
#endif
    {IOCTL(HDIO_GET_NOWERR), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(HDIO_GET_DMA), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(HDIO_DRIVE_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(HDIO_SET_MULTCOUNT), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(HDIO_SET_UNMASKINTR), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(HDIO_SET_KEEPSETTINGS), OK, RLONG, 3, {FD_REQ, /* int */}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(HDIO_SET_CHIPSET), OK, RLONG, 3, {FD_REQ, /* int */}},
#endif
    {IOCTL(HDIO_SET_NOWERR), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(HDIO_SET_DMA), OK, RLONG, 3, {FD_REQ, /* int */}},

#if 0 /* FIXME: having problems including header */
    // <include/linux/if_eql.h>
    {IOCTL(EQL_ENSLAVE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
    {IOCTL(EQL_EMANCIPATE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
    {IOCTL(EQL_GETSLAVECFG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
    {IOCTL(EQL_SETSLAVECFG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
    {IOCTL(EQL_GETMASTRCFG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
    {IOCTL(EQL_SETMASTRCFG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}}, /* FIXME: more */
#endif

    // <include/linux/if_plip.h>
    {IOCTL(SIOCDEVPLIP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},

#if 0 /* FIXME: having problems including header */
    // <include/linux/if_ppp.h>
    {IOCTL(PPPIOCGFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(PPPIOCSFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCGASYNCMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(PPPIOCSASYNCMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCGUNIT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(PPPIOCSINPSIG), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCSDEBUG), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCGDEBUG), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(PPPIOCGSTAT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ppp_stats), W}}},
    {IOCTL(PPPIOCGTIME), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ppp_ddinfo), W}}},
    {IOCTL(PPPIOCGXASYNCMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct { int [8]; }), W}}},
    {IOCTL(PPPIOCSXASYNCMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct { int [8]; }), R}}},
    {IOCTL(PPPIOCSMRU), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCRASYNCMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(PPPIOCSMAXCID), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
#endif

#if 0 /* FIXME: identical to ax25 1st 3 */
    // <include/linux/ipx.h>
    {IOCTL(SIOCAIPXITFCRT), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), R, INT_TYPE}}},
    {IOCTL(SIOCAIPXPRISLT), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), R, INT_TYPE}}},
    {IOCTL(SIOCIPXCFGDATA), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipx_config_data), W}}},
#endif

    // <include/linux/kd.h>
    {IOCTL(GIO_FONT), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[8192]), W}}},
    {IOCTL(PIO_FONT), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[8192]), R}}},
#if 0 /* FIXME: struct not in my defines */
    {IOCTL(GIO_FONTX), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct console_font_desc), R|W}}}, /* FIXME: more */
    {IOCTL(PIO_FONTX), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct console_font_desc), R}}}, /* FIXME: more */
#endif
    {IOCTL(GIO_CMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[48]), W}}},
    {IOCTL(PIO_CMAP), OK, RLONG, 3, {FD_REQ, /* const struct { char [48]; } */}},
    {IOCTL(KIOCSOUND), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDMKTONE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGETLED), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), W, INT_TYPE}}},
    {IOCTL(KDSETLED), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGKBTYPE), OK, RLONG, 3, {FD_REQ, {2, sizeof(char), W, INT_TYPE}}},
    {IOCTL(KDADDIO), OK, RLONG, 3, {FD_REQ, /* int */}}, /* FIXME: more */
    {IOCTL(KDDELIO), OK, RLONG, 3, {FD_REQ, /* int */}}, /* FIXME: more */
    {IOCTL(KDENABIO), OK, RLONG, 3, {FD_REQ, /* void */}}, /* FIXME: more */
    {IOCTL(KDDISABIO), OK, RLONG, 3, {FD_REQ, /* void */}}, /* FIXME: more */
    {IOCTL(KDSETMODE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGETMODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(KDMAPDISP), OK, RLONG, 3, {FD_REQ, /* void */}}, /* FIXME: more */
    {IOCTL(KDUNMAPDISP), OK, RLONG, 3, {FD_REQ, /* void */}}, /* FIXME: more */
    {IOCTL(GIO_SCRNMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[E_TABSZ]), W}}},
    {IOCTL(PIO_SCRNMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(char[E_TABSZ]), R}}},
    {IOCTL(GIO_UNISCRNMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(short[E_TABSZ]), W}}},
    {IOCTL(PIO_UNISCRNMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(short[E_TABSZ]), R}}},
    {IOCTL(GIO_UNIMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct unimapdesc), R|W}}}, /* FIXME: more */
    {IOCTL(PIO_UNIMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct unimapdesc), R}}}, /* FIXME: more */
    {IOCTL(PIO_UNIMAPCLR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct unimapinit), R}}},
    {IOCTL(KDGKBMODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(KDSKBMODE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGKBMETA), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(KDSKBMETA), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGKBLED), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(KDSKBLED), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(KDGKBENT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbentry), R|W}}},
    {IOCTL(KDSKBENT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbentry), R}}},
    {IOCTL(KDGKBSENT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbsentry), R|W}}},
    {IOCTL(KDSKBSENT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbsentry), R}}},
    {IOCTL(KDGKBDIACR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbdiacrs), W}}},
    {IOCTL(KDSKBDIACR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbdiacrs), R}}},
    {IOCTL(KDGETKEYCODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbkeycode), R|W}}},
    {IOCTL(KDSETKEYCODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct kbkeycode), R}}},
    {IOCTL(KDSIGACCEPT), OK, RLONG, 3, {FD_REQ, /* int */}},

    // <include/linux/lp.h>
    {IOCTL(LPCHAR), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPTIME), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPABORT), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPSETIRQ), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPGETIRQ), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(LPWAIT), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPCAREFUL), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPABORTOPEN), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(LPGETSTATUS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(LPRESET), OK, RLONG, 3, {FD_REQ, /* void */}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(LPGETSTATS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct lp_stats), W}}},
#endif

#if 0 /* FIXME: identical to ax25 1st 2 */
    // <include/linux/mroute.h>
    {IOCTL(SIOCGETVIFCNT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sioc_vif_req), R|W}}},
    {IOCTL(SIOCGETSGCNT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sioc_sg_req), R|W}}},
#endif

#ifndef ANDROID
    // <include/linux/mtio.h>
    {IOCTL(MTIOCTOP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mtop), R}}},
    {IOCTL(MTIOCGET), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mtget), W}}},
    {IOCTL(MTIOCPOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mtpos), W}}},
    {IOCTL(MTIOCGETCONFIG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mtconfiginfo), W}}},
    {IOCTL(MTIOCSETCONFIG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mtconfiginfo), R}}},
#endif

#if 0 /* FIXME: define not in my headers */
    // <include/linux/netrom.h>
    {IOCTL(SIOCNRGETPARMS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct nr_parms_struct), R|W}}},
    {IOCTL(SIOCNRSETPARMS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct nr_parms_struct), R}}},
    {IOCTL(SIOCNRDECOBS), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SIOCNRRTCTL), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
#endif

#if 0 /* FIXME: define not in my headers */
    // <include/linux/sbpcd.h>
    {IOCTL(DDIOCSDBG), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(CDROMAUDIOBUFSIZ), OK, RLONG, 3, {FD_REQ, /* int */}},
#endif

#if 0 /* FIXME: define not in my headers */
    // <include/linux/scc.h>
    {IOCTL(TIOCSCCINI), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(TIOCCHANINI), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct scc_modem), R}}},
    {IOCTL(TIOCGKISS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ioctl_command), R|W}}},
    {IOCTL(TIOCSKISS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ioctl_command), R}}},
    {IOCTL(TIOCSCCSTAT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct scc_stat), W}}},
#endif

#if 0 /* FIXME: define not in my headers */
    // <include/linux/scsi.h>
    {IOCTL(SCSI_IOCTL_GET_IDLUN), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct { int [2]; }), W}}},
    {IOCTL(SCSI_IOCTL_TAGGED_ENABLE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SCSI_IOCTL_TAGGED_DISABLE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SCSI_IOCTL_PROBE_HOST), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}}, /* FIXME: more */
#endif

    // <include/linux/smb_fs.h>
    {IOCTL(SMB_IOC_GETMOUNTUID), OK, RLONG, 3, {FD_REQ, {2, sizeof(uid_t), W, UINT_TYPE}}},

    // <include/linux/sockios.h>
    {IOCTL(SIOCADDRT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct rtentry), R}}}, /* FIXME: more */
    {IOCTL(SIOCDELRT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct rtentry), R}}}, /* FIXME: more */
    {IOCTL(SIOCGIFCONF), OK, RLONG, 3, {FD_REQ, /* handled manually */}},
    {IOCTL(SIOCGIFNAME), OK, RLONG, 3, {FD_REQ, /* char [] */}},
    {IOCTL(SIOCSIFLINK), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SIOCGIFFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFFLAGS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFDSTADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFDSTADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFBRDADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFBRDADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFNETMASK), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFNETMASK), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFMETRIC), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFMETRIC), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFMEM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFMEM), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCGIFMTU), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFMTU), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(OLD_SIOCGIFHWADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
#endif
    {IOCTL(SIOCSIFHWADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}}, /* FIXME: more */
    {IOCTL(SIOCGIFENCAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SIOCSIFENCAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SIOCGIFHWADDR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCGIFSLAVE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SIOCSIFSLAVE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SIOCADDMULTI), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
    {IOCTL(SIOCDELMULTI), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(SIOCADDRTOLD), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SIOCDELRTOLD), OK, RLONG, 3, {FD_REQ, /* void */}},
#endif
    {IOCTL(SIOCDARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R}}},
    {IOCTL(SIOCGARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R|W}}},
    {IOCTL(SIOCSARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R}}},
    {IOCTL(SIOCDRARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R}}},
    {IOCTL(SIOCGRARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R|W}}},
    {IOCTL(SIOCSRARP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct arpreq), R}}},
    {IOCTL(SIOCGIFMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R|W}}},
    {IOCTL(SIOCSIFMAP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ifreq), R}}},

    // <include/linux/soundcard.h>
    {IOCTL(SNDCTL_SEQ_RESET), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_SEQ_SYNC), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_SYNTH_INFO), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct synth_info), R|W}}},
    {IOCTL(SNDCTL_SEQ_CTRLRATE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_GETOUTCOUNT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_GETINCOUNT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_PERCMODE), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_FM_LOAD_INSTR), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct sbi_instrument), R}}},
    {IOCTL(SNDCTL_SEQ_TESTMIDI), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_RESETSAMPLES), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_NRSYNTHS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_SEQ_NRMIDIS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_MIDI_INFO), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct midi_info), R|W}}},
    {IOCTL(SNDCTL_SEQ_THRESHOLD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SNDCTL_SYNTH_MEMAVL), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_FM_4OP_ENABLE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(SNDCTL_PMGR_ACCESS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct patmgr_info), R|W}}},
#endif
    {IOCTL(SNDCTL_SEQ_PANIC), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_SEQ_OUTOFBAND), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct seq_event_rec), R}}},
    {IOCTL(SNDCTL_TMR_TIMEBASE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
#if 0 /* FIXME: identical to TCSETS and subsequent 2 */
    {IOCTL(SNDCTL_TMR_START), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_TMR_STOP), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_TMR_CONTINUE), OK, RLONG, 3, {FD_REQ, /* void */}},
#endif
    {IOCTL(SNDCTL_TMR_TEMPO), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_TMR_SOURCE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_TMR_METRONOME), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(SNDCTL_TMR_SELECT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
#if 0 /* FIXME: define not in my headers */
    {IOCTL(SNDCTL_PMGR_IFACE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct patmgr_info), R|W}}},
#endif
    {IOCTL(SNDCTL_MIDI_PRETIME), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_MIDI_MPUMODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
#if 0 /* FIXME: struct not in my headers */
    {IOCTL(SNDCTL_MIDI_MPUCMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct mpu_command_rec), R|W}}},
#endif
    {IOCTL(SNDCTL_DSP_RESET), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_DSP_SYNC), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_DSP_SPEED), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_STEREO), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_GETBLKSIZE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_PCM_WRITE_CHANNELS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_PCM_WRITE_FILTER), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_POST), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_DSP_SUBDIVIDE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_SETFRAGMENT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_GETFMTS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_SETFMT), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SNDCTL_DSP_GETOSPACE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct audio_buf_info), W}}},
    {IOCTL(SNDCTL_DSP_GETISPACE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct audio_buf_info), W}}},
    {IOCTL(SNDCTL_DSP_NONBLOCK), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SOUND_PCM_READ_RATE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_PCM_READ_CHANNELS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_PCM_READ_BITS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_PCM_READ_FILTER), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SNDCTL_COPR_RESET), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(SNDCTL_COPR_LOAD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_buffer), R}}},
    {IOCTL(SNDCTL_COPR_RDATA), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R|W}}},
    {IOCTL(SNDCTL_COPR_RCODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R|W}}},
    {IOCTL(SNDCTL_COPR_WDATA), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R}}},
    {IOCTL(SNDCTL_COPR_WCODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R}}},
    {IOCTL(SNDCTL_COPR_RUN), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R|W}}},
    {IOCTL(SNDCTL_COPR_HALT), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_debug_buf), R|W}}},
    {IOCTL(SNDCTL_COPR_SENDMSG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_msg), R}}},
    {IOCTL(SNDCTL_COPR_RCVMSG), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct copr_msg), W}}},
    {IOCTL(SOUND_MIXER_READ_VOLUME), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_BASS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_TREBLE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_SYNTH), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_PCM), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_SPEAKER), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_LINE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_MIC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_CD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_IMIX), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_ALTPCM), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_RECLEV), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_IGAIN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_OGAIN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_LINE1), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_LINE2), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_LINE3), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_MUTE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
#if 0 /* FIXME: identical to SOUND_MIXER_READ_MUTE */
    {IOCTL(SOUND_MIXER_READ_ENHANCE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_LOUD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
#endif
    {IOCTL(SOUND_MIXER_READ_RECSRC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_DEVMASK), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_RECMASK), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_STEREODEVS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_READ_CAPS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_VOLUME), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_BASS), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_TREBLE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_SYNTH), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_PCM), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_SPEAKER), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_LINE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_MIC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_CD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_IMIX), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_ALTPCM), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_RECLEV), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_IGAIN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_OGAIN), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_LINE1), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_LINE2), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_LINE3), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_MUTE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
#if 0 /* FIXME: identical to SOUND_MIXER_WRITE_MUTE */
    {IOCTL(SOUND_MIXER_WRITE_ENHANCE), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
    {IOCTL(SOUND_MIXER_WRITE_LOUD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},
#endif
    {IOCTL(SOUND_MIXER_WRITE_RECSRC), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R|W, INT_TYPE}}},

#if 0 /* FIXME: define not in my headers */
    // <include/linux/umsdos_fs.h>
    {IOCTL(UMSDOS_READDIR_DOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R|W}}},
    {IOCTL(UMSDOS_UNLINK_DOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
    {IOCTL(UMSDOS_RMDIR_DOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
    {IOCTL(UMSDOS_STAT_DOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R|W}}},
    {IOCTL(UMSDOS_CREAT_EMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
    {IOCTL(UMSDOS_UNLINK_EMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
    {IOCTL(UMSDOS_READDIR_EMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R|W}}},
    {IOCTL(UMSDOS_GETVERSION), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), W}}},
    {IOCTL(UMSDOS_INIT_EMD), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(UMSDOS_DOS_SETUP), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
    {IOCTL(UMSDOS_RENAME_DOS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct umsdos_ioctl), R}}},
#endif

    // <include/linux/vt.h>
    {IOCTL(VT_OPENQRY), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(VT_GETMODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct vt_mode), W}}},
    {IOCTL(VT_SETMODE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct vt_mode), R}}},
    {IOCTL(VT_GETSTATE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct vt_stat), W}}},
    {IOCTL(VT_SENDSIG), OK, RLONG, 3, {FD_REQ, /* void */}},
    {IOCTL(VT_RELDISP), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(VT_ACTIVATE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(VT_WAITACTIVE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(VT_DISALLOCATE), OK, RLONG, 3, {FD_REQ, /* int */}},
    {IOCTL(VT_RESIZE), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct vt_sizes), R}}},
    {IOCTL(VT_RESIZEX), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct vt_consize), R}}},

    /* include <linux/ipmi.h> PR 531644 */
    {IOCTL(IPMICTL_SEND_COMMAND), OK, RLONG, 3, {FD_REQ, /* handled manually */}},
    {IOCTL(IPMICTL_SEND_COMMAND_SETTIME), OK, RLONG, 3, {FD_REQ, /* handled manually */}},
    {IOCTL(IPMICTL_RECEIVE_MSG), OK, RLONG, 3, {FD_REQ, /* handled manually */}},
    {IOCTL(IPMICTL_RECEIVE_MSG_TRUNC), OK, RLONG, 3, {FD_REQ, /* handled manually */}},
    {IOCTL(IPMICTL_REGISTER_FOR_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_cmdspec), R}}},
    {IOCTL(IPMICTL_UNREGISTER_FOR_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_cmdspec), R}}},
    {IOCTL(IPMICTL_REGISTER_FOR_CMD_CHANS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_cmdspec_chans), R}}},
    {IOCTL(IPMICTL_UNREGISTER_FOR_CMD_CHANS), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_cmdspec_chans), R}}},
    {IOCTL(IPMICTL_SET_GETS_EVENTS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
    {IOCTL(IPMICTL_SET_MY_CHANNEL_ADDRESS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_channel_lun_address_set), R}}},
    {IOCTL(IPMICTL_GET_MY_CHANNEL_ADDRESS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_channel_lun_address_set), W}}},
    {IOCTL(IPMICTL_SET_MY_CHANNEL_LUN_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_channel_lun_address_set), R}}},
    {IOCTL(IPMICTL_GET_MY_CHANNEL_LUN_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_channel_lun_address_set), W}}},
    {IOCTL(IPMICTL_SET_MY_ADDRESS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(uint), R, UINT_TYPE}}},
    {IOCTL(IPMICTL_GET_MY_ADDRESS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(uint), W, UINT_TYPE}}},
    {IOCTL(IPMICTL_SET_MY_LUN_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(uint), R, UINT_TYPE}}},
    {IOCTL(IPMICTL_GET_MY_LUN_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(uint), W, UINT_TYPE}}},
    {IOCTL(IPMICTL_SET_TIMING_PARMS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_timing_parms), R}}},
    {IOCTL(IPMICTL_GET_TIMING_PARMS_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(struct ipmi_timing_parms), W}}},
    {IOCTL(IPMICTL_GET_MAINTENANCE_MODE_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), W, INT_TYPE}}},
    {IOCTL(IPMICTL_SET_MAINTENANCE_MODE_CMD), OK, RLONG, 3, {FD_REQ, {2, sizeof(int), R, INT_TYPE}}},
};

size_t count_syscall_ioctl_info = BUFFER_SIZE_ELEMENTS(syscall_ioctl_info);
