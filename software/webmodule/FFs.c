
/*

// Apple II 16-bit IDE/ATA interface software
// (c) 2001 stephane.guillard@steria.com
// Hardware and software design available at
// http:// s.guillard.free.fr
//
// This software implements :
//
//	o	FAT16 filesystem.
//	o	Low level sector / cluster / dir API (small footprint)
//	o	POSIX-like API (easy porting & writing)

*/

//
//
//
// Conditional compilation directives
//
//
//#define CCD_DEBUG				// Enable verbose behaviour
#undef        CCD_DEBUG                               
//#define CCD_STRICTCHECK			// Enforce full checkings of IDE ide_status etc.
#undef	CCD_ALLOCCHAIN			// Enable smarter, recursive and faster (but bigger and stack hungry) clust_allocchain() instead of clust_extendchain()
#define	CCD_POSIX_API			// Posix-style file API (minimal set)
#define	CCD_POSIX_API_FULL		// Posix-style file API (full set)

//
//
// ****************** I N C L U D E S ******************
//
//

#ifdef CCD_DEBUG
#define DEBUG
#endif

#include "FFs.h"
#include "ide.h"
#include "kernel.h"

#include <string.h>
#include <ctype.h>

// FAT directory entry
typedef struct
{
	char	 Name[11];			//	 8+3 name.
	u8	 Attr;				//	 File attributes - The upper two bits of the attribute u8 are reserved and should always be set to 0 when a file is created and never modified or looked at after that.
	u8	 NTRes;				//	 Reserved for use by Windows NT. Set value to 0 when a file is created and never modify or look at it after that.
	u8	 CrtTimeTenth;		//	 Millisecond stamp at file creation time. This field actually contains a count of tenths of a second. The granularity of the seconds part of CrtTime is 2 seconds so this field is a count of tenths of a second and its valid value range is 0-199 inclusive.
	u16	 CrtTime;			//	 Time file was created.
	u16	 CrtDate;			//	 Date file was created.
	u16	 LstAccDate;		//	 Last access date. Note that there is no last access time, only a date. This is the date of last read or write. In the case of a write, this should be set to the same date as WrtDate.
	u16	 FstClusHI;			//	 High word of this entry's first cluster number (always 0 for a FAT12 or FAT16 volume).
	u16	 WrtTime;			//	 Time of last write. Note that file creation is considered a write.
	u16	 WrtDate;			//	 Date of last write. Note that file creation is considered a write.
	u16	 FstClusLO;			//	 Low word of this entry's first cluster number.
	u32	 FileSize;			//	 32-bit DWORD holding this file's size in u8s.
} dirent;

typedef struct
{
	u16	clust;		// 1st cluster of file (as found in dirent)
	u32 curlba;		// LBA of current sector being read / written to
	u32	pos;		// Current file pointer (u8 offset from start of file)
	u32	size;		// Current file size (u8 count of file)
	u8	inuse;		// In-use flag
	u8	mode;		// O_RDWR, O_RDONLY, O_WRONLY

	u32 dirlba;		// LBA of dir sector
	dirent *dirptr;	// ptr of file in secbuf when dirlba is in secbuf
} file_handle;

//
//
// ****************** C O N S T A N T S ******************
//
//
// General


#define FILE_USED	FALSE
#define FILE_FREE	TRUE

// IDE block size
#define BLOCKSIZE	512

// Max count of file handles
#define MAX_FILES	2

// End of cluster chain
#define CLUSTCHAIN_END		((u16) 0xFFF8)

//
//
// ****************** M A C R O S ******************
//
//

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
//
//
// ****************** G L O B A L S ******************
//
//

// Drive geometry
static u32 hd1_start_lba;		// first lba of partiton
static u8	hd1_geom_secperclust;	// sectors per cluster
static u16 hd1_geom_rd_size;		// root directory size in sectors
static u32 hd1_geom_offfat;		// 1st sector after 1st FAT
static u32	hd1_geom_clustsize;	// u8s per cluster

// Main FS struct properties
static u32
	lba_fat,		// FAT 1st sector
	lba_rd,			// Root directory 1st sector
	lba_data,		// Partition data area 1st sector
	lba_curdir,		// Current directory 1st sector (initialized by dir_examine())
	lba_tmpdir;		// Current sector in current directory (used by dir_exnext())

static dirent *pde_cur, de_cur; // de_cur is a copy of the current dirent. pde_cur points at the original in secbuf

#ifdef CCD_POSIX_API
// Global file handle table
static file_handle __files[MAX_FILES];

#ifdef CCD_POSIX_API
// Default mask
static u8 _umask = ATTR_ARCHIVE;
#endif
#endif

// Sector cache
static u32 lba_incache = -1; // valid LBA starts at 1 so 0 is invalid cache

// 1st free cluster
static u16 clust_firstfree = 1;

//
//
// ****************** E X P O R T E D   P R O T O S ******************
//
//


//
//
// ****************** M I S C   U T I L I T Y   F U N C S ******************
//
//

static u16  lba2clust(u32 lba)	// lba2clust() : find cluster number from LBA sector address
{
	if (lba < lba_data) return 0;	// 0 is error code since clust < 2 is error

	return 2 + (u16) ((lba - lba_data) / hd1_geom_secperclust);
}

