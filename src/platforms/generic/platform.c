#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "types/error.h"

Platform Platform_instance = { 0 };

Error Platform_create(
	int cmdArgc, const C8 *cmdArgs[], 
	void *data, 
	FreeFunc free, AllocFunc alloc, void *allocator
) {

	U16 v = 1;

	if(!*(U8*)&v)
		return Error_unsupportedOperation(0);		//Invalid endianness

	if(Platform_instance.platformType != EPlatform_Uninitialized)
		return Error_invalidOperation(0);

	if(!cmdArgc)
		return Error_invalidParameter(0, 0, 0);

	Platform_instance =	(Platform) {
		.platformType = _PLATFORM_TYPE,
		.data = data,
		.alloc = (Allocator) {
			.free = free,
			.alloc = alloc,
			.ptr = allocator
		}
	};

	Error err = WindowManager_create(&Platform_instance.windowManager);

	if (err.genericError) {
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	StringList sl = (StringList){ 0 };

	if(cmdArgc > 1) {

		err = StringList_createx(cmdArgc - 1, &sl);

		if (err.genericError) {
			WindowManager_free(&Platform_instance.windowManager);
			Platform_instance =	(Platform) { 0 };
			return err;
		}

		//If we're passed invalid cmdArg this could be a problem
		//But that'd happen anyways

		for(int i = 1; i < cmdArgc; ++i)
			sl.ptr[i - 1] = String_createConstRefUnsafe(cmdArgs[i]);
	}

	Platform_instance.args = sl;

	if ((err = Platform_initExt(&Platform_instance, String_createConstRefUnsafe(cmdArgs[0]))).genericError) {
		StringList_freex(&sl);
		WindowManager_free(&Platform_instance.windowManager);
		Platform_instance =	(Platform) { 0 };
		return err;
	}

	return Error_none();
}

void Platform_cleanup() {

	if(Platform_instance.platformType == EPlatform_Uninitialized)
		return;

	String_freex(&Platform_instance.workingDirectory);
	WindowManager_free(&Platform_instance.windowManager);
	StringList_freex(&Platform_instance.args);

	Platform_instance =	(Platform) { 0 };
}

Bool Lock_isLocked(Lock l) {
	return l.lockThread;
}

void Log_printCapturedStackTrace(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options) {
	Log_printCapturedStackTraceCustom((const void**) stackTrace, _STACKTRACE_SIZE, lvl, options);
}
