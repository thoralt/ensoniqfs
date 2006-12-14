/****************************************************************************/
/*																			*/
/*	NAME	  : OmniFlop													*/
/*																			*/
/*	FUNCTION  : Definitions for the public interfaces to OmniFlop			*/
/*																			*/
/*	CREATED	  :	31 Dec 2004		Jason Watton								*/
/*																			*/
/*	NOTES	  :	Edit using tabstops of 4 for correct alignment.				*/
/*				.c header files must not use C++ language, e.g. // comment	*/
/*																			*/
/****************************************************************************/
/*																			*/
/*	COPYRIGHT:																*/
/*	This source code and the associated object file are copyright (C) 2004	*/
/*	Jason Watton. Use, duplication and disclosure are subject to a License	*/
/*	Agreement with the copyright holder.									*/
/*																			*/
/*	The author of this work is Jason Watton, who asserts his right to be	*/
/*	identified as such.														*/
/*																			*/
/****************************************************************************/

#ifndef OMNIFLOP_H
#define OMNIFLOP_H

/* ======================================================================== */
/* Declarations																*/
/* ======================================================================== */

/* ------------------------------------------------------------------------ */
/* Build options															*/
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Headers																	*/
/* ------------------------------------------------------------------------ */

#ifndef DRIVER
/* Header winoioctl.h includes definition of a structure without a name - this prevents warning */
//#pragma warning( push )
//#pragma warning( disable: 4201 )
#include <winioctl.h>
//#pragma warning( pop )
#endif

/* ------------------------------------------------------------------------ */
/* Public globals															*/
/* ------------------------------------------------------------------------ */

/* This header is designed to work with the driver which reports its version */
/* as this. You should check that the returned version number is >= this. */
#define OMNIFLOP_DRIVER_VER		0x0201000BUL

/* These are all the (extended) formats currently supported by OmniDisk. */
/* These supplement the disk MEDIA_TYPEs in ntdddisk.h (0x00 - 0x19) */
/* and the STORAGE_MEDIA_TYPEs in ntddstor.h (0x20-0x5B). */

/* 'Spare' MEDIA_TYPEs for floppy disks run from 0x1A through 0x1F inclusive. */
/* Not enough! But by testing, the MEDIA_TYPE enum is 4 bytes long... */
/* so we'll use a 2-byte prefix '0F10' (OmniFlop) for all our formats. */

/* The physical drive size is ignored (3.5", 5.25") because it's largely irrelevant. */
/* The formats must be distinguishable using the GEOMETRY structures returned, */
/* which include Cylinders/Heads/Sectors/Bytes per sector/Media type - but not */
/* starting sector, head numbering (single-sided formats), or FM/MFM. */
/* Hence we use MEDIA_TYPE to signal parameters not covered by the physical geometry, */
/* such as 1-sided formats (which can still use two heads!). */
/* Note that if you use Unknown for the MEDIA_TYPE then the geometry is set to zero */
/* and nothing can be deduced. */

/* If you use an F5 media type with a 3.5" drive then the drive is displayed as 5.25". */

/* Note that EXTENDED_FORMATs usually have to be enabled to be used, using the IOCTL below. */

#define EXTENDED_FORMATS_MIN (0x0F100000UL)
#define EXTENDED_FORMATS_MAX (0x0F10FFFFUL)