static u32  clust2lba(u16 clust)	// clust2lba() : find LBA  address of cluster's 1st sector
{
	if (clust < 2) return 0;	// 0 is error code since LBA < 1 is error

	return lba_data + (u32) (clust - 2) * (u32) hd1_geom_secperclust;
}


//
//
// ****************** LV1 - S E C T O R   I N T E R F A C E   F U N C S ******************
//
//

static u8  secbuf(u16 index)
{
	return sector[index];

}

static u16  peekw(u16 index)
{
	u8 *p;
	u16 t;

	p=sector+index;

	t=p[1];
	t<<=8;
	t|=p[0];

	return t;
}

static u32  peekl(u16 index)
{
	u8 *p;
	u32 t;

	p=sector+index;

	t=p[3];
	t<<=8;
	t|=p[2];
	t<<=8;
	t|=p[1];
	t<<=8;
	t|=p[0];

	return t;
}

static void  secbufw(u16 index,u8 value)
{
	sector[index]=value;
	
}

static void  secbufw16(u16 index,u16 value)
{
	u8 t;

	t=(u8)(value & 255);

	sector[index]=t;

	t=(u8)(value >> 8);

	sector[index+1]=t;

}


// sec_write() : write a sector pointed by lba from secbuf
static void  sec_write(void)
{

#ifdef CCD_DEBUG
	debug("Write sector: %lu\n\r",lba_incache);
#endif

	ide_SectorWrite(lba_incache);
}

// sec_read() : read a sector pointed by lba into secbuf
static void  sec_read(u32 lba)
{
	lba_incache = lba;

#ifdef CCD_DEBUG
	debug("Read sector: %lu\n\r",lba);
#endif

	ide_SectorRead(lba);
}

#ifdef CCD_DEBUG
static void  sec_dump(void)	// Dump sector buffer on screen as HEX
{
	u16 x;

	for(x=0;x<512;x+=16)
	{
		debug("%04x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n\r",x,
					secbuf(x),secbuf(x+1),secbuf(x+2),secbuf(x+3),secbuf(x+4),secbuf(x+5),secbuf(x+6), secbuf(x+7),
					secbuf(x+8),secbuf(x+9),secbuf(x+10),secbuf(x+11),secbuf(x+12),secbuf(x+13),secbuf(x+14), secbuf(x+15));




	}
}

static void  dbg_dumpprm(void)
{
	debug("Start LBA %lu\n\r",hd1_start_lba);
}
#endif

static void  sec_get(u32 *sa)	// Read sector into sector buffer. CHS is recalculated from LBA.
{

	// Simple cache mechanism : don't read an already present sector into buffer
	if (lba_incache == *sa)
	{

		return;
	}

	// 2 - Read sector
	sec_read(*sa);
}

//
//
// ****************** LV2 - H A R D   D I S K   I N T E R F A C E   F U N C S ******************
//
//
bool  hd_qry(void)			// hd_qry() : inquiry disk properties and display them
{
	u8 rc;

#ifdef CCD_DEBUG
	debugs("Query disk info\n\r");
#endif


	rc=ide_Initialise();

	if(rc==TRUE)
	{
		return TRUE;
	}
	else
	{
#ifdef CCD_DEBUG

	debugs("IDE not responding?\n\r");

#endif
		return FALSE;
	}

}

bool  hd_mbr(void)			// hd_mbr() : read disk Master Boot Record, check it and analyze partition entries
{
#ifdef CCD_DEBUG
	u8 pnum;
	u16 p_offset;
#else
#define p_offset	446
#endif

	// Analyze MBR and find 1st partition boot sector (leave CHS of this BPB in IDE parameters for further read)
#ifdef CCD_DEBUG
	debugs("Chk MBR\n\r");
#endif

	sec_read(0);	// Read MBR sector


	// Check signature MBR : offset 1FE=0x55 1FF=AA
	if ((secbuf(0x1FE) != 0x55) || (secbuf(0x1FF) != 0xAA))
	{
#ifdef CCD_DEBUG
		debugs("No\n\r");
#endif
		return FALSE;
	}

#ifdef CCD_DEBUG

	for (pnum = 0 ; pnum < 4 ; ++pnum)	// Scan the 4 partition entries of MBR (starts at secbuf + 446)
	{
		p_offset = 446 + 16 * pnum;	// Offset of partition #pnum struct in MBR

		// Display current part #, and skip if not defined (type == 0)


		debug("Partition #%d ",pnum);

#endif

		if (secbuf(p_offset + 4))	// If partition defined (type != 0)..
		{

#ifdef CCD_DEBUG
			// Display type
			debug("type %d ", secbuf(p_offset + 4));

#endif
			// Extract start LBA
			hd1_start_lba = peekl(p_offset + 8);

#ifdef CCD_DEBUG
			// Display active Y/N
			debug("Active: %s",secbuf(p_offset) == 0x80 ? "Yes " : "No ");

			// Output start  CHS (note that start CHS is the boot sector of the partition)

			debug("LBA %lu\n\r",hd1_start_lba);
#endif
		}

#ifdef CCD_DEBUG
		else
			debugs("None\n\r");

	}

	p_offset = 446;

	// extract start LBA
	hd1_start_lba = peekl(p_offset + 8);

#endif

	return (!(secbuf(446 /*p_offset*/ + 4) != 6 && secbuf(446 /*p_offset*/ + 4) != 4));	// Only return 0 if 1st partition type is FAT16
}

