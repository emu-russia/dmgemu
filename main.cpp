#include "pch.h"

#define MAX_NUM_ARGVS 128
int argc;
char *argv[MAX_NUM_ARGVS];

void ParseCommandLine(LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}


char *Open_File_Proc()
{
	char prevc[256];
	OPENFILENAME ofn;
	char szFileName[120];
	char szFileTitle[120]; 
	static char file_to_load[256];

	_getcwd(prevc, 255);

	memset(&szFileName,0,sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= NULL;
	ofn.lpstrFilter			= "GameBoy ROM\0*.gb;sc*.??;sl*.??\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= 120;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= 120;
	ofn.lpstrTitle			= "Open GameBoy ROM\0";
	ofn.lpstrDefExt			= "GB";
	ofn.Flags				= OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
	 
	if (GetOpenFileName ((LPOPENFILENAME)&ofn))
	{
		strcpy(file_to_load, szFileName);
		chdir(prevc);
		return file_to_load;
	}
	else
	{
		chdir(prevc);
		return NULL;
	}
}

/* platform initialization code */
void plat_init()
{
	char *name;

	rand_init();
	TimerInit();

	if(argc <= 1)
	{
		name = Open_File_Proc();
		if(name) load_game(name);
		//else exit(1);
	}
	else
	{
		if(argv[1][0]=='"'){
			argv[1]++;
			argv[1][strlen(argv[1]) - 1] = 0;
		}
		load_game(argv[1]);
	}
}

/* platform Entry-point */
PLAT int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	ParseCommandLine(lpCmdLine);
	plat_init();

	gb_init();
	start();
}
