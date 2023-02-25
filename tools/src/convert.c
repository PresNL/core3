/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/time.h"
#include "types/file.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/file.h"
#include "cli.h"

Bool _CLI_convert(ParsedArgs args, Bool isTo) {

	Ns start = Time_now();

	//Prepare for convert to/from

	Format f = Format_values[args.format];
	U64 offset = 0;

	Buffer inputArgBuf;
	Error err = List_get(args.args, offset++, &inputArgBuf);

	if (err.genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	String inputArg = *(const String*)inputArgBuf.ptr;

	//Check if output is valid

	Buffer outputArgBuf;

	if ((err = List_get(args.args, offset++, &outputArgBuf)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	String outputArg = *(const String*)outputArgBuf.ptr;

	//TODO: Support multiple files

	//Check if input is valid

	//Check if input file and file type are valid

	FileInfo info = (FileInfo) { 0 };

	if ((err = File_getInfo(inputArg, &info)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	if (!(f.flags & EFormatFlags_SupportFiles) && info.type == EFileType_File) {
		Log_errorLn("Invalid file passed to convertTo. Only accepting folders.");
		return false;
	}

	if (!(f.flags & EFormatFlags_SupportFolders) && info.type == EFileType_Folder) {
		Log_errorLn("Invalid file passed to convertTo. Only accepting files.");
		return false;
	}

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_AES) {

		String key = String_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError || 
			!String_isHex(key)
		) {
			Log_errorLn("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = String_startsWithString(key, String_createConstRefUnsafe("0x"), EStringCase_Insensitive) ? 2 : 0;

		if (String_length(key) - off != 64) {
			Log_errorLn("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < String_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	//Now convert it

	switch (args.format) {

		case EFormat_oiDL: 

			if(isTo)
				err = _CLI_convertToDL(args, inputArg, info, outputArg, encryptionKey);

			else err = _CLI_convertFromDL(args, inputArg, info, outputArg, encryptionKey);

			break;

		case EFormat_oiCA: 

			if(isTo)
				err = _CLI_convertToCA(args, inputArg, info, outputArg, encryptionKey);

			else err = _CLI_convertFromCA(args, inputArg, info, outputArg, encryptionKey);

			break;
		
		default:
			Log_debugLn("Unsupported format");
			return false;
	}

	if (err.genericError) {
		Log_errorLn("File conversion failed!");
		Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
		return false;
	}

	//Tell CLI users

	Log_debugLn("Converted file oiXX format in %ums", (Time_now() - start + MS - 1) / MS);
	return true;
}

Bool CLI_convertTo(ParsedArgs args) {
	return _CLI_convert(args, true);
}

Bool CLI_convertFrom(ParsedArgs args) {
	return _CLI_convert(args, false);
}