bool  hd_bpb(void)	// Analyze partition Boot Param Block to find out addresses of FAT and rootdir
{
	u8 i;

	sec_read(hd1_start_lba );

	hd1_geom_rd_size = peekw(17) / 16;/* * 32 / BLOCKSIZE */ 		// Get root dir size
	hd1_geom_secperclust = secbuf(13);								// Get sector per cluster count

	// Check if BPB looks OK
	if (
#ifdef CCD_STRICTCHECK
			(secbuf(  3) != 'M')	// standard FAT BPB signature == "MSWIN4.1"
		||	(secbuf(  4) != 'S')
		||	(secbuf(  9) != '.') 
		||
#else
		    (secbuf(510) != 0x55)
		||  (secbuf(511) != 0xaa)
		||	(!(peekw( 14)))	// Reserved MUST be >0
		||	(secbuf( 21) != 0xF8)	// Media MUST be 0xF8
		||
#endif
			(peekw( 11) != BLOCKSIZE)		// MUST have BLOCKSIZE u8 sectors
		)
	{
#ifdef CCD_DEBUG
		debugs("Boot block not found\n\r");
#endif
		return FALSE;
	}

	// Calculate LBA of FAT : reserved + hidden
	// LBA of BPB is calculated from current values of IDE parameters (BPB is the last sector read in secbuf)
	lba_fat = (u32) peekw( 14)
		  + peekl( 28);

	// Remember LBA of 1st sector after 1st FAT
	hd1_geom_offfat = lba_fat + (u32) peekw( 22);

	// Calculate LBA of root dir : FAT + numFAT * fatsize
	// Store root dir into current dir
	lba_curdir = lba_rd = lba_fat + (u32) secbuf(16) * (u32) peekw( 22);

	// Calculate LBA of data area : after root dir
	lba_data = lba_rd + (u32) hd1_geom_rd_size;

	// Calculate cluster size in u8s (to speedup later calculation)
	hd1_geom_clustsize = (u32) BLOCKSIZE * (u32) hd1_geom_secperclust;

#ifdef CCD_DEBUG
	debug("Fat   LBA =%lu\n\r",lba_fat);
	debug("Rd    LBA =%lu\n\r",lba_rd);
	debug("Data  LBA =%lu\n\r",lba_data);
	debug("LFat  LBA =%lu\n\r",hd1_geom_offfat);
	debug("Clustsize =%lu\n\r",hd1_geom_clustsize);
#endif

	// clear out file handles
	for(i=0;i<MAX_FILES;i++)
		__files[i].inuse=0;

	return TRUE;
}

//
//
// ****************** LV3 - C L U S T E R   I N T E R F A C E   F U N C S ******************
//
//
static bool  clust_getfat(u16 clust)	// Read into secbuf the FAT sector containing clust.
								// Return TRUE if Ok, FALSE if error
{
	u32 sa;

	// Find out FAT sector number containing cluster#
	// If we are out of 1st FAT then clust is invalid
	if ((sa = lba_fat + ((u32) (clust >> 8) /* * (u32) 2 / (u32) BLOCKSIZE*/)) // a cluster # is 2 u8s, a sector is BLOCKSIZE u8s
		>= hd1_geom_offfat) return FALSE;

	// Read in FAT sector
	sec_get(&sa);

	return TRUE;
}

static u16  clust_next(u16 clust)	// Find next cluster in chain. Return CLUSTCHAIN_END if no next,
							// and 0 if error
{
	u16 next;

	// Read in FAT sector containing clust
	if (!(clust_getfat(clust))) return 0;

	// Find next cluster entry from FAT
	if ((next = peekw( (clust % (BLOCKSIZE / 2)) * 2)) >= CLUSTCHAIN_END) next = CLUSTCHAIN_END;	// Above, CLUSTCHAIN_END (End Of cluster Chain)

	return next;
}

static void  clust_freechain(u16 clust)
{
	u16 next, offset;

#ifdef CCD_DEBUG
	debug("clust_freechain %u",clust);
#endif

	while (clust)
	{
		// Read in FAT sector containing clust
		if (!clust_getfat(clust))
		{
			clust = 0;
			continue;
		}

		// Adjust 1st free cluster if necessary
		if (clust_firstfree > clust) clust_firstfree = clust;

		// Calculate offset of clust in FAT
		offset = (clust % (BLOCKSIZE / 2)) * 2;

		// Find next cluster entry from FAT
		next = peekw( offset);

		// Free clust
#ifdef CCD_DEBUG
		debug("clust_freechain():zeroing offset %u",offset);
#endif
		secbufw16(offset,0);

		// Write FAT back
		sec_write();

		// Process next in chain
		if (clust < CLUSTCHAIN_END)
			clust = next;
		else
			clust = 0;
	}
}

static u16  clust_findfree(u16 from)
{
	u16 next = from - 1;

	do
	{
		// Read in FAT sector containing clust
		if (!(clust_getfat(++next)))
			 next = 0;

	} while(next && (peekw( (next % (BLOCKSIZE / 2)) * 2)));

	// If cached lba of 1st fat sectors with free clusters is uninitialised then it is our first search
	if (clust_firstfree==1)
		clust_firstfree = next;
	else
	{
		// If we are further in FAT then remember it
		if ((next) && (clust_firstfree < next)) clust_firstfree = next;
	}

	return next;
}

