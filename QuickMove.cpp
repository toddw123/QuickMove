#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <shlobj.h>
#include <objbase.h>
#include <time.h>
#include <shlwapi.h>
using namespace std;

#pragma comment(lib, "Shlwapi.lib")

//Function defs
int SearchDir(char *md, char* cd, bool c);
string LocateDir(char* sd, char* name, bool ext);
static inline void loadbar(unsigned int x, unsigned int n, unsigned int w = 50);

//global variable
int files = 0;
char OutputLog[260];
FILE *out = NULL;
HDC hdc;
bool test = false;

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
                                   LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred,
                                   DWORD dwStreamNumber, DWORD dwCallbackReason,
                                   HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData);

//Some guys function that is basically a SHBrowseForFolder() function wrap
BOOL GetFolderSelection(HWND hWnd, LPTSTR szBuf, LPCTSTR szTitle)
{
	LPITEMIDLIST pidl     = NULL;
	BROWSEINFO   bi       = { 0 };
	BOOL         bResult  = FALSE;

	bi.hwndOwner      = hWnd;
	bi.pszDisplayName = szBuf;
	bi.pidlRoot       = NULL;
	bi.lpszTitle      = szTitle;
	bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

	if ((pidl = SHBrowseForFolder(&bi)) != NULL)
	{
		bResult = SHGetPathFromIDList(pidl, szBuf);
		CoTaskMemFree(pidl);
	}

	return bResult;
}