typedef enum _EXTENDED_MEDIA_TYPE {
	/* NOTE: These are ordered numerically to help avoid duplication */

	/* DOS/PC formats come first, in range 0000 - 2FFF */

	/* The following are inherited from the original MEDIA_TYPE: */

	// Unknown:					/* Unknown */
	// F5_1Pt2_512:				/* 5.25" DOS 1.2MB,  512 bytes/sector */
	// F3_1Pt44_512:			/* 3.5" DOS 1.44MB, 512 bytes/sector */
	FX_APPLE_MAC_HD_HFS			= F3_1Pt44_512,		/* Apple Macintosh 1.44MB HD HFS [Jon Ripley] */
	FX_ENS_EPS_ASR_HD			= F3_1Pt44_512,		/* Ensoniq EPS, ASR-10 & compatibles HD-disk [Markus Dimdal] */
	FX_ENS_COMP_1440			= F3_1Pt44_512,		/* Alias */
	FX_ROLAND_S7_HD				= F3_1Pt44_512,		/* Roland S7xx HD-disk [Markus Dimdal, Garth Hjelte] */
	FX_PEAVEY_SP				= F3_1Pt44_512,		/* Peavey SP [Chris Short, Scott Peer, Garth Hjelte] */
	FX_EMU_EOS					= F3_1Pt44_512,		/* EMu EOS [Garth Hjelte] */
	FX_EMU_ESI					= F3_1Pt44_512,		/* EMu ESi [Garth Hjelte] */
	// F3_2Pt88_512:			/* 3.5" DOS 2.88MB, 512 bytes/sector */
	// F3_20Pt8_512:			/* 3.5" DOS 20.8MB, 512 bytes/sector */
	// F3_720_512:				/* 3.5" DOS 720KB,  512 bytes/sector */
	F3_ATRIST_DSDD				= F3_720_512,		/* 3.5" Atari ST DSDD [Jon Ripley] */
	F3_SPEC_CPM					= F3_720_512,		/* 3.5" Sinclair Spectrum +3 CP/M [Andy J Davis, Thomas Heck] */
	F3_AMS_CPM					= F3_720_512,		/* 3.5" Amstrad CP/M [Andy J Davis, Thomas Heck] */
	F3_BBC_MAST_DOS				= F3_720_512,		/* 3.5" Acorn BBC Master 512 DOS [Chris Richardson] */
	F3_ENS_EPS_ASR_DD			= F3_720_512,		/* 3.5" Ensoniq EPS, ASR-10 & compatibles DD-disk [Markus Dimdal] */
	F3_ENS_720					= F3_720_512,		/* Alias */
	F3_ROLAND_S5S7_DD			= F3_720_512,		/* 3.5" Roland S5xx, S7xx DD-disk [Markus Dimdal] */
	F3_KORG_01W					= F3_720_512,		/* 3.5" Korg 01/W [bblueth123_HIRATA] */
	// F5_360_512:				/* 5.25" DOS 360KB,  512 bytes/sector */
	// F5_320_512:				/* 5.25" DOS 320KB,  512 bytes/sector */
	// F5_320_1024:				/* 5.25" DOS 320KB,  1024 bytes/sector */
	// F5_180_512:				/* 5.25" DOS 180KB,  512 bytes/sector */
	// F5_160_512:				/* 5.25" DOS 160KB,  512 bytes/sector */
	F5_AMS_IBM					= F5_160_512,		/* Amstrad IBM Format 5.25" */
	// RemovableMedia:			/* Removable media other than floppy */
	// FixedMedia:				/* Fixed hard disk media */
	// F3_120M_512:				/* 3.5" DOS 120M Floppy */
	// F3_640_512:				/* 3.5" DOS 640KB,  512 bytes/sector */
	// F5_640_512:				/* 5.25" DOS 640KB,  512 bytes/sector */
	// F5_720_512:				/* 5.25" DOS 720KB,  512 bytes/sector */
	F5_ATRIST_DSDD				= F5_720_512,		/* 5.25" Atari ST DSDD [Jon Ripley] */
	F5_SPEC_CPM					= F5_720_512,		/* 5.25" Sinclair Spectrum +3 CP/M [Andy J Davis, Thomas Heck] */
	F5_AMS_CPM					= F5_720_512,		/* 5.25" Amstrad CP/M [Andy J Davis, Thomas Heck] */
	F5_BBC_MAST_DOS				= F5_720_512,		/* 5.25" Acorn BBC Master 512 DOS [Chris Richardson] */
	F5_ENS_EPS_ASR_DD			= F5_720_512,		/* 5.25" Ensoniq EPS, ASR-10 & compatibles DD-disk [Markus Dimdal] */
	F5_ENS_720					= F5_720_512,		/* Alias */
	F5_ROLAND_S5S7_DD			= F5_720_512,		/* 5.25" Roland S5xx, S7xx DD-disk [Markus Dimdal] */
	F5_KORG_01W					= F5_720_512,		/* 5.25" Korg 01/W [bblueth123_HIRATA] */
	// F3_1Pt2_512:				/* 3.5" DOS 1.2Mb,  512 bytes/sector */
	// F3_1Pt23_1024:			/* 3.5" DOS 1.23Mb, 1024 bytes/sector */
	// F5_1Pt23_1024:			/* 5.25" DOS 1.23MB, 1024 bytes/sector */
	// F3_128Mb_512:			/* 3.5" DOS MO 128Mb   512 bytes/sector */
	// F3_230Mb_512:			/* 3.5" DOS MO 230Mb   512 bytes/sector */
	// F8_256_128:				/* 8" DOS 256KB,  128 bytes/sector */

	/* The following are new formats beyond those of the original MEDIA_TYPE: */

	// FX_IBM_DOS160 also known as F5_160_512 where pre-defined */
	/* This media type is returned for drives the standard driver did not implement. Same as F5_160_512. */
	FX_IBM_DOS160				= (EXTENDED_FORMATS_MIN | 0x0160),	/* DOS 160kB */
	FX_AMS_IBM					= FX_IBM_DOS160, /* Amstrad IBM Format */
	// FX_IBM_DOS180	also known as F5_180_512 where pre-defined */
	// This media type is returned for drives the standard driver did not implement. Same as F5_180_512. */
	FX_IBM_DOS180				= (EXTENDED_FORMATS_MIN | 0x0180),	/* DOS 180kB */
	// FX_IBM_DOS320	also known as F5_320_512 where pre-defined */
	/* This media type is returned for drives the standard driver did not implement. Same as F5_320_512. */
	FX_IBM_DOS320				= (EXTENDED_FORMATS_MIN | 0x0320),	/* DOS 320kB */
	// FX_IBM_DOS360	also known as F5_360_512 where pre-defined */
	/* This media type is returned for drives the standard driver did not implement. Same as F5_360_512. */
	FX_IBM_DOS360				= (EXTENDED_FORMATS_MIN | 0x0360),	/* DOS 360kB */
	FX_BBC_MAST_DOS				= FX_IBM_DOS360, /* Acorn BBC Master 512 DOS 360kB [Chris Richardson] */
	FX_IBM_TORCH_GRAD			= FX_IBM_DOS360, /* IBM 360kB Torch Graduate  [Chris Richardson] */
	FX_IBM_DOS640				= (EXTENDED_FORMATS_MIN | 0x0640),	/* DOS 640kB */
	// FX_IBM_DOS720 also known as F5_720_512 and F3_720_512 */
	/* This media type is NEVER RETURNED. It was intended for drives the standard driver did not implement - */
	/* but all drives had it. Same as F5_720_512 or F3_720_512. */
	FX_IBM_DOS720				= (EXTENDED_FORMATS_MIN | 0x0720),	/* DOS 720kB */
	FX_IBM_DOS800				= (EXTENDED_FORMATS_MIN | 0x0800),	/* DOS 800kB */
	FX_SPEC_MGT					= FX_IBM_DOS800, /* Sinclair Spectrum MGT +D/Disciple [Andy J Davis & Thomas Heck] */
	FX_AKAI_MPC_60				= FX_IBM_DOS800, /* AKAI MPC 60 MK II [Dale Henriques] */
	FX_ENS_COMP_DD				= FX_IBM_DOS800, /* Ensoniq EPS, ASR-10 & compatibles DD-disk [Markus Dimdal] */
	FX_ENS_800					= FX_IBM_DOS800, /* Alias */
	FX_ATRISTE_800				= FX_IBM_DOS800, /* Atari STE 800kB [John Davis] */
	FX_EMU_EMAX_DOS				= FX_IBM_DOS800, /* EMu Emax DOS-compatible [Garth Hjelte] */
	// FX_IBM_DOS320_1024 also known as F5_320_1024 where pre-defined */
	/* This media type is returned for drives the standard driver did not implement. Same as F5_320_1024. */
	FX_IBM_DOS320_1024			= (EXTENDED_FORMATS_MIN | 0x1024),	/* DOS 320kB (1024 bytes/sector) */
	// FX_IBM_DOS1M2	also known as F5_1Pt2_512 */
	/* This media type is returned for drives the standard driver did not implement. Same as F5_1Pt2_512. */
	FX_IBM_DOS1200				= (EXTENDED_FORMATS_MIN | 0x1200),	/* DOS 1.2MB */
	FX_IBM_DOS1215				= (EXTENDED_FORMATS_MIN | 0x1215), /* DOS 1.215MB [Vitaliy Vorobyov] */
	FX_IBM_DOS1230				= (EXTENDED_FORMATS_MIN | 0x1230), /* DOS 1.230MB [Vitaliy Vorobyov] */
	FX_IBM_DOS1232				= (EXTENDED_FORMATS_MIN | 0x1232), /* DOS 1.232MB [pstaszkow] */
	FX_IBM_DOS1245				= (EXTENDED_FORMATS_MIN | 0x1245), /* DOS 1.245MB [Vitaliy Vorobyov] */
	// FX_IBM_DOS1440 also known as F3_1Pt44_512 */
	/* This media type is NEVER RETURNED. It was intended for drives the standard driver did not implement - but all */
	/* drives had it. Same as F3_1Pt44_512. */
	FX_IBM_DOS1440				= (EXTENDED_FORMATS_MIN | 0x1440), /* DOS 1.44MB */
	FX_IBM_DOS1458				= (EXTENDED_FORMATS_MIN | 0x1458), /* DOS 1.458MB [Vitaliy Vorobyov] */
	FX_IBM_DOS1476				= (EXTENDED_FORMATS_MIN | 0x1476), /* DOS 1.476MB [Vitaliy Vorobyov] */
	FX_IBM_DOS1494				= (EXTENDED_FORMATS_MIN | 0x1494), /* DOS 1.494MB [Vitaliy Vorobyov] */
	FX_IBM_DOS1722				= (EXTENDED_FORMATS_MIN | 0x1722), /* DOS 1.722MB [Stephane Roth] */
	FX_IBM_DOS1743				= (EXTENDED_FORMATS_MIN | 0x1743), /* DOS 1.743MB [kalman] */

	FX_HP2100					= (EXTENDED_FORMATS_MIN | 0x2100), /* 8" HP-2100 [Dave White] */
	F3_HP2100					= (EXTENDED_FORMATS_MIN | 0x2103), /* 3.5" HP-2100 [Patrice Leonard] */

	// FX_IBM_DOS2880 also known as F3_2Pt88_512 */
	/* This media type is NEVER RETURNED. It was intended for drives the standard driver did not implement - but all */
	/* drives had it. Same as F3_2Pt88_512. */
	FX_IBM_DOS2880				= (EXTENDED_FORMATS_MIN | 0x2880),	/* DOS 2.88MB */

	FX_BMI3030A					= (EXTENDED_FORMATS_MIN | 0x3030), /* BMI3030A [Edward Winterberger] */

	FX_MORI_SEIKI				= (EXTENDED_FORMATS_MIN | 0x4151), /* Mori Seiki DS DD [Thean Low] */

	FX_BBC_SJ_MDFS				= (EXTENDED_FORMATS_MIN | 0x5DF5),	/* Acorn BBC SJ Research MDFS [Mark Ferns] */
	FX_EMU_EMAX					= FX_BBC_SJ_MDFS,	/* EMu Emax [Steve <ste.dal@libero.it>] */

	FX_ALESIS_DATA				= (EXTENDED_FORMATS_MIN | 0xA1ED), /* Alesis Datadisk [Donal Ryan] */

	FX_AKAI_S13K_HD				= (EXTENDED_FORMATS_MIN | 0xA513), /* AKAI S-series HD-disk [Markus Dimdal] */
	FX_AKAI_S1000_HD			= FX_AKAI_S13K_HD, /* Akai S1000 HD [Markus Dimdal] */
	FX_AKAI_S3000_HD			= FX_AKAI_S13K_HD, /* Akai S3000 HD [Markus Dimdal] */
	FX_AKAI_S_HD				= FX_AKAI_S13K_HD, /* Akai S-series HD-disk */
	FX_KORG_T					= FX_AKAI_S13K_HD, /* Korg T-series [Dominic Guss] */

	FX_AMS_SYS_SS				= (EXTENDED_FORMATS_MIN | 0xA551), /* Amstrad AMSDOS System SS [Karl Kopeszki] */
	FX_AMS_SYS_DS				= (EXTENDED_FORMATS_MIN | 0xA552), /* Amstrad AMSDOS System DS [Karl Kopeszki] */
	FX_AMS_SYS_DATA				= (EXTENDED_FORMATS_MIN | 0xA55D), /* Amstrad AMSDOS System/Data DS [Karl Kopeszki] */
	FX_AMS_DATA_SS				= (EXTENDED_FORMATS_MIN | 0xA5D1), /* Amstrad AMSDOS Data SS [Karl Kopeszki] */
	FX_AMS_DATA_DS				= (EXTENDED_FORMATS_MIN | 0xA5D2), /* Amstrad AMSDOS Data DS [Karl Kopeszki] */
	FX_AMS_DATA_SYS				= (EXTENDED_FORMATS_MIN | 0xA5D5), /* Amstrad AMSDOS Data/System DS [Karl Kopeszki] */

	FX_AKAI_S950_HD				= (EXTENDED_FORMATS_MIN | 0xA950), /* Akai S950 HD-disk [unconfirmed] [Markus Dimdal] */

	F3_BBC_ADFS_L				= (EXTENDED_FORMATS_MIN | 0xADF1),	/* 3.5" BBC Acorn ADFS L [Chris Richardson 09/02/05] */
	FX_BBC_ADFS_M				= (EXTENDED_FORMATS_MIN | 0xADF2),	/* BBC Acorn ADFS M [Jonathan G Harston, Chris Richardson] */
	FX_BBC_ADFS_S				= (EXTENDED_FORMATS_MIN | 0xADF3),	/* BBC ADFS S [Jonathan G Harston, Chris Richardson] */
	F5_BBC_ADFS_L				= (EXTENDED_FORMATS_MIN | 0xADF5),	/* 5.25" BBC Acorn ADFS L [Mark Ferns 03/02/05, Jon Ripley, Tim Felgate 28/03/05] */
	FX_BBC_ADFS_DE				= (EXTENDED_FORMATS_MIN | 0xADFD),	/* BBC ADFS D/D+/E/E+ [Jon Ripley, Chris Richardson] */
	FX_QL_QDOS					= FX_BBC_ADFS_DE,	/* Sinclair QL QDOS [Ali Booker] */
	FX_OBERHEIM_DPX				= FX_BBC_ADFS_DE,	/* Oberheim DPX [Garth Hjelte] */
	FX_PROPHET_2002				= FX_BBC_ADFS_DE,	/* Prophet 2002 [Garth Hjelte] */
	FX_BBC_ADFS_F				= (EXTENDED_FORMATS_MIN | 0xADFF),	/* BBC ADFS F/F+ [Jon Ripley, Chris Richardson] */

	FX_ATRISTE_810				= (EXTENDED_FORMATS_MIN | 0xAE10), /* Atari STE 810kB [John Davis] */
	FX_ATRISTE_820				= (EXTENDED_FORMATS_MIN | 0xAE20), /* Atari ST 820kB [David Williams] */
	FX_ATRISTE_738				= (EXTENDED_FORMATS_MIN | 0xAE38), /* Atari STE 738kB [John Davis] */
	FX_ATRIST_SSDD				= (EXTENDED_FORMATS_MIN | 0xAE60), /* Atari ST SSDD [Mark "alfspanners"] */

	FX_BALZER					= (EXTENDED_FORMATS_MIN | 0xBA12), /* Balzer Metal Evaporator [Richard Scott, Dynex Semiconductor] */

	FX_BBC_DFS40				= (EXTENDED_FORMATS_MIN | 0xBBC4), /* Acorn BBC DFS 40-track single-sided [Chris Richardson, Rob Nicholds] */
	FX_BBC_DFS40x2				= (EXTENDED_FORMATS_MIN | 0xBBC5), /* Acorn BBC DFS 40-track double-sided [Chris Richardson] */
	FX_BBC_DFS80				= (EXTENDED_FORMATS_MIN | 0xBBC8), /* Acorn BBC DFS 80-track single-sided [Chris Richardson] */
	FX_BBC_DFS80x2				= (EXTENDED_FORMATS_MIN | 0xBBC9), /* Acorn BBC DFS 80-track double-sided [Chris Richardson] */
	FX_BBC_Z80_CPM				= (EXTENDED_FORMATS_MIN | 0xBBCA), /* Acorn BBC Z80 CP/M [Chris Richardson] 12/02/2005 */
	FX_BBC_DOS_PLUS				= (EXTENDED_FORMATS_MIN | 0xBBCD), /* Master 512 DOS Plus [Chris Richardson] */
	FX_AKAI_S900				= FX_BBC_DOS_PLUS, /* Akai S900 Sampler DD [Markus Dimdal] */
	FX_AKAI_S1000_DD			= FX_BBC_DOS_PLUS, /* Akai S1000 double-density [Markus Dimdal] */
	FX_AKAI_S3000_DD			= FX_BBC_DOS_PLUS, /* Akai S3000 double-density [Markus Dimdal] */
	FX_AKAI_S_DD				= FX_BBC_DOS_PLUS, /* AKAI S-series DD-disk [Markus Dimdal] */
	FX_KORG_DSS1				= FX_BBC_DOS_PLUS, /* Korg DSS-1 [Claude Climer] */

	FX_SPEC_BETA40S				= (EXTENDED_FORMATS_MIN | 0xBE40), /* Sinclair ZX Spectrum BetaDisk 40-track SS [Roberto Jose] */
	FX_SPEC_BETA40D				= (EXTENDED_FORMATS_MIN | 0xBE41), /* Sinclair ZX Spectrum BetaDisk 40-track DS [Walter G Hertlein, Roberto Jose] */
	FX_SPEC_BETA80S				= (EXTENDED_FORMATS_MIN | 0xBE80), /* Sinclair ZX Spectrum BetaDisk 80-track SS [Roberto Jose] */
	FX_SPEC_BETA80D				= (EXTENDED_FORMATS_MIN | 0xBE81), /* Sinclair ZX Spectrum BetaDisk 80-track DS [Roberto Jose] */
	F3_THOMSON_MOTO_DS			= FX_SPEC_BETA80D, /* Thomson MO/TO double-sided 3.5" [Daniel Coulom/Yoann Riou/Jean Rech] */

	FX_RSDOS48					= (EXTENDED_FORMATS_MIN | 0xC0C0), /* Tandy CoCo RSDOS single-sided 48TPI [Darren Atkinson] */
	FX_TANDY_COCO				= FX_RSDOS48,
	FX_RSDOS48x2				= (EXTENDED_FORMATS_MIN | 0xC0C1), /* Tandy CoCo RSDOS double-sided 48TPI [Darren Atkinson] */
	FX_TANDY_COCOx2				= FX_RSDOS48x2,
	FX_RSDOS96					= (EXTENDED_FORMATS_MIN | 0xC0C2), /* Tandy CoCo RSDOS single-sided 96TPI [Darren Atkinson, Benoit Bleau] */
	FX_RSDOS96x2				= (EXTENDED_FORMATS_MIN | 0xC0C3), /* Tandy CoCo RSDOS double-sided 96TPI [Darren Atkinson, Benoit Bleau] */
	FX_RSOS9_40_48				= (EXTENDED_FORMATS_MIN | 0xC0C4), /* RadioShack CoCo NitrOS9 40x48TPI SS [Benoit Bleau] */
	FX_RSOS9_40_48x2			= (EXTENDED_FORMATS_MIN | 0xC0C5), /* RadioShack CoCo NitrOS9 40x48TPI SSx2 [Benoit Bleau] */
	FX_RSOS9_40_96				= (EXTENDED_FORMATS_MIN | 0xC0C6), /* RadioShack CoCo NitrOS9 40x96TPI SS [Benoit Bleau] */
	FX_RSOS9_40_96x2			= (EXTENDED_FORMATS_MIN | 0xC0C7), /* RadioShack CoCo NitrOS9 40x96TPI SSx2 [Benoit Bleau] */
	FX_RSOS9_80					= (EXTENDED_FORMATS_MIN | 0xC0C8), /* RadioShack CoCo NitrOS9 80trk SS [Benoit Bleau] */
	FX_RSOS9_80x2				= (EXTENDED_FORMATS_MIN | 0xC0C9), /* RadioShack CoCo NitrOS9 80trk SSx2 [Benoit Bleau] */
	FX_RSOS9_80DS				= (EXTENDED_FORMATS_MIN | 0xC0CA), /* RadioShack CoCo NitrOS9 80trk DS [Bob Devries] */
	FX_RSOS9_40_48DS			= (EXTENDED_FORMATS_MIN | 0xC0CB), /* RadioShack CoCo NitrOS9 40x48TPI DSDD [Carey] */
	FX_RSOS9_40_96DS			= (EXTENDED_FORMATS_MIN | 0xC0CC), /* RadioShack CoCo NitrOS9 40x96TPI DSDD [Carey] */

	FX_CBM1581					= (EXTENDED_FORMATS_MIN | 0xC158), /* cbm1581 [Wolfgang Moser] */
	FX_LYNXDOS_800				= FX_CBM1581, /* LynxDOS 800kB [Pete Todd] */

	FX_CPM_PDOS					= (EXTENDED_FORMATS_MIN | 0xCB35),	/* 640kB CP/M / PDOS, 80/2/16x256, 1+, 640kB */
	FX_CPM_640					= FX_CPM_PDOS, /* CP/M-80 [Jason Watton (Philips P2000C but used on others)] */
	FX_STRIDE_PDOS				= FX_CPM_PDOS, /* Stride PDOS [Jason Watton] */
	FX_TRDOS					= FX_CPM_PDOS, /* Sinclair ZX Spectrum TR-DOS [Art (apri@tut.by)] */
	FX_ZEISS_M400				= FX_CPM_PDOS, /* ZEISS Spectrophotometer Specord M400 [Milan Kubasek] */
	FX_SHIMA_SEIKI_DSDD			= FX_CPM_PDOS, /* Shima Seiki DS DD [Paulo Gomes, Kathy Newey] */
	FX_ABB_ROBOT				= FX_CPM_PDOS, /* ABB/Asea Robot [Daniel C Hayden] */
	FX_MOOG_TMC_BLOWMOULD		= FX_CPM_PDOS, /* Moog TMC Blowmould control [Richard Coppack] */

	FX_CMDFD1M					= (EXTENDED_FORMATS_MIN | 0xCFD1), /* cmdfd1m [Wolfgang Moser] */
	FX_CMDFD2M					= (EXTENDED_FORMATS_MIN | 0xCFD2), /* cmdfd2m [Wolfgang Moser] */
	FX_CMDFD4M					= (EXTENDED_FORMATS_MIN | 0xCFD4), /* cmdfd4m [Wolfgang Moser] */

	FX_SPEC_OPUSDISC			= (EXTENDED_FORMATS_MIN | 0xD15C), /* Spectrum Opus Discovery [Simon Owen] */

	FX_DEC_RAINBOW				= (EXTENDED_FORMATS_MIN | 0xD5B0), /* DEC Rainbow 100 [Paul Hughes] */

	FX_DYNACORD					= (EXTENDED_FORMATS_MIN | 0xDAC0), /* Dynacord [Garth Hjelte] */

	FX_BBC_DDOS80				= (EXTENDED_FORMATS_MIN | 0xDD58),	/* Acorn BBC DDOS 80-track single-sided [Chris Richardson] */
	FX_BBC_DDOS80x2				= (EXTENDED_FORMATS_MIN | 0xDD59),	/* Acorn BBC DDOS 80-track double-sided [Chris Richardson] */
	FX_WATFORD_DDFS				= FX_BBC_DDOS80x2, /* Watford Electronics DDFS [Herman Klaassen] */

	FX_NEC_DMF					= (EXTENDED_FORMATS_MIN | 0xDFFF), /* NEC PC9801 FC9801 DMF [Christopher J.M. Robertson] */
	FX_NEC_PC9801				= FX_NEC_DMF, /* NEC PC9801 UV DMF HD [Christopher J.M. Robertson] */
	FX_NEC_FC9801				= FX_NEC_DMF, /* NEC FC9801 V DMF HD [Christopher J.M. Robertson] */

	FX_ELG_WP_CPM				= (EXTENDED_FORMATS_MIN | 0xE1E5), /* Electroglas Wafer Probers CP/M [Phil Wiens] */

	FX_ENS_ASR10				= (EXTENDED_FORMATS_MIN | 0xE510), /* Ensoniq EPS, ASR-10 & compatibles HD-disk [Markus Dimdal] */
	FX_ENS_1600					= FX_ENS_ASR10, /* Alias */
	FX_ENS_COMP_HD				= (EXTENDED_FORMATS_MIN | 0xE511), /* Ensoniq [Computer Format] HD [Markus Dimdal] */
	FX_ENS_COMP_1600			= FX_ENS_COMP_HD,
	FX_ENS_TS12					= (EXTENDED_FORMATS_MIN | 0xE512), /* Ensoniq TS12 [Dominic] */
	FX_ENS_MIRAGE				= (EXTENDED_FORMATS_MIN | 0xE514), /* Ensoniq Mirage [Claude Climer] */
	FX_ENS_ASR10_SP				= (EXTENDED_FORMATS_MIN | 0xE515), /* Ensoniq EPS, ASR-10 & compatibles Special HD-disk [Markus Dimdal] */
	FX_ENS_1640					= FX_ENS_ASR10_SP, /* Alias */
	FX_ENS_EPS16				= (EXTENDED_FORMATS_MIN | 0xE516), /* Ensoniq EPS 16+/Classic DD [Matt Savard, Markus Dimdal, H Mandingo] */
	FX_ENS_COMP_800				= FX_ENS_EPS16, /* Alias */
	FX_ENS_ASR10_COMP			= FX_ENS_EPS16, /* Ensoniq ASR-10 [Computer Format] DD [Markus Dimdal] */
	FX_ENS_EPS16_SP				= (EXTENDED_FORMATS_MIN | 0xE517), /* Ensoniq EPS, ASR-10 & compatibles DD-disk [Markus Dimdal] */
	FX_ENS_820					= FX_ENS_EPS16_SP, /* Alias */
	FX_ENS_SQ80					= (EXTENDED_FORMATS_MIN | 0xE580), /* Ensoniq SQ80 [Eric Nevarez] */

	FX_CUSTOM 					= EXTENDED_FORMATS_MAX /* Unknown uniform custom format */
} EXTENDED_MEDIA_TYPE;