#ifdef CCD_ALLOCCHAIN
#define clust_allocchain(a) __clust_allocchain((a), clust_firstfree)
static u16  __clust_allocchain(u16 count, from)
{
	u16 clust, next;

	// If no more cluster to allocate then return EOC, which will successfully end
	// recursive allocation (remember clusters 0 and EOC are not true clusters
	// so we can use these numbers to break recursion with 0 = fail, and EOC = success)
	if (!count) return CLUSTCHAIN_END;

	// Find a free cluster
	// If we did not find a free cluster then return 0 which will break
	// recursive chain with failure
	if (!(clust = clust_findfree(from))) return 0;

	// Find next free cluster (and go to end of allocation chain).
	// Here, clust_allocchain may be 0 (allocation failed)
	// or any valid free cluster or EOC.
	if (next = __clust_allocchain(count - 1, clust))
	{
		// Remember __clust_allocchain has a side effect on secbuf thus re read FAT sector
		clust_getfat(clust);

		// Link clust to next
		secbufw16( (clust % (BLOCKSIZE / 2)) * 2), next);

		// Write back FAT sector
		sec_write();

		return clust;
	}
	else
		return 0;		// Back-propagate alloc error
}
#endif

static u32  clust_nextlba(u32 lba)	// Find next sector in same cluster chain
{
	u32 next = lba + 1;
	u16 c1, c2;

	// Calculate lba and next clusters
	c1 = lba2clust(lba);
	c2 = lba2clust(next);

	// If sectors are in the same cluster, simply return next
	if (c1 == c2) return next;

	// If not, fetch next cluster in cluster chain and return
	// 1st sector of this new cluster
	if ((c2 = clust_next(c1)) != CLUSTCHAIN_END) return clust2lba(c2);

	// Did not find next cluster
	return 0;
}

static u16  clust_extendchain(u16 clust)	// Add 1 cluster to chain. If clust == 0 then allocate 1 cluster and point it to EOC
{
	u16 next, last;

	// If invalid clust then do nothing
	if ((clust == 1) || (clust >= CLUSTCHAIN_END)) return 0;

	// Try to allocate a new cluster
	if (!(next = clust_findfree(clust_firstfree))) return 0;

	if (clust)
	{
		// Find last cluster of chain
		while ((last = clust_next(clust)) != CLUSTCHAIN_END) clust = last;

		// Link clust -> next
		clust_getfat(clust);
		secbufw16(  (clust % (BLOCKSIZE / 2)) * 2, next);
		sec_write();
	}

	// Link next to CLUSTCHAIN_END
	clust_getfat(next);
	secbufw16((next % (BLOCKSIZE / 2)) * 2, CLUSTCHAIN_END);
	sec_write();

	return next;
}

//
//
// ****************** LV4 - F I L E   T R E E   I N T E R F A C E   F U N C S ******************
//
//
//	From Microsoft Extensible Firmware Initiative FAT32 File System Specification :
//	Special notes about the first u8 (Name[0]) of a FAT directory entry :
//		If Name[0] == 0xE5, then the directory entry is free (there is no file or directory name in this entry).
//		If Name[0] == 0x00, then the directory entry is free (same as for 0xE5), and there are no allocated directory entries after this one (all of the Name[0] u8s in all of the entries after this one are also set to 0).

#define DE_FREE			((u8) 0xE5)
#define DE_FREE_LAST	((u8) 0x00)
#define DE_NONE			((u8) 0xFF)

bool  dir_next(bool bfree)	// dir_next() : return next dirent
							// If bfree is FALSE then we look for an unused dirent.
							// If bfree is TRUE then we look for an used dirent.
{
	u8 c;

	// Read current directory sector into buffer
	sec_get(&lba_tmpdir);

	do
	{
		// Go to next dirent
		++pde_cur;

		// Put 1st char of current dirent filename into c;
		c = pde_cur -> Name[0];

		// If we go outside secbuf, go to next sector of directory (if any)
		if ((char*) pde_cur - (char*) sector >= BLOCKSIZE)
		{
			c = (bfree ? 1 : DE_FREE); // Prepare to loop in there

			if (lba_curdir == lba_rd)	// We're in root dir so we merely go to next sector checking we're below hd1_geom_rd_size
			{
				if (lba_tmpdir - lba_curdir < hd1_geom_rd_size)
					lba_tmpdir++;
				else
					c = (bfree ? DE_NONE : DE_FREE_LAST);
			}
			else	// We're not in root dir so find next sector of current dir in its cluster chain
			{
				if (!(lba_tmpdir = clust_nextlba(lba_tmpdir)))
					c = (bfree ? DE_NONE : DE_FREE_LAST);
			}

			if (c != (bfree ? DE_NONE : 0))	// We are on a new dir sector, read it in and prepare next scan
			{
				sec_get(&lba_tmpdir);

				pde_cur = (dirent *) sector;
				--pde_cur;
			}
		}
	}
	while ((bfree ?		// skip (un)used entries
			/* unused */	((c != DE_FREE_LAST) && (c != DE_FREE) && (c != DE_NONE))
			/* used */ :	(c == DE_FREE)
		   ));

	// If we are on a (un)used entry then return OK
	if ((bfree ? (c != DE_NONE) : (c != DE_FREE_LAST)))
	{
		de_cur = *pde_cur;
		return TRUE;
	}

	return FALSE;
}