int main()
{
    SetConsoleTitle("Quick Move - Created By Todd Withers");
    char StartDir[260]; //Directory we want to start at
    char MoveToDir[260]; //Directory we want to copy folders to

    time_t rawtime;
    struct tm * timeinfo;

    time (&rawtime);
    timeinfo = localtime (&rawtime);

    sprintf(OutputLog, "Output_%02i%02i-%i_%02i-%02i-%02i.log", (timeinfo->tm_mon+1), timeinfo->tm_mday, (timeinfo->tm_year+1900), timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    //printf("%s\n", OutputLog);

    out = fopen(OutputLog, "a+");
    if(out == NULL)
        return MessageBox(NULL, "Failed to create log file", "Error", MB_OK);

	int ret = MessageBox(0, "Would you like to run as a test?", "", MB_YESNO);
	if(ret == IDYES)
		test = true;
	else
		test = false;

    CoInitialize(NULL); //stupid crap that has to be run to use the folder dialog

    if(!GetFolderSelection(NULL, StartDir, TEXT("Select Master Staging Folder")))
    {
        //basically the only reason you would reach this section is if the person hit "cancle" or didnt select a folder.
        return MessageBox(NULL, "No folder or invalid folder selected", "Error", MB_OK|MB_ICONERROR);
    }
    if(!GetFolderSelection(NULL, MoveToDir, TEXT("Select Destination Folder")))
    {
        //basically the only reason you would reach this section is if the person hit "cancle" or didnt select a folder.
        return MessageBox(NULL, "No folder or invalid folder selected", "Error", MB_OK|MB_ICONERROR);
    }

    //So we have to append the string inorder for it to be a search. Just like in windows search, the * means wildcard
    char searcharg[260];
    sprintf(searcharg, "%s\\*", StartDir);

    //Calls my function, but only wants to find the number of indexed files
    files = SearchDir(StartDir, MoveToDir, false);
    if(files <= 1)
    {
        return MessageBox(NULL, "The selected folder doesnt contain any files to move", "Error", MB_OK|MB_ICONERROR);
    }

    //File I/O
    fprintf(out, "-------------------New Instance-------------------\nMaster directory: %s\nDestination directory: %s\nFiles indexed: %i\n", StartDir, MoveToDir, files);
    fflush(out);

    //Calls my function again, but this time i tell it to go ahead and copy the files
    int copyed = SearchDir(StartDir, MoveToDir, true);

    //More File I/O
    fprintf(out, "Finished. Moved %i files.\n", copyed);
    fclose(out);

    CoUninitialize();

    MessageBox(NULL, "Finished Moving Files", "Operation Completed", MB_OK);

    return 0;
}

//Function i made real quick.
//Main API's used are FindFirstFile, FindNextFile
int SearchDir(char *md, char* cd, bool c)
{
    int res = 0;
    char searcharg[260];
    sprintf(searcharg, "%s\\*", md);

    char* folder = PathFindFileName(md);
    //return MessageBox(NULL, folder, folder, MB_OK);

    WIN32_FIND_DATA ffd;
    HANDLE hFind;

    hFind = FindFirstFile(searcharg, &ffd);
    if(hFind == INVALID_HANDLE_VALUE) return 0;

    do
    {
        if(strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
        {
            if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && strcmp(ffd.cFileName, folder) != 0)
            {
                //Folder
                char path[260]; //Will store the contents of the folder name
                sprintf(path, "%s\\%s\\*", md, ffd.cFileName);

                string CopyTo = "";
                if(c) CopyTo = LocateDir(cd, ffd.cFileName, false);

                WIN32_FIND_DATA tmpffd;
                HANDLE hTemp = FindFirstFile(path, &tmpffd);
                do
                {
                    if(strcmp(tmpffd.cFileName, ".") != 0 && strcmp(tmpffd.cFileName, "..") != 0)
                    {
                        //File that needs to be copied
                        res++;
                        if(c)
                        {
                            if(CopyTo != "")
                            {
                                char ExistingFileName[260];
                                char NewFileName[260];
                                char lpData[280];

                                sprintf(ExistingFileName, "%s\\%s\\%s", md, ffd.cFileName, tmpffd.cFileName);
                                sprintf(NewFileName, "%s\\%s", CopyTo.c_str(), tmpffd.cFileName);

                                char lpStr1[260];
                                strcpy(lpStr1,ExistingFileName);
                                char lpStr2[260];
                                strcpy(lpStr2, NewFileName);
                                PathCompactPath(hdc,lpStr1,125);
                                PathCompactPath(hdc,lpStr2,125);

                                sprintf(lpData, "Moving %s -> %s", lpStr1, lpStr2);

								if(test)
								{
									fprintf(out, "--<moved> %s -> %s\n", ExistingFileName, NewFileName);
									loadbar(res, files, 50);
								}
								else
								{
									if(CopyFileEx(ExistingFileName, NewFileName, (LPPROGRESS_ROUTINE)CopyProgressRoutine, (LPVOID)lpData, false, NULL))
										fprintf(out, "--<moved> %s -> %s\n", ExistingFileName, NewFileName);
									else
										fprintf(out, "--<error %d> failed to move file %s -> %s\n", GetLastError(), ExistingFileName, NewFileName);
									printf("\n");
								}
                                fflush(out);

                                //loadbar(res, files, 50);
                            }
                        }
                    }
                }
                while (FindNextFile(hTemp, &tmpffd) != 0);
                FindClose(hTemp);

            }
            else
            {

				string CopyTo = "";
                if(c) CopyTo = LocateDir(cd, ffd.cFileName, true);

                //File
                res++;
                if(c)
                {
					if(CopyTo != "")
					{
						char ExistingFileName[260];
						char NewFileName[260];
						char lpData[300];

						sprintf(ExistingFileName, "%s\\%s", md, ffd.cFileName);
						sprintf(NewFileName, "%s\\%s", CopyTo.c_str(), ffd.cFileName);

						char lpStr1[260];
						strcpy(lpStr1,ExistingFileName);
						char lpStr2[260];
						strcpy(lpStr2, NewFileName);

						PathCompactPath(hdc,lpStr1,125);
						PathCompactPath(hdc,lpStr2,125);

						sprintf(lpData, "Moving %s -> %s", lpStr1, lpStr2);
					
						if(test)
						{
							fprintf(out, "--<moved> %s -> %s\n", ExistingFileName, NewFileName);
							loadbar(res, files, 50);
						}
						else
						{
							if(CopyFileEx(ExistingFileName, NewFileName, (LPPROGRESS_ROUTINE)CopyProgressRoutine, (LPVOID)lpData, false, NULL))
								fprintf(out, "--<moved> %s -> %s\n", ExistingFileName, NewFileName);
							else
								fprintf(out, "--<error %d> failed to move file %s -> %s\n", GetLastError(), ExistingFileName, NewFileName);
							printf("\n");
						}
						fflush(out);
						//loadbar(res, files, 50);
					}
					else
					{
						fprintf(out, "Failed to located the directory for file %s\n", ffd.cFileName);
						fflush(out);
					}
                }
            }
        }
   }
   while (FindNextFile(hFind, &ffd) != 0);


   FindClose(hFind);

   return res;
}

//This function i made to search the destination directory for the corrisponding folders
string LocateDir(char* sd, char* name, bool ext)
{
    char searcharg[260];
    sprintf(searcharg, "%s\\*", sd);

    WIN32_FIND_DATA ffd;
    HANDLE hFind;

    hFind = FindFirstFile(searcharg, &ffd);
    if(hFind == INVALID_HANDLE_VALUE) return "";
    do
    {
        if(strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
        {
            if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
				int length = 0;
				if(ext)
				{
					for(length = 0; length < strlen(name); length++)
						if(name[length] == '_') break;
				}
				else
				{
					length = strlen(name);
				}
                if(memcmp(ffd.cFileName, name, length) == 0)
                {
                    //Found folder we want to copy too
                    string path = sd;
                    path += "\\";
                    path += ffd.cFileName;

                    fprintf(out, "Emptying Folder: %s\n", path.c_str());
                    fflush(out);

                    char path2[260]; //Will store the contents of the folder name
                    sprintf(path2, "%s\\%s\\*", sd, ffd.cFileName);

                    WIN32_FIND_DATA tmpffd;
                    HANDLE hTemp = FindFirstFile(path2, &tmpffd);
                    do
                    {
                        if(strcmp(tmpffd.cFileName, ".") != 0 && strcmp(tmpffd.cFileName, "..") != 0)
                        {
                            char path3[260]; //Will store the contents of the folder name
                            sprintf(path3, "%s\\%s\\%s", sd, ffd.cFileName, tmpffd.cFileName);

							if(test)
							{
								fprintf(out, "--<deleted> %s\n", path3);
							}
							else
							{
								if(DeleteFile(path3))
									fprintf(out, "--<deleted> %s\n", path3);
								else
									fprintf(out, "--<error> failed to delete file %s", path3);
							}
                            fflush(out);
                            //DeleteFile();
                        }
                    }
                    while (FindNextFile(hTemp, &tmpffd) != 0);
                    FindClose(hTemp);

                    return path;
                }
            }
        }
   }
   while (FindNextFile(hFind, &ffd) != 0);


   FindClose(hFind);

   fprintf(out, "No corresponding folder found for: %s\n", name);

   return "";
}

static inline void loadbar(unsigned int x, unsigned int n, unsigned int w)
{
    if ( (x != n) && (x % (n/100) != 0) ) return;

    float ratio  =  x/(float)n;
    int   c      =  ratio * w;

    cout << setw(3) << (int)(ratio*100) << "% [";
    for (int x=0; x<c; x++) cout << "=";
    for (int x=c; x<w; x++) cout << " ";
    cout << "]\r" << flush;
}

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
                                   LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred,
                                   DWORD dwStreamNumber, DWORD dwCallbackReason,
                                   HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData)
{
    char *tmp = (char*)lpData;
    printf("\r%s...%i%%", tmp, (TotalBytesTransferred.LowPart / TotalFileSize.LowPart * 100));
    return 0;
}
