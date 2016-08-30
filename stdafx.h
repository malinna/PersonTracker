#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#endif

#include <windows.h>
#include <Shlobj.h>
#include <d2d1.h>
#include <Kinect.h>
#include <iostream>
#include <deque>
#include <string>
#include <codecvt>
#include <ctime>
#include <iomanip>
#include <cstdio>

#pragma comment (lib, "d2d1.lib")

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

// Safe release for interfaces.

template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
    if (pInterfaceToRelease != NULL)
    {
        pInterfaceToRelease->Release();
        pInterfaceToRelease = NULL;
    }
}

// String conversion.

inline std::wstring s2ws(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converterX;
	return converterX.from_bytes(str);
}

inline std::string ws2s(const std::wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converterX;
	return converterX.to_bytes(wstr);
}

// Disk operations.

inline std::deque<std::string> GetFileList(const std::string& folder, const std::string& wildcard)
{
	std::deque<std::string> names;
	std::wstring searchPath = s2ws(folder + '\\' + wildcard);
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(searchPath.c_str(), &fd);

	if (hFind != INVALID_HANDLE_VALUE) 
	{
		do 
		{
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
			{
				names.push_back(ws2s(fd.cFileName));
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}

	return names;
}

inline std::deque<std::string> GetDirList(const std::string& folder, const std::string& wildcard)
{
	std::deque<std::string> names;
	std::wstring searchPath = s2ws(folder + '\\' + wildcard);
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(searchPath.c_str(), &fd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				names.push_back(ws2s(fd.cFileName));
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}

	return names;
}

inline bool DirExists(const std::string& dirName)
{
	DWORD fa = GetFileAttributesA(dirName.c_str());
	
	if (fa != INVALID_FILE_ATTRIBUTES && 
		fa & FILE_ATTRIBUTE_DIRECTORY)
	{
		return true;
	}

	return false;
}

inline int DeleteDir(const std::string &refcstrRootDirectory,	bool bDeleteSubdirectories = true)
{
	bool bSubdirectory = false;

	std::wstring strPattern = s2ws(refcstrRootDirectory + "\\*.*");

	WIN32_FIND_DATA FileInformation;
	HANDLE hFile = ::FindFirstFile(strPattern.c_str(), &FileInformation);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FileInformation.cFileName[0] != '.')
			{
				std::wstring strFilePath = s2ws(refcstrRootDirectory + "\\") + FileInformation.cFileName;

				if (FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (bDeleteSubdirectories)
					{
						int iRC = DeleteDir(ws2s(strFilePath), bDeleteSubdirectories);
						if (iRC)
						{
							return iRC;
						}
					}
					else
						bSubdirectory = true;
				}
				else
				{
					if (::SetFileAttributes(strFilePath.c_str(),
						FILE_ATTRIBUTE_NORMAL) == FALSE)
					{
						return ::GetLastError();
					}

					if (::DeleteFile(strFilePath.c_str()) == FALSE)
					{
						return ::GetLastError();
					}
				}
			}
		} while (::FindNextFile(hFile, &FileInformation) == TRUE);

		// Close handle
		::FindClose(hFile);

		DWORD dwError = ::GetLastError();
		if (dwError != ERROR_NO_MORE_FILES)
			return dwError;
		else
		{
			if (!bSubdirectory)
			{
				// Set directory attributes
				if (::SetFileAttributes(s2ws(refcstrRootDirectory).c_str(),
					FILE_ATTRIBUTE_NORMAL) == FALSE)
					return ::GetLastError();

				// Delete directory
				if (::RemoveDirectory(s2ws(refcstrRootDirectory).c_str()) == FALSE)
					return ::GetLastError();
			}
		}
	}

	return 0;
}

inline std::wstring GetUserDocumentsPath()
{
	WCHAR path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, path)))
	{
		return std::wstring(path);
	}

	return std::wstring();
}

/// Convert body point to screen space.

inline D2D1_POINT_2F BodyToScreen(const CameraSpacePoint& bodyPoint, ICoordinateMapper* pCoordinateMapper)
{
	ColorSpacePoint colorPoint = { 0 };
	pCoordinateMapper->MapCameraPointToColorSpace(bodyPoint, &colorPoint);

	return D2D1::Point2F(colorPoint.X, colorPoint.Y);
}