bool  dir_examine(bool bfree)	// dir_examine() : start scan of current dir and return first (un)used dirent
{
	// Set current directory sector to 1st sector of current dir
	lba_tmpdir = lba_curdir;

	// "Rewind back" so that dir_next starts scan at 1st dirent
	pde_cur = (dirent *) sector;
	--pde_cur;

	// Find next dirent
	return dir_next(bfree);
}

//
// Convert filename to padded 8.3 format
//
static char *  __create_83_name(char *filename)
{
	static char name83[12];
	int cnt;
	char *p;
	// copy name portion of filename, converting case
	cnt=0;
	p=filename;
	while(cnt<8)
	{
		if(*p=='.' || *p==0)
			break;
		name83[cnt++]=toupper(*p);
		p++;
	}

	// pad name out with spaces
	while(cnt<8)
		name83[cnt++]=' ';

	// copy extension portion of filename, converting case
	if(*p++=='.')
	{
		while(cnt<11)
		{
			if(*p==0)
				break;
			name83[cnt++]=toupper(*p);
			p++;
		}
	}

	// pad extension out with spaces
	while(cnt<11)
		name83[cnt++]=' ';

	name83[11]=0;

	return name83;
}

bool  dir_findbyname(char *filename)
{
	char *name = __create_83_name(filename);

	if (dir_examine(FILE_USED))
		do
		{

			if (de_cur.Attr != ATTR_LONG_NAME)
			{
			
				// We stop at first match, only comparing at strlen(filename) maxed at 11
				if (!(strncmp(name,	de_cur.Name,	11	)))
					return TRUE;
			}
		}
		while (dir_next(FILE_USED));

	return FALSE;
}

//
//
// ****************** LV6 - P O S I X   L I B   P R O T O S ******************
//
//
#ifdef CCD_POSIX_API

static bool  __h_in_use(u8 handle)
{
	// Limit collateral damages in case of wrong handle
	if (handle >= MAX_FILES) return FALSE;

	return __files[handle].inuse;
}

static u8  __h_findfree(void)
{
	u8 hnum;

	for (hnum = 0 ; (hnum < MAX_FILES) && (__h_in_use(hnum)) ; ++hnum);

	return hnum;
}

static void  __h_refresh_dir(u8 handle)
{
	file_handle *fd = &(__files[handle]);

	if (!(__h_in_use(handle))) return;

    // don't update files open for reading
	if(fd -> mode & O_RDONLY)
			return;

	sec_get(&(fd -> dirlba));

	fd -> dirptr -> FstClusLO = swap16(fd -> clust);
	fd -> dirptr -> FileSize = swap32(fd -> size);

	sec_write();
}

s8  close(u8 handle)
									/* Close file handle
										Parameter
											handle	Handle referring to open file

										Returns
											0 if the file was successfully closed.
											A return value of -1 indicates an error,
											in which case errno is set to EBADF,
											indicating an invalid file-handle parameter.
									*/
{
#ifdef CCD_DEBUG
	debug("close %d",handle);
#endif

	if (!(__h_in_use(handle)))	return -1;

#ifdef CCD_STRICTCHECK
	__h_refresh_dir(handle);
#endif

	__files[handle].inuse = FALSE;

#ifdef CCD_DEBUG
	debug("close() OK");
#endif

	return 0;
}

static char filename[64];

u8  creat(char *filename, u8 pmode)
									/* Create new file and return handle

										Parameters
											filename	Name of new file
											pmode	Permission setting
											S_IREAD				Reading permitted
											S_IREAD | S_IWRITE	Reading and writing permitted

										Returns
											if successful, returns a handle to the created
											file. Otherwise the function returns -1 and sets
											errno as follows :
											EACCES	Filename specifies an existing read-only
													file or specifies a directory instead of
													a file
											EMFILE	No more file handles are available
											ENOENT	The specified file could not be found
									*/
{

	// Find a free handle
	u8 handle = __h_findfree();
	file_handle *fd = &(__files[handle]);

	// Check if file already exists
	bool bexist;

	bexist = dir_findbyname(filename);

#ifdef CCD_DEBUG
	debug("creat(%s mode %d)",filename,pmode);
#endif

	// If no more handle available then error EMFILE
	if (handle >= MAX_FILES) return -1;

	// If file already exists
	if (bexist)
	{
#ifdef CCD_DEBUG
	debug("creat() : file exists");
#endif
		// If file is a dir or read only then error EACCESS
		if (de_cur.Attr & (ATTR_READ_ONLY | ATTR_DIRECTORY))
			return -1;

		// Free cluster chain
		clust_freechain(swap16(de_cur.FstClusLO));

		// Recall file dir into secbuf
		sec_get(&lba_tmpdir);
	}
	else
	{
#ifdef CCD_DEBUG
	debug("creat() : file does not exist");
#endif

	  // Find a free dirent in current dir
		if (!(dir_examine(FILE_FREE))) return -1;

		// Store filename in dirent and pad with spaces
		strncpy(pde_cur -> Name, __create_83_name(filename), 11);

	}

    // Set file size to 0 and empty its cluster chain
	fd -> pos = fd -> size = pde_cur -> FileSize = 0;
	fd -> clust = pde_cur -> FstClusLO = 0;
	// Zero high order of cluster
    pde_cur -> FstClusHI = 0;

	// Adjust file mode and other fd properties
#ifdef CCD_POSIX_API_FULL
	pde_cur -> Attr = _umask;
#else
	pde_cur -> Attr = ATTR_ARCHIVE;
#endif
	if (!(pmode & S_IWRITE)) pde_cur -> Attr |= ATTR_READ_ONLY;

	fd -> inuse = TRUE;
	fd -> mode = (pmode & S_IWRITE ? O_RDWR : O_RDONLY);
	fd -> dirlba = lba_tmpdir;
	fd -> dirptr = pde_cur;
	fd -> curlba = 0;

	// Write back dir block (which is current in buffer)
	sec_write();

	return handle;
}

