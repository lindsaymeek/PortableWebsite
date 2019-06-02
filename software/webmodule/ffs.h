
#ifndef _FFS_H
#define _FFS_H

#include "kernel.h"

/*

Description:	FAT16 file system POSIX interface

*/

// FAT stuff
#define ATTR_READ_ONLY  ((u8) 0x01)
#define ATTR_HIDDEN 	((u8) 0x02)
#define ATTR_SYSTEM 	((u8) 0x04)
#define ATTR_VOLUME_ID 	((u8) 0x08)
#define ATTR_DIRECTORY	((u8) 0x10)
#define ATTR_ARCHIVE  	((u8) 0x20)
#define ATTR_LONG_NAME 	(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_ALL 		~ATTR_VOLUME_ID

// fstat stuff
#define S_IREAD        0x01         /* read permission, owner */
#define S_IWRITE       0x02         /* write permission, owner */

// open stuff
#define O_RDONLY       0x01  /* open for reading only */
#define O_WRONLY       0x02  /* open for writing only */
#define O_RDWR         0x04  /* open for reading and writing */
#define O_APPEND       0x08  /* writes done at eof */

#define O_CREAT        0x10  /* create and open file */
#define O_TRUNC        0x20  /* open and truncate */
#define O_EXCL         0x40  /* open only if file doesn't already exist */

// lseek stuff (attention, various values found on various systems)
#undef SEEK_CUR
#undef SEEK_END
#undef SEEK_SET
#define SEEK_CUR	0
#define SEEK_END	1
#define SEEK_SET	2

bool hd_qry(void);			// hd_qry() : inquiry disk properties and display them

bool hd_mbr(void);			// hd_mbr() : read disk Master Boot Record, check it and analyze partition entries

bool hd_bpb(void);	// Analyze partition Boot Param Block to find out addresses of FAT and rootdir

s8 access(char *path, u8 mode);
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

s8  chmod(char *filename, u8 pmode);
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

s8  close(u8 handle);
									/* Close file handle
										Parameter
											handle	Handle referring to open file

										Returns
											0 if the file was successfully closed.
											A return value of -1 indicates an error,
											in which case errno is set to EBADF,
											indicating an invalid file-handle parameter.
									*/

u8  creat(char *filename, u8 pmode);
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

u8  dup(u8 handle);

s8  eof(u8 handle);
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

long  filelength(u8 handle);
									/* Get the length of a file.
										Parameter
											handle	Target file handle

										Returns
											the file length, in u8s, of the target
											file associated with handle. Returns a value
											of -1L to indicate an error, and an invalid
											handle sets errno to EBADF.
									*/

long  lseek(u8 handle, s32 offset, u8 origin);
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

s8  open(char *filename, u8 oflag, u8 pmode);
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

s16  read(u8 handle, u8 *buffer, u16 count);
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

long  tell(u8 handle);
									/* Get the position of the file pointer.
											Parameter
												handle	Handle referring to open file

											Returns
												A return value of -1L indicates an error,
												and errno is set to EBADF to indicate an
												invalid file-handle argument.
									*/



s8  unlink(char *filename);

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

s16  write(u8 handle, u8 *buffer, s16 count);
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
#endif
