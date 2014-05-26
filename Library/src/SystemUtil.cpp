//
//  SystemUtil.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 11/28/12.
//  Copyright (c) 2012 Patryk Cieslak. All rights reserved.
//

#include "SystemUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <Carbon/Carbon.h>
#include "SimulationApp.h"

//Precise time
uint64_t GetTimeInMicroseconds()
{
#ifdef __MACH__
    return mach_absolute_time()/1000;
#else
    unsigned ticks;
	LARGE_INTEGER count;
	static bool firstTime = true;
	static bool haveTimer = false;
	static LARGE_INTEGER frequency;
	static LARGE_INTEGER baseCount;
    
	if (firstTime) {
		firstTime = false;
		haveTimer = QueryPerformanceFrequency(&frequency) ? true : false;
		QueryPerformanceCounter (&baseCount);
	}
    
	QueryPerformanceCounter (&count);
	count.QuadPart -= baseCount.QuadPart;
	ticks = (uint64_t)(count.QuadPart * LONGLONG (1000000) / frequency.QuadPart);
	return ticks;
#endif
}

void GetCWD(char* buffer, int length)
{
#ifdef _MSC_VER
	GetCurrentDirectory(length, buffer);
#else
    getcwd(buffer, length);
#endif
}

void GetShaderPath(char* path, int len)
{
    GetCWD(path, len-64);
    strcat(path, "/");
    strcat(path, SimulationApp::getApp()->getShaderPath());
    strcat(path, "/");
}

void GetDataPath(char* path, int len)
{
    GetCWD(path, len-64);
    strcat(path, "/");
    strcat(path, SimulationApp::getApp()->getDataPath());
    strcat(path, "/");
}

const char* GetDataPathPrefix(const char* directory)
{
    static char dataPathPrefix[PATH_MAX];
    
#ifdef __APPLE__
    
    CFStringRef dir = CFStringCreateWithCString(CFAllocatorGetDefault(), directory, kCFStringEncodingMacRoman);
    
    CFURLRef datafilesURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(), dir, 0, 0);
    
    CFURLGetFileSystemRepresentation(datafilesURL, true, reinterpret_cast<UInt8*>(dataPathPrefix), PATH_MAX);
    
    if(datafilesURL != NULL)
        CFRelease(datafilesURL);
    
    CFRelease(dir);
#else
    char* envDataPath = 0;
    
    // get data path from environment var
    envDataPath = getenv(DATAPATH_VAR_NAME);
    
    // set data path prefix / base directory.  This will
    // be either from an environment variable, or from
    // a compiled in default based on original configure
    // options
    if (envDataPath != 0)
        strcpy(dataPathPrefix, envDataPath);
    else
        strcpy(dataPathPrefix, CEGUI_SAMPLE_DATAPATH);
#endif
    
    return dataPathPrefix;
}

//Extensions
GLboolean CheckForExtension(const GLchar *extensionName, const GLubyte *extensions)
{
	GLboolean extensionAvailable;
#ifdef _MSC_VER
	extensionAvailable = glewIsSupported(extensionName);
#else
	extensionAvailable = gluCheckExtension((GLubyte *)extensionName, extensions);
#endif
	return extensionAvailable;
}