s8  eof(u8 handle)
									/* Tests for end-of-file.
										Parameter
											handle	Handle referring to open file

										Returns
											1 if the current position is end of file,
											or 0 if it is not. A return value of -1
											indicates an error; in this case, errno
											is set to EBADF, which indicates an invalid
											file handle.
									*/
{
	if (!(__h_in_use(handle)))	return -1;

	if (__files[handle].pos + 1 > __files[handle].size) return 1;

	return 0;
}

s8  open(char *filename, u8 oflag, u8 pmode)
									/* Open a file.
										Parameters
											filename	Filename
											oflag		Type of operations allowed
											pmode		Permission mode

										Returns
											Returns a file handle for the opened file.
											A return value of -1 indicates an error,
											in which case errno is set to one of the following
											values:
												EACCES	Tried to open read-only file for
														writing, or file’s sharing mode does
														not allow specified operations, or
														given path is directory
												EEXIST	O_CREAT and O_EXCL flags specified,
														but filename already exists
												EINVAL	Invalid oflag or pmode argument
												EMFILE	No more file handles available
														(too many open files)
												ENOENT	File or path not found

											oflag is an integer expression formed from one
											or more of the following manifest constants
											or constant combinations :
												O_APPEND	Moves file pointer to end of
														file before every write operation.
												O_CREAT		Creates and opens new file for
														writing. Has no effect if file
														specified by filename exists. pmode
														argument is required when O_CREAT is
														specified.
												O_CREAT | O_EXCL	Returns error value
														if file specified by filename
														exists. Applies only when used
														with O_CREAT.
												O_RDONLY	Opens file for reading only;
														cannot be specified with O_RDWR
														or O_WRONLY.
												O_RDWR	Opens file for both reading and
														writing; you cannot specify this
														flag with O_RDONLY or O_WRONLY.
												O_TRUNC	Opens file and truncates it to zero
														length; file must have write
														permission. You cannot specify this
														flag with O_RDONLY. O_TRUNC used
														with O_CREAT opens an existing file
														or creates a new file.
														Warning   The O_TRUNC flag destroys
														the contents of the specified file.
												O_WRONLY	Opens file for writing only;
														cannot be specified with O_RDONLY
														or O_RDWR.

											To specify the file access mode, you must specify
											either O_RDONLY, O_RDWR, or O_WRONLY. There is no
											default value for the access mode.

											The pmode argument is required only when O_CREAT
											is specified. If the file already exists, pmode
											is ignored. Otherwise, pmode specifies the file
											permission settings, which are set when the new
											file is closed the first time. open applies the
											current file-permission mask to pmode before
											setting the permissions (for more information,
											see umask). pmode is an integer expression
											containing one or both of the following manifest
											constants :
												S_IREAD		Reading only permitted
												S_IWRITE	Writing permitted (effectively
													permits reading and writing)
												S_IREAD | S_IWRITE	Reading and writing
													permitted
									*/
{
	// Find a free handle
	u8 handle = __h_findfree();
	file_handle *fd = &(__files[handle]);

	// Check if file already exists
	bool bexist;

#ifdef CCD_DEBUG
	debug("open(%s mode %d)\n\r",filename,oflag);
#endif

	bexist = dir_findbyname(filename);

	// If no more handle available then error EMFILE
	if (handle >= MAX_FILES) return -1;

	// If asked to create a new file which already exists, error
	if (bexist && (oflag & O_CREAT) && (oflag & O_EXCL)) return -1;

	// If asked to truncate a file without write permission, error
	if (bexist && (oflag & O_TRUNC) && (pde_cur -> Attr & ATTR_READ_ONLY)) return -1;

	// If asked to create or truncate, revert to creat()
	if ((oflag & O_CREAT) || (oflag & O_TRUNC)) return creat(filename, pmode);

	// Now we are left with O_APPEND, O_RDONLY, O_RDWR. File must exist.
	if (!bexist) return -1;

	// If asked for meaningless combinations of RDWR, WRONLY and RDONLY then error
	if ((oflag & O_RDWR)	&& (oflag & O_RDONLY)) return -1;
	if ((oflag & O_RDWR)	&& (oflag & O_WRONLY)) return -1;
	if ((oflag & O_RDONLY)	&& (oflag & O_WRONLY)) return -1;

	// Everything is OK, open file
	fd -> size = peekl((u16)((char *)&pde_cur->FileSize - (char*)sector));
	fd -> clust = peekw((u16)((char *)&pde_cur->FstClusLO - (char*)sector));
	fd -> pos = (oflag & O_APPEND ? pde_cur -> FileSize : 0);
	fd -> mode = oflag & (O_RDWR | O_WRONLY | O_RDONLY);
	fd -> inuse = TRUE;
	fd -> dirlba = lba_tmpdir;
	fd -> dirptr = pde_cur;
	fd -> curlba = clust2lba(fd -> clust);

	return handle;
}