/* Codes 0x000 - 0x7FF are reserved for Microsoft; 0x800 - 0xFFF are reserved for customers according to WINIOCTL.h */

/* IOCTLs ================================================================== */

/* IOCTL_OMNIFLOP_GET_DRIVER_VER */
/* ----------------------------- */
/* Returns one DWORD (4 bytes) for the driver version. This should be the same or greater than OMNIFLOP_DRIVER_VER */
/* above to use this header. */

#define IOCTL_OMNIFLOP_GET_DRIVER_VER		CTL_CODE(IOCTL_DISK_BASE, 0xF00, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_GET_FLOPPY_DRIVE_TYPE */
/* ------------------------------------ */
/* Returns one byte for the drive type: */
#define OFL_DRIVE_TYPE_UNKNOWN				0	/* None/Unknown */
#define OFL_DRIVE_TYPE_INVALID				OFL_DRIVE_TYPE_UNKNOWN
#define OFL_DRIVE_TYPE_0360					1	/* 5.25" 360kB 40-track double-sided double-density, no change line */
#define OFL_DRIVE_TYPE_1200					2	/* 5.25" 1.2MB 80-track double-sided high density, or NEC98 5.25" 1.2MB external drive */
#define OFL_DRIVE_TYPE_0720					3	/* 3.5"  720kB 80-track double-sided double-density */
#define OFL_DRIVE_TYPE_1440					4	/* 3.5" 1.44MB 80-track double-sided high density */
#define OFL_DRIVE_TYPE_2880					5	/* 3.5" 2.88MB 80-track double-sided extended density */

