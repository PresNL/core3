#include "platforms/file.h"
#include "platforms/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

#include <stdio.h>
#include <sys/stat.h>

//This is fine in file.c instead of wfile.c because unix + windows systems all share very similar ideas about filesystems
//But windows is a bit stricter in some parts (like the characters you can include) and has some weird quirks
//Still, it's common enough here to not require separation

#ifndef _WIN32
	#define _ftelli64 ftell
#else

	#include <direct.h>

	#define stat _stat64
	#define S_ISREG(x) (x & _S_IFREG)
	#define S_ISDIR(x) (x & _S_IFDIR)

	#define mkdir _mkdir

#endif

Error File_resolve(String loc, Bool *isVirtual, String *result) {

	if(result && result->ptr)
		return Error_invalidOperation(0);

	StringList res = (StringList) { 0 };
	Error err = Error_none();

	if(!isVirtual || !result)
		_gotoIfError(clean, Error_nullPointer(!isVirtual ? 1 : 2, 0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	//Copy string so we can modifiy it

	_gotoIfError(clean, String_createCopyx(loc, result));
	*isVirtual = File_isVirtual(loc);

	//Virtual files

	if (*isVirtual)
		_gotoIfError(clean, String_popFrontCount(result, 2));

	//Network drives are a thing on windows and allow starting a path with \\
	//We shouldn't be supporting this.
	//The reason; you can access servers from an app with a file read instead of a https read!
	//This can obfuscate the application's true intentions.
	//ex. \\0.0.0.0\ would make a file web request to 0.0.0.0.
	//Unix can map a folder to a webserver, but that link has to be created beforehand, not our app.
	//You can also read from hardware in a platform dependent way, which makes it harder to standardize.
	//TODO: We should however support this in the future as a custom instruction that allows it

	if (String_getAt(*result, 0) == '\\' && String_getAt(*result, 1) == '\\')
		_gotoIfError(clean, Error_unsupportedOperation(3));

	//Backslash is replaced with forward slash for easy windows compatibility

	if (!String_replaceAll(result, '\\', '/', EStringCase_Sensitive))
		_gotoIfError(clean, Error_invalidOperation(1));

	//On Windows, it's possible to change drive but keep same relative path. We don't support it.
	//e.g. C:myFolder/ (relative folder on C) instead of C:/myFolder/ (Absolute folder on C)
	//We also obviously don't support 0:\ and such or A:/ on unix

	#ifdef _WIN32

		if(result->length >= 3 && result->ptr[1] == ':' && (result->ptr[2] != '/' || !C8_isAlpha(result->ptr[0])))
			_gotoIfError(clean, Error_unsupportedOperation(2));

	#else

		if(result->length >= 2 && result->ptr[1] == ':')
			_gotoIfError(clean, Error_invalidOperation(6));

	#endif

	//Now we have to discover the real directory it references to. This means resolving:
	//Empty filename and . to mean no difference and .. to step back

	_gotoIfError(clean, String_splitx(*result, '/', EStringCase_Sensitive, &res));

	U64 realSplitLen = res.length;		//We have to reset this before unallocating the StringList!

	String back = String_createConstRefUnsafe("..");

	for (U64 i = 0; i < res.length; ++i) {

		//We pop from StringList since it doesn't change anything
		//Starting with a / is valid with local files, so don't remove it. (not with virtual files)
		//Having multiple // after each other means empty file this is invalid.
		//So both empty file and . resolve to nothing

		if(
			(String_isEmpty(res.ptr[i]) && i && !*isVirtual) ||
			String_equals(res.ptr[i], '.', EStringCase_Sensitive)
		) {

			//Move to left

			for (U64 k = res.length - 1; k > i; --k) 
				res.ptr[k - 1] = res.ptr[k];			//This is OK, we're dealing with refs from split

			--i;			//Ensure we keep track of the removed element
			--res.length;
			continue;
		}

		//In this case, we have to pop StringList[j], so that's only possible if that's still there

		if (String_equalsString(res.ptr[i], back, EStringCase_Sensitive)) {

			if(!i) {
				res.length = realSplitLen;
				_gotoIfError(clean, Error_invalidParameter(0, 0, 0));
			}

			for (U64 k = res.length - 1; k > i + 1; --k) 
				res.ptr[k - 2] = res.ptr[k];			//This is OK, we're dealing with refs from split

			i -= 2;			//Ensure we keep track of the removed element
			res.length -= 2;
			continue;
		}

		//Validate file name

		if (!String_isValidFileName(res.ptr[i], i == res.length - 1)) {

			#ifdef _WIN32

				//Drive name

				if(!i && res.ptr[i].length == 2 && C8_isAlpha(res.ptr[0].ptr[0]) && res.ptr[0].ptr[1] == ':')
					continue;

			#endif

			res.length = realSplitLen;
			_gotoIfError(clean, Error_invalidParameter(0, 0, 1));
		}

		//Validation to make sure we're not using weird legacy MS DOS keywords
		//Because these will not be writable correctly!

		if(String_equalsString(
			res.ptr[i], String_createConstRefUnsafe("CON"), EStringCase_Insensitive
		))
			_gotoIfError(clean, Error_invalidParameter(0, 0, 2));

		if(String_equalsString(
			res.ptr[i], String_createConstRefUnsafe("AUX"), EStringCase_Insensitive
		))
			_gotoIfError(clean, Error_invalidParameter(0, 0, 3));

		if(String_equalsString(
			res.ptr[i], String_createConstRefUnsafe("NUL"), EStringCase_Insensitive
		))
			_gotoIfError(clean, Error_invalidParameter(0, 0, 4));

		if(String_equalsString(
			res.ptr[i], String_createConstRefUnsafe("PRN"), EStringCase_Insensitive
		))
			_gotoIfError(clean, Error_invalidParameter(0, 0, 5));

		if(
			String_startsWithString(
				res.ptr[i], String_createConstRefUnsafe("COM"), EStringCase_Insensitive
			) &&
			res.ptr[i].length == 4 && C8_isDec(res.ptr[i].ptr[3])
		)
			_gotoIfError(clean, Error_invalidParameter(0, 0, 6));

		if(
			String_startsWithString(
				res.ptr[i], String_createConstRefUnsafe("LPT"), EStringCase_Insensitive
			) &&
			res.ptr[i].length == 4 && C8_isDec(res.ptr[i].ptr[3])
		)
			_gotoIfError(clean, Error_invalidParameter(0, 0, 7));

		//Continue processing the path until it's done

		continue;
	}

	//If we have nothing left, we get current work/app directory

	if(!res.length) {
		res.length = realSplitLen;
		goto clean;
	}

	//Re-assemble path now

	String tmp = String_createNull();

	if ((err = StringList_concatx(res, '/', &tmp)).genericError) {
		res.length = realSplitLen;
		StringList_freex(&res);
	}

	String_freex(result);		//This can't be done before concat, because the string is still in use.
	*result = tmp;

	res.length = realSplitLen;
	StringList_freex(&res);

	//Check if we're an absolute or relative path

	Bool isAbsolute = false;

	#ifdef _WIN32	//Starts with [A-Z]:/ if absolute. If it starts with / it's unsupported!

		if (String_startsWith(*result, '/', EStringCase_Sensitive))
			_gotoIfError(clean, Error_unsupportedOperation(4));

		isAbsolute = result->length >= 2 && result->ptr[1] == ':';

	#else			//Starts with / if absolute
		isAbsolute = String_startsWith(*result, '/', EStringCase_Sensitive);
	#endif

	//Our path has to be made relative to our working directory.
	//This is to avoid access from folders we shouldn't be able to read in.

	if (isAbsolute) {

		if(!String_startsWithString(
			*result, Platform_instance.workingDirectory, EStringCase_Insensitive
		))
			_gotoIfError(clean, Error_unauthorized(0));
	}

	//Prepend our path

	else _gotoIfError(clean, String_insertStringx(result, Platform_instance.workingDirectory, 0));

	//Since we're gonna use this in file operations, we wanna have a null terminator

	if(String_getAt(*result, result->length - 1) != '\0')
		_gotoIfError(clean, String_insertx(result, '\0', result->length));

	#ifdef _WIN32

		if(result->length >= 260 /* MAX_PATH */)
			_gotoIfError(clean, Error_outOfBounds(0, 0, result->length, 260));

	#endif

	return Error_none();

clean:
	StringList_freex(&res);
	String_freex(result);
	return err;
}

Error File_getInfo(String loc, FileInfo *info) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!info) 
		_gotoIfError(clean, Error_nullPointer(0, 0));

	if(info->path.ptr) 
		_gotoIfError(clean, Error_invalidOperation(0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_getInfoVirtual(loc, info));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	struct stat inf = (struct stat) { 0 };

	if (stat(resolved.ptr, &inf))
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if (!S_ISDIR(inf.st_mode) && !S_ISREG(inf.st_mode))
		_gotoIfError(clean, Error_invalidOperation(2));

	if (!(inf.st_mode & (S_IREAD | S_IWRITE)))
		_gotoIfError(clean, Error_unauthorized(0));

	*info = (FileInfo) {

		.timestamp = (Ns)inf.st_mtime * SECOND,
		.path = resolved,

		.type = S_ISDIR(inf.st_mode) ? FileType_Folder : FileType_File,
		.fileSize = (U64) inf.st_size,

		.access = 
			(inf.st_mode & S_IWRITE ? FileAccess_Write : FileAccess_None) | 
			(inf.st_mode & S_IREAD  ? FileAccess_Read  : FileAccess_None)
	};

	return Error_none();

clean:
	String_freex(&resolved);
	return err;
}

Bool File_isVirtual(String loc) { return String_getAt(loc, 0) == '/' && String_getAt(loc, 1) == '/'; }

Bool File_fileExists(String loc) { return File_existsAsType(loc, FileType_File); }
Bool File_folderExists(String loc) { return File_existsAsType(loc, FileType_Folder); }

Error File_queryFileCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_File, isRecursive, res); 
}