s16  read(u8 handle, u8 *buffer, u16 count)
									/* Reads data from a file.
											Parameters
												handle	Handle referring to open file
												buffer	Storage location for data
												count	Maximum number of u8s

											Returns
												the number of u8s read, which may be less
												than count if there are fewer than count u8s
												left in the file.
												If the function tries to read at end of file,
												it returns 0. If the handle is invalid, or the
												file is not open for reading, the function
												returns -1 and sets errno to EBADF.
									*/
{
	file_handle *fd = &(__files[handle]);

	u16
		offset_start = (u16) (fd -> pos & (BLOCKSIZE-1)),
		u8s_read = 0,
		u8s_toread,
		u8s_leftinsec, i;

	if (!(__h_in_use(handle)))	return -1;

	// If asked to read from a non read file then error
	if (!((fd -> mode & O_RDWR) || (fd -> mode & O_RDONLY)))	return -1;

	while ((u8s_read < count) && !(eof(handle)))
	{
		// Read in the file current sector
		sec_get(&(fd -> curlba));

		// 1 - calculate how many u8s are left in the current sector
		u8s_leftinsec = BLOCKSIZE - offset_start;

		// 2 - calculate how many u8s we can thus read in the current sector
		u8s_toread = min(count - u8s_read, u8s_leftinsec);


		// 3 - read these u8s in the user buffer
		 for(i=0;i<u8s_toread;i++)
			buffer[u8s_read+i] = secbuf(offset_start+i);

		// 4 - increment the read u8s counter
		u8s_read += u8s_toread;

		// 5 - if necessary go to next sector
		if (u8s_toread == u8s_leftinsec)
			fd -> curlba = clust_nextlba(fd -> curlba);

		// 6 - zero offset_start, which is only useful in 1st pass
		offset_start = 0;
	}

	// Adjust file pos
	fd -> pos += u8s_read;

	return u8s_read;
}

s16  write(u8 handle, u8 *buffer, s16 count)
									/* Writes data to a file.
										Parameters
											handle	Handle of file into which data is written
											buffer	Data to be written
											count	Number of u8s

										Returns
											If successful, write returns the number of u8s
											actually written. If the actual space remaining
											on the disk is less than the size of the buffer
											the function is trying to write to the disk,
											write fails and does not flush any of the buffer’s
											contents to the disk. A return value of -1
											indicates an error. In this case, errno is set to
											one of two values: EBADF, which means the file
											handle is invalid or the file is not opened for
											writing, or ENOSPC, which means there is not
											enough space left on the device for the operation.
									*/
{
	file_handle *fd = &(__files[handle]);
	u16 offset, size, next,i;
	u8 *start;

#ifdef CCD_DEBUG
	debug("write %d %d",handle,count);
#endif

	if (!(__h_in_use(handle)))	return -1;

	// If asked to write to a non write file then error
	if (!(fd -> mode & (O_RDWR | O_WRONLY)))	return -1;

	// If asked to do nothing then do it
	if (!count) return 0;

	for (start = buffer; count ; count -= size)
	{
		// If current sector is invalid... (either file empty or past end of cluster chain)
		if (!(fd -> curlba))
		{
			// Extend file cluster chain
			// If we could not find a cluster then abort
			if (!(next = clust_extendchain(fd -> clust))) goto __write_abort;

			// If cluster chain was previously empty then link it
			if (!(fd -> clust))
			{
				 fd -> clust = next;

				// Update dirent of file to store clust
				if (next) __h_refresh_dir(handle);
			}

			// Go to 1st sector of the new cluster
			fd -> curlba = clust2lba(next);
		}

		// Calculate base offset and count of u8s to write into secbuf
		offset = fd -> pos & (BLOCKSIZE - 1);
		size = min(BLOCKSIZE - offset, count);

		// Get the sector into secbuf
		sec_get(&(fd -> curlba));

		// Put data into secbuf and write back to HDD
		for(i=0;i<size;i++)
			secbufw(i+(fd -> pos & (BLOCKSIZE-1)),buffer[i]);

		sec_write();


		buffer += size;
		fd -> pos += size;

		// If block full then proceed to next lba in file cluster chain
		if (offset + size == BLOCKSIZE) fd -> curlba = clust_nextlba(fd -> curlba);
	}

__write_abort:

	// If necessary, update file size in dirent
	if (fd -> size < fd -> pos)
	{
		fd -> size = fd -> pos;
		__h_refresh_dir(handle);
	}

	// If we did not write anything, return error
	if (start == buffer) return -1;

	// Else return number of u8s written into file
	return buffer - start;
}
#endif // CCD_POSIX_API