#define OFL_NUMBER_OF_DRIVE_TYPES			6

#define IOCTL_OMNIFLOP_GET_FLOPPY_DRIVE_TYPE	CTL_CODE(IOCTL_DISK_BASE, 0xF01, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_ENABLE_EXTENDED_FORMATS */
/* -------------------------------------- */
/* INPUT: String to enable formats, custom to each implementation. YOU WILL BE ADVISED OF THIS. */
/* OUTPUT: None. */
/* Call fails if the string is unrecognised or unlicensed. */
/* WHEN FINISHED, MAKE SURE YOU DISABLE_EXTENDED_FORMATS. */

#define IOCTL_OMNIFLOP_ENABLE_EXTENDED_FORMATS	CTL_CODE(IOCTL_DISK_BASE, 0xF10, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_DISABLE_EXTENDED_FORMATS */
/* --------------------------------------- */
/* Takes nothing, returns nothing. The driver returns to normal Windows operation. */
/* IF YOU DO NOT DO THIS ONCE YOU HAVE FINISHED WITH THE FLOPPY THE SYSTEM MAY BECOME UNSTABLE. */

#define IOCTL_OMNIFLOP_DISABLE_EXTENDED_FORMATS	CTL_CODE(IOCTL_DISK_BASE, 0xF11, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_SELECT_MEDIA_TYPE */
/* -------------------------------- */
/* Takes one MEDIA_TYPE to select (reading type is by IOCTL_DISK_GET_DRIVE_GEOMETRY) */
#define IOCTL_OMNIFLOP_SELECT_MEDIA_TYPE		CTL_CODE(IOCTL_DISK_BASE, 0xF50, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_LOCK_MEDIA_TYPE */
/* ------------------------------ */
/* Takes no parameters - MediaType left unchanged, even over disk change */
#define IOCTL_OMNIFLOP_LOCK_MEDIA_TYPE			CTL_CODE(IOCTL_DISK_BASE, 0xF52, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_UNLOCK_MEDIA_TYPE */
/* -------------------------------- */
/* Takes no parameters - MediaType allowed to change as normal */
#define IOCTL_OMNIFLOP_UNLOCK_MEDIA_TYPE		CTL_CODE(IOCTL_DISK_BASE, 0xF53, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_ENABLE_READ_WRITE */
/* -------------------------------- */
/* Takes and returns one byte of boolean (0 = FALSE) */
/* By default, and without this, the Read/Write is enabled. */
/* However, when trying to format, the disk can be unformatted, which results in */
/* dialogs prompting the user to format the disk... The Open to the drive can't be */
/* circumvented, and although the locks above go far, there are still formats */
/* which the Open tries to read and produces errors depending on how successful it */
/* was... This just 'succeeds' all reads and writes without doing anything, and is */
/* really nothing more than a hack to allow a format to proceed. */
#define IOCTL_OMNIFLOP_ENABLE_READ_WRITE		CTL_CODE(IOCTL_DISK_BASE, 0xF54, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_OMNIFLOP_SET_SRT_HUT */
/* -------------------------- */
/* Takes one unsigned char for SRT/HUT value. Value 0x00 is the maximum, although 0x0F is recommended (more like default), then 1F, 2E etc. */
#define IOCTL_OMNIFLOP_SET_SRT_HUT				CTL_CODE(IOCTL_DISK_BASE, 0xF55, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SLOWEST_SRTHUT		(0x0FU)

/* IOCTL_OMNIFLOP_UNSET_SRT_HUT */
/* ---------------------------- */
/* Removes the constant SRT/HUT setting. Takes no arguments. */
#define IOCTL_OMNIFLOP_UNSET_SRT_HUT			CTL_CODE(IOCTL_DISK_BASE, 0xF56, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* ------------------------------------------------------------------------ */
/* Public classes (C++ only)												*/
/* ------------------------------------------------------------------------ */

#endif /* OMNIFLOP_H */

/* ------------------------------------------------------------------------ */
/* End of source code														*/
/* ------------------------------------------------------------------------ */