Error File_queryFolderCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, FileType_Folder, isRecursive, res); 
}

Bool FileInfo_free(FileInfo *info) {

	if(!info)
		return false;

	Bool freed = String_freex(&info->path);
	*info = (FileInfo) { 0 };
	return freed;
}

typedef struct FileCounter {
	FileType type;
	Bool useType;
	U64 counter;
} FileCounter;

Error countFileType(FileInfo info, FileCounter *counter) {

	if (!counter->useType) {
		++counter->counter;
		return Error_none();
	}

	if(info.type == counter->type)
		++counter->counter;

	return Error_none();
}

Error File_queryFileObjectCount(String loc, FileType type, Bool isRecursive, U64 *res) {

	Error err = Error_none();
	String resolved = String_createNull();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(3, 0));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!String_isValidFilePath(loc)) 
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	String_freex(&resolved);
	return err;
}

Error File_queryFileObjectCountAll(String loc, Bool isRecursive, U64 *res) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(2, 0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_queryFileObjectCountAllVirtual(loc, isRecursive, res));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { 0 };
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	String_freex(&resolved);
	return err;
}

Error File_add(String loc, FileType type, Ns maxTimeout) {

	String resolved = String_createNull();
	StringList str = (StringList) { 0 };
	FileInfo info = (FileInfo) { 0 };
	Error err = Error_none();

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_addVirtual(loc, type, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	err = File_getInfo(resolved, &info);

	if(err.genericError && err.genericError != EGenericError_NotFound)
		_gotoIfError(clean, err);

	//Already exists

	if(!err.genericError)
		_gotoIfError(clean, info.type != type ? Error_alreadyDefined(0) : Error_none());

	FileInfo_free(&info);

	//Check parent directories until none left

	if(String_contains(resolved, '/', EStringCase_Sensitive)) {
	
		_gotoIfError(clean, String_splitx(resolved, '/', EStringCase_Sensitive, &str));

		for (U64 i = 0; i < str.length; ++i) {

			String parent = String_createConstRefSized(resolved.ptr, String_end(str.ptr[i]) - resolved.ptr);

			err = File_getInfo(parent, &info);

			if(err.genericError && err.genericError != EGenericError_NotFound)
				_gotoIfError(clean, err);

			if(info.type != FileType_Folder)
				_gotoIfError(clean, Error_invalidOperation(2));

			FileInfo_free(&info);

			if(!err.genericError)		//Already defined, continue to child
				continue;

			if (mkdir(parent.ptr))
				_gotoIfError(clean, Error_invalidOperation(1));
		}

		StringList_freex(&str);
	}

	//Create folder

	if (type == FileType_Folder && mkdir(resolved.ptr))
		_gotoIfError(clean, Error_invalidOperation(0));

	//Create file

	if(type == FileType_File)
		_gotoIfError(clean, File_write(Buffer_createNull(), resolved, maxTimeout));

clean:
	FileInfo_free(&info);
	String_freex(&resolved);
	StringList_freex(&str);
	return err;
}

Error File_remove(String loc, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_removeVirtual(resolved, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int res = remove(resolved.ptr);

	while (res && maxTimeout) {

		Thread_sleep(maxTimeoutTry);

		res = remove(resolved.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if (res)
		_gotoIfError(clean, Error_unauthorized(0));

clean:
	String_freex(&resolved);
	return err;
}

Bool File_exists(String loc) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	FileInfo_free(&info);
	return !err.genericError;
}

Bool File_existsAsType(String loc, FileType type) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	Bool sameType = info.type == type;
	FileInfo_free(&info);
	return !err.genericError && sameType;
}

Error File_rename(String loc, String newFileName, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!String_isValidFilePath(newFileName))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!String_isValidFileName(newFileName, true))
		_gotoIfError(clean, Error_invalidParameter(1, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_renameVirtual(loc, newFileName, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	//Check if file exists

	Bool fileExists = File_exists(loc);

	if(!fileExists)
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int ren = rename(resolved.ptr, newFileName.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, newFileName.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		_gotoIfError(clean, Error_invalidState(0));

clean:
	FileInfo_free(&info);
	String_freex(&resolved);
	return err;
}

Error File_move(String loc, String directoryName, Ns maxTimeout) {

	String resolved = String_createNull(), resolvedFile = String_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!String_isValidFilePath(directoryName))
		_gotoIfError(clean, Error_invalidParameter(1, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_moveVirtual(loc, directoryName, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));
	_gotoIfError(clean, File_resolve(directoryName, &isVirtual, &resolvedFile));

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Check if file exists

	if(!File_exists(resolved))
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if(!File_folderExists(resolvedFile))
		_gotoIfError(clean, Error_notFound(0, 1, 0));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	String fileName = resolved;
	String_cutBeforeLast(fileName, '/', EStringCase_Sensitive, &fileName);

	String_setAt(resolvedFile, resolvedFile.length - 1, '/');

	_gotoIfError(clean, String_appendStringx(&resolvedFile, fileName));
	_gotoIfError(clean, String_appendx(&resolvedFile, '\0'));

	int ren = rename(resolved.ptr, resolvedFile.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, resolvedFile.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		_gotoIfError(clean, Error_invalidState(0));

clean:
	FileInfo_free(&info);
	String_freex(&resolved);
	String_freex(&resolvedFile);
	return err;
}

Error File_write(Buffer buf, String loc, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_writeVirtual(buf, loc, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	f = fopen(resolved.ptr, "wb");

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, "wb");

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		_gotoIfError(clean, Error_notFound(1, 0, 0));

	U64 bufLen = Buffer_length(buf);

	if(bufLen && fwrite(buf.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_invalidState(0));

clean:
	if(f) fclose(f);
	String_freex(&resolved);
	return err;
}

Error File_read(String loc, Ns maxTimeout, Buffer *output) {

	String resolved = String_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!output)
		_gotoIfError(clean, Error_nullPointer(2, 0));

	if(output->ptr)
		_gotoIfError(clean, Error_invalidOperation(0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_readVirtual(loc, output, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolve(loc, &isVirtual, &resolved));

	f = fopen(resolved.ptr, "rb");

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, "rb");

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if(fseek(f, 0, SEEK_END))
		_gotoIfError(clean, Error_invalidState(0));

	_gotoIfError(clean, Buffer_createUninitializedBytesx((U64)_ftelli64(f), output));

	if(fseek(f, 0, SEEK_SET))
		_gotoIfError(clean, Error_invalidState(1));

	Buffer b = *output;
	U64 bufLen = Buffer_length(b);

	if (fread(b.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_invalidState(2));

	goto success;

clean:
	Buffer_freex(output);
success:
	if(f) fclose(f);
	String_freex(&resolved);
	return err;
}