#ifdef CCD_POSIX_API_FULL
s8  access(char *filename, u8 mode)
									/* Determine file-access permission
										Parameters
											path	File or directory path
											mode	Permission setting

											mode Value	Checks File For
											00			Existence only
											02			Write permission
											04			Read permission
											06			Read and write permission

										Returns
											0 if the file has the given mode.
											The function returns -1 if the named file does not
											exist or is not accessible in the given mode;
											in this case, errno is set as follows:
											EACCES : Access denied: file’s permission setting
													 does not allow specified access.
											ENOENT : Filename or path not found.
									*/
{
	// Find file
	bool bExist;

	bExist = dir_findbyname(filename);

	if (!bExist) return -1;

	switch(mode)
	{
		case 0 :
		case 4 :
			break;

		case 2 :
		case 6 :
			bExist = !(de_cur.Attr & ATTR_READ_ONLY);
			break;

		default :
			bExist = FALSE;
			break;
	}

	return bExist ? 0 : -1;
}

s8  chmod(char *filename, u8 pmode)
									/* Change the file-permission settings
										Parameters
											filename	Name of existing file
											pmode		Permission setting for file
											S_IREAD				Reading permitted
											S_IREAD | S_IWRITE	Reading and writing permitted

										Returns
											0 if the permission setting is successfully
											changed. A return value of -1 indicates that
											the specified file could not be found,
											in which case errno is set to ENOENT.
									*/
{


	// Find file
	if (!(dir_findbyname(filename))) return -1;

	// Reset read only bit
	pde_cur -> Attr &= ~ATTR_READ_ONLY;

	// Set read only if asked
	if (!(pmode & S_IWRITE)) pde_cur -> Attr |= ATTR_READ_ONLY;

	// Write back dir sector
	sec_write();

	return 0;
}

u8  dup(u8 handle)
{
	u8 handle2;

	// If handle not used then error
	if (!(__h_in_use(handle))) return -1;

	// If no more handle available then error EMFILE
	if ((handle2 = __h_findfree()) >= MAX_FILES) return -1;

	__files[handle2] = __files[handle];

	return handle2;
}

long  filelength(u8 handle)
									/* Get the length of a file.
										Parameter
											handle	Target file handle

										Returns
											the file length, in u8s, of the target
											file associated with handle. Returns a value
											of -1L to indicate an error, and an invalid
											handle sets errno to EBADF.
									*/
{
	if (!(__h_in_use(handle)))	return -1;

	return __files[handle].size;
}

long  lseek(u8 handle, s32 offset, u8 origin)
									/* Move a file pointer to the specified location.
										Parameters
											handle	Handle referring to open file
											offset	Number of u8s from origin
											origin	Initial position
											origin argument must be one of the following
											constants :
												SEEK_SET	Beginning of file
												SEEK_CUR	Current position of file pointer
												SEEK_END	End of file

										Returns
											the offset, in u8s, of the new position from
											the beginning of the file. Returns -1L to
											indicate an error and sets errno either to EBADF,
											meaning the file handle is invalid, or to EINVAL,
											meaning the value for origin is invalid or the
											position specified by offset is before the
											beginning of the file.
									*/
{
	file_handle *fd = &(__files[handle]);
	u16 clust, clustcnt;
	u8 secoff;

	if (!(__h_in_use(handle)))	return -1;

	// Recalculate the offset from the start of the file
	switch(origin)
	{
		default:
			return -1;

		case SEEK_SET:
			break;

		case SEEK_CUR :	//	Current position of file pointer
			offset += fd -> pos;
			break;

		case SEEK_END :	//	End of file
			offset = fd -> size - 1 - offset;
			break;
	}

	// Process special offsets
	if (offset > fd -> size ) return -1;

	if (offset == fd -> pos)
		return offset;


	// Update current pos
	fd -> pos = offset;

	// Calculate cluster / sector number of new pos
	clustcnt =  offset / hd1_geom_clustsize;
	secoff = (u8) ((offset % hd1_geom_clustsize) / (u32) BLOCKSIZE);

	// Update current LBA
	clust = fd -> clust;
	while ((clustcnt--) && (clust != CLUSTCHAIN_END)) clust = clust_next(clust);

	if (clust != CLUSTCHAIN_END)
	{
		fd -> clust = clust;
		fd -> curlba = clust2lba(clust) + secoff;
	}
	else
		fd->curlba = 0; // flag for extending chain

	return offset;
}

long  tell(u8 handle)
									/* Get the position of the file pointer.
											Parameter
												handle	Handle referring to open file

											Returns
												A return value of -1L indicates an error,
												and errno is set to EBADF to indicate an
												invalid file-handle argument.
									*/
{
	if (!(__h_in_use(handle)))	return -1;

	return __files[handle].pos;
}

#endif // CCD_POSIX_API_FULL

s8  unlink(char *filename)
									/* Delete a file.
										Parameter
											filename	Name of file to remove

										Returns
											0 if successful. Otherwise, the function returns
											-1 and sets errno to EACCES, which means the path
											specifies a read-only file, or to ENOENT, which
											means the file or path is not found or the path
											specified a directory.
									*/
{

	// Find file
	if (!(dir_findbyname(filename))) return -1;

	// Mark entry as free
	pde_cur -> Name[0] = 0xE5;

	// Write back dir block
	sec_write();

	// Free cluster chain
	clust_freechain(swap16(de_cur.FstClusLO));

	return 0;
}
